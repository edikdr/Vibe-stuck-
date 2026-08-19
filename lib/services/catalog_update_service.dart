import 'dart:convert';
import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:cryptography/cryptography.dart';
import 'package:http/http.dart' as http;

import '../app_info.dart';
import 'catalog_database.dart';

/// Where the catalog is published, in the order they are tried.
///
/// `releases/latest/download/<asset>` is a permanent redirect maintained by
/// the forge to the newest published release, so the app never has to call an
/// API, never needs a token, and never runs into an unauthenticated rate
/// limit — which matters for an app that checks once a day on every device.
///
/// A list rather than a constant because a release can move and a host can be
/// unreachable from where the user is. Add a mirror by adding its
/// `manifest.json` here; the archive is always fetched from whichever host
/// answered, so the two can never be mixed. A mirror that publishes no release
/// does not belong here — it would only add a failing request to every check.
const defaultCatalogManifestUrls = <String>[
  'https://github.com/edikdr/Vibe-stuck-/releases/latest/download/manifest.json',
];

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

  /// Version of the signed payload format, not of the manifest.
  static const signaturePayloadVersion = 'vibestack-atlas-catalog-v1';
  static const signaturePrefix = 'ed25519:';

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

  /// Detached Ed25519 signature over [signingPayload], as `ed25519:<base64>`.
  ///
  /// Empty when the project has not switched signing on. Whether an empty
  /// signature is acceptable is decided by the app, not by the manifest: see
  /// [CatalogUpdateService._verifySignature].
  final String signature;

  /// The exact bytes a signature covers.
  ///
  /// Built from named fields rather than from the manifest's own JSON, so that
  /// adding a field to a future manifest cannot invalidate the signature for
  /// every app already installed. Kept identical to `signing_payload()` in
  /// tool/catalog/signing.py — the two are one format described twice.
  List<int> get signingPayload => utf8.encode([
        signaturePayloadVersion,
        '$schemaVersion',
        catalogVersion,
        '$size',
        sha256,
        '$databaseSize',
        databaseSha256,
      ].join('\n'));

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

/// A manifest together with the host that served it.
class _FetchedManifest {
  const _FetchedManifest(this.manifest, this.source);
  final CatalogManifest manifest;
  final String source;
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
/// decides whether to download at all, its signature decides whether to trust
/// it, the archive is verified against the hash it names, the decompressed
/// file is verified again, and it must open as a readable catalog before it is
/// allowed to replace anything.
class CatalogUpdateService {
  CatalogUpdateService({
    required CatalogDatabase database,
    http.Client? client,
    List<String>? manifestUrls,
    this.publicKey = releasePublicKey,
    this.timeout = const Duration(seconds: 30),
  })  : _database = database,
        _client = client ?? http.Client(),
        manifestUrls = manifestUrls ?? defaultCatalogManifestUrls;

  final CatalogDatabase _database;
  final http.Client _client;

  /// Mirrors to try, in order.
  final List<String> manifestUrls;

  /// Ed25519 public key releases must be signed with, base64, or empty.
  ///
  /// Empty means signing is not switched on: downloads are still verified
  /// against the SHA-256 in the manifest, which is itself fetched over HTTPS.
  /// Non-empty makes a signature mandatory — this is what makes it worth
  /// having, since a signature the client would accept the absence of stops
  /// nobody.
  final String publicKey;

  final Duration timeout;

  /// Largest archive the app will download. A release asset far bigger than
  /// the catalog has ever been is a mistake or a mirror gone wrong, not an
  /// update worth streaming onto a phone.
  static const maxArchiveBytes = 64 * 1024 * 1024;

  /// Fetches the manifest from the first mirror that answers.
  ///
  /// Failing over matters more than it looks: the forge can be unreachable
  /// from where the user is, and an offline-first catalog that cannot update
  /// because one host is blocked is exactly the failure it exists to avoid.
  Future<_FetchedManifest> _fetchManifest() async {
    if (manifestUrls.isEmpty) {
      throw const FormatException('no catalog manifest URL is configured');
    }
    Object? lastError;
    for (final candidate in manifestUrls) {
      try {
        final uri = Uri.parse(candidate);
        if (uri.scheme != 'https') {
          throw const FormatException('catalog manifest must be served over https');
        }
        final response = await _client.get(uri).timeout(timeout);
        if (response.statusCode != 200) {
          throw HttpException('manifest returned HTTP ${response.statusCode}', uri: uri);
        }
        final manifest = CatalogManifest.fromJson(
            jsonDecode(utf8.decode(response.bodyBytes)) as Map<String, dynamic>);
        return _FetchedManifest(manifest, candidate);
      } on Object catch (error) {
        lastError = error;
      }
    }
    throw lastError!;
  }

  /// Public entry point for callers that only want to look.
  Future<CatalogManifest> fetchManifest() async => (await _fetchManifest()).manifest;

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

    final _FetchedManifest fetched;
    try {
      fetched = await _fetchManifest();
    } on Object catch (error) {
      return CatalogUpdateResult(CatalogUpdateStatus.failed,
          message: 'manifest: ${error.runtimeType}');
    }
    final manifest = fetched.manifest;

    if (manifest.schemaVersion != CatalogDatabase.supportedSchemaVersion ||
        _compare(appVersion, manifest.minAppVersion) < 0) {
      return CatalogUpdateResult(CatalogUpdateStatus.appTooOld,
          version: manifest.catalogVersion,
          message: 'catalog ${manifest.catalogVersion} needs app '
              '${manifest.minAppVersion} or newer');
    }

    final installed = _database.metadata.catalogVersion;

    // Before the version comparison: an unsigned manifest is not evidence of
    // anything, including of being up to date.
    try {
      await _verifySignature(manifest);
    } on Object catch (error) {
      return CatalogUpdateResult(CatalogUpdateStatus.failed,
          version: installed, message: '$error');
    }

    if (!force &&
        CatalogDatabase.compareVersions(manifest.catalogVersion, installed) <= 0) {
      return CatalogUpdateResult(CatalogUpdateStatus.upToDate, version: installed);
    }

    try {
      await _downloadAndInstall(manifest, fetched.source);
    } on Object catch (error) {
      return CatalogUpdateResult(CatalogUpdateStatus.failed,
          version: installed, message: '${error.runtimeType}: $error');
    }
    return CatalogUpdateResult(CatalogUpdateStatus.installed,
        version: manifest.catalogVersion);
  }

  /// Throws unless the manifest is signed by [publicKey].
  ///
  /// Does nothing when no key is compiled in, which is the state of a project
  /// that has not generated one yet.
  Future<void> _verifySignature(CatalogManifest manifest) async {
    if (publicKey.isEmpty) return;
    if (manifest.signature.isEmpty) {
      throw const FormatException('catalog is unsigned and this build requires a signature');
    }
    if (!manifest.signature.startsWith(CatalogManifest.signaturePrefix)) {
      throw const FormatException('catalog signature is not ed25519');
    }

    final List<int> signatureBytes;
    final List<int> keyBytes;
    try {
      signatureBytes = base64.decode(
          manifest.signature.substring(CatalogManifest.signaturePrefix.length));
      keyBytes = base64.decode(publicKey);
    } on FormatException {
      throw const FormatException('catalog signature or key is not valid base64');
    }

    final verified = await Ed25519().verify(
      manifest.signingPayload,
      signature: Signature(
        signatureBytes,
        publicKey: SimplePublicKey(keyBytes, type: KeyPairType.ed25519),
      ),
    );
    if (!verified) {
      throw const FormatException('catalog signature does not match the release key');
    }
  }

  Future<void> _downloadAndInstall(CatalogManifest manifest, String source) async {
    final uri = Uri.parse(manifest.url.isNotEmpty
        ? manifest.url
        : _siblingOf(source, manifest.file));
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
  /// URL with the last path segment swapped. Derived from the mirror that
  /// actually answered, so a fallback never mixes two hosts.
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
