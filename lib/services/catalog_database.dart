import 'dart:io';

import 'package:flutter/services.dart';
import 'package:path/path.dart' as p;
import 'package:path_provider/path_provider.dart';
import 'package:sqlite3/sqlite3.dart';

import '../app_info.dart';
import '../models/catalog_item.dart';

/// The catalog as it lives on the device: one SQLite file, read-only for the
/// app, replaced wholesale by an update.
///
/// Nothing the user owns is stored here. Favourites, custom entries and
/// settings stay in shared preferences precisely because this file is thrown
/// away and rewritten whenever a newer catalog arrives.
class CatalogDatabase {
  CatalogDatabase({
    this.assetKey = 'assets/catalog.db',
    this.fileName = 'catalog.db',
    Future<Directory> Function()? directory,
  }) : _directory = directory ?? getApplicationSupportDirectory;

  /// Schema this build knows how to read. A database built for any other
  /// version is rejected rather than half-read.
  static const supportedSchemaVersion = bundledCatalogSchema;

  final String assetKey;
  final String fileName;
  final Future<Directory> Function() _directory;

  Database? _handle;
  String? _path;

  bool get isOpen => _handle != null;

  /// Absolute path of the installed database, once [open] has run.
  String? get path => _path;

  /// Where an update writes before it is allowed to replace the live file.
  String stagingPathFor(String databasePath) => '$databasePath.incoming';

  Future<String> resolvePath() async {
    final directory = await _directory();
    return p.join(directory.path, fileName);
  }

  /// Opens the installed catalog, seeding it from the bundled asset the first
  /// time, or whenever the bundled copy is newer than the installed one.
  ///
  /// A user who installs a new build of the app should not have to wait for a
  /// download to see the catalog that build shipped with.
  Future<void> open() async {
    final target = File(await resolvePath());
    await target.parent.create(recursive: true);

    if (!await target.exists() || _bundledIsNewer(target.path)) {
      await _seedFromAsset(target);
    }

    _handle?.dispose();
    try {
      _handle = sqlite3.open(target.path, mode: OpenMode.readOnly);
      _readVersion(_handle!);
    } on Object {
      // A corrupt file is not worth diagnosing on a phone: the bundled asset
      // is always a valid catalog, so fall back to it and carry on.
      _handle?.dispose();
      _handle = null;
      await _seedFromAsset(target);
      _handle = sqlite3.open(target.path, mode: OpenMode.readOnly);
    }
    _path = target.path;
  }

  /// Opens a catalog file directly, skipping the asset-seeding path.
  ///
  /// Used by tests, which have no plugin channel to ask for an application
  /// directory, and by anything that needs to read a specific file rather than
  /// the installed one.
  void openFile(String databasePath) {
    _handle?.dispose();
    _handle = sqlite3.open(databasePath, mode: OpenMode.readOnly);
    _readVersion(_handle!);
    _path = databasePath;
  }

  /// Opens the catalog unless it is already open.
  ///
  /// The headless runner and the background worker both reach the catalog
  /// without going through app start-up, and neither should pay for a second
  /// seed check.
  Future<void> ensureOpen() async {
    if (_handle == null) await open();
  }

  void close() {
    _handle?.dispose();
    _handle = null;
  }

  Future<void> _seedFromAsset(File target) async {
    final data = await rootBundle.load(assetKey);
    final bytes = data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes);
    await target.writeAsBytes(bytes, flush: true);
  }

  /// True when this build ships a newer catalog than the installed file.
  ///
  /// Compared against the constant the build wrote into `app_info.dart`
  /// rather than against the asset itself: unpacking a few megabytes on every
  /// launch to compare two strings is the kind of start-up cost nobody ever
  /// goes looking for.
  bool _bundledIsNewer(String installedPath) {
    try {
      final installed = readMetadataAt(installedPath);
      return compareVersions(bundledCatalogVersion, installed.catalogVersion) > 0;
    } on Object {
      // Unreadable, wrong schema, or truncated — the bundled copy is known
      // good, so use it.
      return true;
    }
  }

  /// Reads the `meta` table of any catalog file without installing it.
  ///
  /// Used both for the live database and to inspect a download before it is
  /// allowed anywhere near the installed one.
  static CatalogMetadata readMetadataAt(String databasePath) {
    final database = sqlite3.open(databasePath, mode: OpenMode.readOnly);
    try {
      return _readVersion(database);
    } finally {
      database.dispose();
    }
  }

  static CatalogMetadata _readVersion(Database database) {
    final rows = database.select('SELECT key, value FROM meta');
    final meta = {
      for (final row in rows) row['key'] as String: row['value'] as String,
    };
    final schema = int.tryParse(meta['schemaVersion'] ?? '') ?? 0;
    if (schema != supportedSchemaVersion) {
      throw CatalogSchemaException(found: schema, supported: supportedSchemaVersion);
    }
    return CatalogMetadata(
      schemaVersion: schema,
      catalogVersion: meta['catalogVersion'] ?? '',
      verifiedAt: meta['verifiedAt'] ?? '',
      itemCount: int.tryParse(meta['itemCount'] ?? '') ?? 0,
    );
  }

  CatalogMetadata get metadata => _readVersion(_require());

  /// Every entry, in the order the build sorted them.
  ///
  /// The whole catalog fits in memory comfortably at this size, and the
  /// screens filter and sort it there; the database exists to make the file
  /// replaceable and the start-up read cheap, not to run the queries.
  List<CatalogItem> loadItems() {
    final database = _require();
    final showcases = <String, List<Showcase>>{};
    for (final row in database.select(
        'SELECT * FROM showcases ORDER BY item_id, position')) {
      final note = <String, String>{
        if ((row['note_en'] as String).isNotEmpty) 'en': row['note_en'] as String,
        if ((row['note_ru'] as String).isNotEmpty) 'ru': row['note_ru'] as String,
      };
      showcases.putIfAbsent(row['item_id'] as String, () => []).add(
            Showcase(
              title: row['title'] as String,
              url: row['url'] as String,
              kind: row['kind'] as String,
              note: note['en'] ?? note['ru'] ?? '',
              localizedNote: note,
              image: row['image'] as String,
            ),
          );
    }

    final rows = database.select('SELECT * FROM items ORDER BY sort_index');
    return [
      for (final row in rows) _itemFrom(row, showcases[row['id'] as String] ?? const []),
    ];
  }

  CatalogItem _itemFrom(Row row, List<Showcase> showcases) {
    final summaryEn = row['summary_en'] as String;
    final summaryRu = row['summary_ru'] as String;
    final tipEn = row['tip_en'] as String;
    final tipRu = row['tip_ru'] as String;
    final tags = _split(row['tags'] as String);

    return CatalogItem(
      id: row['id'] as String,
      name: row['name'] as String,
      kind: ItemKind.values.byName(row['kind'] as String),
      category: row['category'] as String,
      categoryGroup: row['category_group'] as String,
      // The entry reads in whichever language it was written in when the
      // chosen one is missing, which is what the UI falls back to anyway.
      summary: summaryEn.isNotEmpty ? summaryEn : summaryRu,
      url: row['url'] as String,
      access: AccessType.values.byName(row['access'] as String),
      compatibility: _split(row['compatibility'] as String),
      tags: tags,
      tip: tipEn.isNotEmpty ? tipEn : tipRu,
      verifiedAt: row['verified_at'] as String,
      install: row['install'] as String,
      source: row['source'] as String,
      sourceLanguage: summaryEn.isNotEmpty ? 'en' : 'ru',
      localizedSummary: {
        if (summaryEn.isNotEmpty) 'en': summaryEn,
        if (summaryRu.isNotEmpty) 'ru': summaryRu,
      },
      localizedTip: {
        if (tipEn.isNotEmpty) 'en': tipEn,
        if (tipRu.isNotEmpty) 'ru': tipRu,
      },
      showcases: showcases,
      pricing: PricingInfo(
        model: row['pricing_model'] as String,
        from: row['pricing_from'] as String,
        freeQuota: row['pricing_free_quota'] as String,
        url: row['pricing_url'] as String,
        currency: (row['pricing_currency'] as String).isEmpty
            ? 'USD'
            : row['pricing_currency'] as String,
        checkedAt: row['pricing_checked_at'] as String,
      ),
      searchIndex: SearchIndex(
        name: row['norm_name'] as String,
        category: row['norm_category'] as String,
        tags: _split(row['norm_tags'] as String),
        text: row['norm_text'] as String,
      ),
    );
  }

  static List<String> _split(String value) =>
      value.isEmpty ? const [] : value.split(',');

  Database _require() {
    final handle = _handle;
    if (handle == null) throw StateError('CatalogDatabase.open() has not run');
    return handle;
  }

  /// Replaces the installed catalog with [incoming], which must already be a
  /// verified, openable database.
  ///
  /// The live handle is closed first: on Windows a file cannot be replaced
  /// while it is open, and on every platform reading half a swap is worse than
  /// a moment without a catalog.
  Future<void> install(File incoming) async {
    final target = File(await resolvePath());
    // Rejects a truncated or mislabelled download before anything is replaced.
    readMetadataAt(incoming.path);
    close();
    if (await target.exists()) await target.delete();
    await incoming.rename(target.path);
    await open();
  }

  /// Orders two `YYYY.MM.DD` catalog versions. Unparseable parts compare as 0,
  /// so a malformed version never looks newer than a real one.
  static int compareVersions(String left, String right) {
    final a = left.split('.');
    final b = right.split('.');
    for (var i = 0; i < (a.length > b.length ? a.length : b.length); i++) {
      final x = i < a.length ? int.tryParse(a[i]) ?? 0 : 0;
      final y = i < b.length ? int.tryParse(b[i]) ?? 0 : 0;
      if (x != y) return x.compareTo(y);
    }
    return 0;
  }
}

/// What the `meta` table of a catalog file says about itself.
class CatalogMetadata {
  const CatalogMetadata({
    required this.schemaVersion,
    required this.catalogVersion,
    required this.verifiedAt,
    required this.itemCount,
  });

  final int schemaVersion;
  final String catalogVersion;
  final String verifiedAt;
  final int itemCount;
}

/// Thrown when a catalog file was built for a different app generation.
class CatalogSchemaException implements Exception {
  const CatalogSchemaException({required this.found, required this.supported});
  final int found;
  final int supported;

  @override
  String toString() =>
      'catalog schema $found is not readable by this build (supports $supported)';
}
