import 'dart:convert';
import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:http/http.dart' as http;

import 'catalog_database.dart';

/// Where the catalog is published.
///
/// `releases/latest/download/<asset>` is a permanent redirect maintained by
/// GitHub to the newest published release, so the app never has to call the
/// API, never needs a token, and never runs into the unauthenticated rate
/// limit — which matters for an app that checks once a day on every device.
const defaultCatalogManifestUrl =
    'https://github.com/edikdr/Vibe-stuck-/releases/latest/download/manifest.json';

/// What a release publishes next to the platform builds.
class CatalogManifest {
  const CatalogManifest({
    required this.schemaVersion,
    required this.catalogVersion,
    required this.verifiedAt,
    required this.itemCount,
    required this.file,
    required this.url,
    required this.size,
    required this.sha256,
    required this.databaseSize,
    required this.databaseSha256,
    required this.minAppVersion,
    required this.signature,
  });

  final int schemaVersion;
  final String catalogVersion;
  final String verifiedAt;
  final int itemCount;

  /// Asset name of the compressed database in the same release.
  final String file;

  /// Absolute download URL, when the release recorded one.
  final String url;
  final int size;
  final String sha256;
  final int databaseSize;
  final String databaseSha256;
  final String minAppVersion;

  /// Reserved for a detached signature. Nothing verifies it yet; the download
  /// is authenticated by HTTPS to github.com and checked against [sha256].
  final String signature;

  factory CatalogManifest.fromJson(Map<String, dynamic> json) => CatalogManifest(
        schemaVersion: (json['schemaVersion'] as num?)?.toInt() ?? 0,
        catalogVersion: json['catalogVersion'] as String? ?? '',
        verifiedAt: json['verifiedAt'] as String? ?? '',
        itemCount: (json['itemCount'] as num?)?.toInt() ?? 0,
        file: json['file'] as String? ?? 'catalog.db.gz',
        url: json['url'] as String? ?? '',
        size: (json['size'] as num?)?.toInt() ?? 0,
        sha256: json['sha256'] as String? ?? '',
        databaseSize: (json['databaseSize'] as num?)?.toInt() ?? 0,
        databaseSha256: json['databaseSha256'] as String? ?? '',
        minAppVersion: json['minAppVersion'] as String? ?? '',
        signature: json['signature'] as String? ?? '',
      );
}

/// Why a check did not end in an installed update.
enum CatalogUpdateStatus {
  /// A newer catalog was downloaded, verified and installed.
  installed,

  /// The published catalog is the one already installed.
  upToDate,

  /// The published catalog needs a newer build of the app.
  appTooOld,

  /// Nothing was installed because something went wrong; see `message`.
  failed,
}

class CatalogUpdateResult {
  const CatalogUpdateResult(this.status, {this.version = '', this.message = ''});

  final CatalogUpdateStatus status;
  final String version;
  final String message;

  bool get changed => status == CatalogUpdateStatus.installed;
}

/// Downloads catalog releases and installs them over the local database.
///
/// Every step is checked before the previous one is thrown away: the manifest
/// decides whether to download at all, the archive is verified against the
/// hash in the manifest, the decompressed file is verified again, and it must
/// open as a readable catalog before it is allowed to replace anything.
class CatalogUpdateService {
  CatalogUpdateService({
    required CatalogDatabase database,
    http.Client? client,
    this.manifestUrl = defaultCatalogManifestUrl,
    this.timeout = const Duration(seconds: 30),
  })  : _database = database,
        _client = client ?? http.Client();

  final CatalogDatabase _database;
  final http.Client _client;
  final String manifestUrl;
  final Duration timeout;

  /// Largest archive the app will download. A release asset far bigger than
  /// the catalog has ever been is a mistake or a mirror gone wrong, not an
  /// update worth streaming onto a phone.
  static const maxArchiveBytes = 64 * 1024 * 1024;

  Future<CatalogManifest> fetchManifest() async {
    final uri = Uri.parse(manifestUrl);
    if (uri.scheme != 'https') {
      throw const FormatException('catalog manifest must be served over https');
    }
    final response = await _client.get(uri).timeout(timeout);
    if (response.statusCode != 200) {
      throw HttpException('manifest returned HTTP ${response.statusCode}', uri: uri);
    }
    return CatalogManifest.fromJson(
        jsonDecode(utf8.decode(response.bodyBytes)) as Map<String, dynamic>);
  }

  /// Checks for a newer catalog and installs it if there is one.
  ///
  /// [appVersion] is the running build, compared against the manifest's
  /// `minAppVersion`: an app that cannot read the new schema should say so
  /// rather than download 800 KB it will have to throw away.
  Future<CatalogUpdateResult> update({
    required String appVersion,
    bool force = false,
  }) async {
    // The headless runner reaches this before anything has opened the catalog,
    // and the installed version is what decides whether to download at all.
    await _database.ensureOpen();

    final CatalogManifest manifest;
    try {
      manifest = await fetchManifest();
    } on Object catch (error) {
      return CatalogUpdateResult(CatalogUpdateStatus.failed,
          message: 'manifest: ${error.runtimeType}');
    }

    if (manifest.schemaVersion != CatalogDatabase.supportedSchemaVersion ||
        _compare(appVersion, manifest.minAppVersion) < 0) {
      return CatalogUpdateResult(CatalogUpdateStatus.appTooOld,
          version: manifest.catalogVersion,
          message: 'catalog ${manifest.catalogVersion} needs app '
              '${manifest.minAppVersion} or newer');
    }

    final installed = _database.metadata.catalogVersion;
    if (!force &&
        CatalogDatabase.compareVersions(manifest.catalogVersion, installed) <= 0) {
      return CatalogUpdateResult(CatalogUpdateStatus.upToDate, version: installed);
    }

    try {
      await _downloadAndInstall(manifest);
    } on Object catch (error) {
      return CatalogUpdateResult(CatalogUpdateStatus.failed,
          version: installed, message: '${error.runtimeType}: $error');
    }
    return CatalogUpdateResult(CatalogUpdateStatus.installed,
        version: manifest.catalogVersion);
  }

  Future<void> _downloadAndInstall(CatalogManifest manifest) async {
    final uri = Uri.parse(manifest.url.isNotEmpty
        ? manifest.url
        : _siblingOf(manifestUrl, manifest.file));
    if (uri.scheme != 'https') {
      throw const FormatException('catalog archive must be served over https');
    }

    final response = await _client.get(uri).timeout(timeout);
    if (response.statusCode != 200) {
      throw HttpException('archive returned HTTP ${response.statusCode}', uri: uri);
    }
    final archive = response.bodyBytes;
    if (archive.length > maxArchiveBytes) {
      throw const FormatException('catalog archive is implausibly large');
    }
    if (manifest.size > 0 && archive.length != manifest.size) {
      throw FormatException(
          'archive is ${archive.length} bytes, manifest says ${manifest.size}');
    }
    _requireDigest(archive, manifest.sha256, 'archive');

    final decoded = gzip.decode(archive);
    if (manifest.databaseSize > 0 && decoded.length != manifest.databaseSize) {
      throw FormatException(
          'database is ${decoded.length} bytes, manifest says ${manifest.databaseSize}');
    }
    _requireDigest(decoded, manifest.databaseSha256, 'database');

    final staging = File(_database.stagingPathFor(await _database.resolvePath()));
    await staging.writeAsBytes(decoded, flush: true);
    try {
      // Opens the file and reads its meta table; a database that cannot answer
      // that is never swapped in.
      final incoming = CatalogDatabase.readMetadataAt(staging.path);
      if (incoming.catalogVersion != manifest.catalogVersion) {
        throw FormatException(
            'downloaded catalog is ${incoming.catalogVersion}, '
            'manifest promised ${manifest.catalogVersion}');
      }
      await _database.install(staging);
    } on Object {
      if (await staging.exists()) await staging.delete();
      rethrow;
    }
  }

  void _requireDigest(List<int> bytes, String expected, String what) {
    if (expected.isEmpty) {
      throw FormatException('manifest carries no $what checksum');
    }
    final actual = sha256.convert(bytes).toString();
    if (actual != expected.toLowerCase()) {
      throw FormatException('$what checksum mismatch: got $actual');
    }
  }

  /// Release assets sit next to each other, so the archive URL is the manifest
  /// URL with the last path segment swapped.
  static String _siblingOf(String manifest, String file) {
    final uri = Uri.parse(manifest);
    final segments = [...uri.pathSegments];
    if (segments.isEmpty) return manifest;
    segments[segments.length - 1] = file;
    return uri.replace(pathSegments: segments).toString();
  }

  /// Compares dotted versions such as `0.7.0`, ignoring any `+build` suffix.
  static int _compare(String left, String right) {
    List<int> parts(String value) => value
        .split('+')
        .first
        .split('.')
        .map((part) => int.tryParse(part.trim()) ?? 0)
        .toList();
    final a = parts(left);
    final b = parts(right);
    for (var i = 0; i < (a.length > b.length ? a.length : b.length); i++) {
      final x = i < a.length ? a[i] : 0;
      final y = i < b.length ? b[i] : 0;
      if (x != y) return x.compareTo(y);
    }
    return 0;
  }

  void dispose() => _client.close();
}
