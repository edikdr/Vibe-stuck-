import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:sqlite3/sqlite3.dart';
import 'package:vibestack_atlas/app_info.dart';
import 'package:vibestack_atlas/i18n/app_strings.dart';
import 'package:vibestack_atlas/i18n/categories.dart';
import 'package:vibestack_atlas/models/catalog_item.dart';
import 'package:vibestack_atlas/services/catalog_database.dart';

/// Contract test for the catalog the app actually ships.
///
/// It reads `assets/catalog.db` through the same code the app uses, so a
/// broken entry fails here rather than rendering as an empty card. The same
/// contract is enforced without a Flutter SDK by `tool/verify_catalog.py`,
/// which is what CI gates catalog changes on; this test additionally proves
/// the Dart side can read every row the build produced.
void main() {
  final reason = _sqliteUnavailableReason();

  group('bundled catalog', skip: reason, () {
    late CatalogDatabase database;
    late List<CatalogItem> items;

    setUpAll(() {
      expect(File('assets/catalog.db').existsSync(), isTrue,
          reason: 'assets/catalog.db is missing — run tool/build_catalog.py');
      database = CatalogDatabase()..openFile('assets/catalog.db');
      items = database.loadItems();
    });

    tearDownAll(() => database.close());

    test('metadata matches what this build was compiled against', () {
      final meta = database.metadata;
      expect(meta.schemaVersion, bundledCatalogSchema);
      expect(meta.catalogVersion, bundledCatalogVersion);
      expect(meta.itemCount, items.length);
      expect(items.length, bundledCatalogItemCount);
    });

    test('ids and names are unique', () {
      final ids = <String>{};
      final names = <String>{};
      for (final item in items) {
        expect(ids.add(item.id), isTrue, reason: 'duplicate id ${item.id}');
        expect(names.add(item.name.toLowerCase()), isTrue,
            reason: 'duplicate name ${item.name}');
      }
    });

    test('every entry has https links and required text', () {
      for (final item in items) {
        expect(item.url, startsWith('https://'), reason: item.id);
        expect(item.summary.trim(), isNotEmpty, reason: item.id);
        expect(item.tip.trim(), isNotEmpty, reason: item.id);
        expect(item.tags, isNotEmpty, reason: item.id);
        expect(item.verifiedAt, matches(RegExp(r'^\d{4}-\d{2}-\d{2}$')),
            reason: item.id);
      }
    });

    test('every category resolves to a translated label and a group', () {
      const en = AppStrings(AppLanguage.en);
      const de = AppStrings(AppLanguage.de);
      for (final item in items) {
        // `category()` falls back to the raw key when the generated map has no
        // entry, and that fallback is the actual failure mode. Comparing the
        // label against the key cannot detect it, because several categories
        // are already English in the source data ("AI skills", "Workflow
        // skills"), so the check asks the generated maps directly instead.
        expect(categoryLabels.containsKey(item.category), isTrue,
            reason: 'no generated label for ${item.category}');
        expect(categoryGroupOf.containsKey(item.category), isTrue,
            reason: 'no group mapping for ${item.category}');
        expect(en.category(item.category), isNotEmpty, reason: item.category);
        expect(de.category(item.category), isNotEmpty, reason: item.category);
        expect(item.categoryGroup, isNotEmpty, reason: item.id);
      }
    });

    test('showcases are well formed', () {
      var cards = 0;
      for (final item in items) {
        for (final showcase in item.showcases) {
          cards++;
          expect(showcase.title.trim(), isNotEmpty, reason: item.id);
          expect(showcase.url, startsWith('https://'), reason: item.id);
          expect(const ['gallery', 'site', 'demo', 'code'],
              contains(showcase.kind),
              reason: '${item.id}: ${showcase.kind}');
        }
      }
      expect(cards, greaterThanOrEqualTo(90));
    });

    test('paid entries carry a price', () {
      for (final item in items.where((entry) => entry.access == AccessType.paid)) {
        expect(item.pricing.from.trim(), isNotEmpty, reason: item.id);
        expect(item.pricing.model, isNot('free'), reason: item.id);
      }
    });

    test('every kind is represented', () {
      for (final kind in ItemKind.values) {
        expect(items.where((item) => item.kind == kind).length, greaterThan(20),
            reason: 'too few entries of kind $kind');
      }
    });

    test('every entry ships pre-normalized search text', () {
      // The whole point of storing it: if it were missing the app would fall
      // back to normalizing on every keystroke, and nothing else would notice.
      for (final item in items) {
        final index = item.searchIndex;
        expect(index, isNotNull, reason: item.id);
        expect(index!.name, isNotEmpty, reason: item.id);
        expect(index.text, contains(index.name), reason: item.id);
        expect(index.name, equals(index.name.toLowerCase()), reason: item.id);
      }
    });

    test('version comparison orders catalog releases', () {
      expect(CatalogDatabase.compareVersions('2026.08.19', '2026.08.18'),
          greaterThan(0));
      expect(CatalogDatabase.compareVersions('2026.08.09', '2026.08.10'),
          lessThan(0));
      expect(CatalogDatabase.compareVersions('2026.08.19', '2026.08.19'), 0);
      // A malformed version must never look newer than a real one.
      expect(CatalogDatabase.compareVersions('', '2026.08.19'), lessThan(0));
    });
  });
}

/// Why the database tests cannot run here, or null when they can.
///
/// `flutter test` runs on the host VM, where the native SQLite library comes
/// from the operating system rather than from `sqlite3_flutter_libs`. Linux
/// and macOS runners always have it; a bare Windows machine may not. The
/// Python verifier gates catalog changes in CI either way, so skipping is
/// honest rather than a hole.
String? _sqliteUnavailableReason() {
  try {
    sqlite3.version;
    return null;
  } on Object catch (error) {
    return 'no native SQLite library on this host ($error)';
  }
}
