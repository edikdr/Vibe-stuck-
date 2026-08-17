import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:vibestack_atlas/i18n/app_strings.dart';
import 'package:vibestack_atlas/i18n/categories.dart';
import 'package:vibestack_atlas/models/catalog_item.dart';

/// Contract test for the bundled catalog.
///
/// It runs on the real asset, so a broken entry fails CI instead of shipping
/// as an empty card. Same idea as verify-project.js in the web projects.
void main() {
  late Map<String, dynamic> raw;
  late List<CatalogItem> items;

  setUpAll(() {
    final file = File('assets/catalog.json');
    expect(file.existsSync(), isTrue, reason: 'assets/catalog.json is missing');
    raw = jsonDecode(file.readAsStringSync()) as Map<String, dynamic>;
    items = (raw['items'] as List)
        .map((entry) => CatalogItem.fromJson(entry as Map<String, dynamic>))
        .toList();
  });

  test('schema version and size', () {
    expect(raw['schemaVersion'], 3);
    expect(items.length, greaterThanOrEqualTo(380));
  });

  test('ids and names are unique', () {
    final ids = <String>{};
    final names = <String>{};
    for (final item in items) {
      expect(ids.add(item.id), isTrue, reason: 'duplicate id ${item.id}');
      expect(names.add(item.name.toLowerCase()), isTrue, reason: 'duplicate name ${item.name}');
    }
  });

  test('every entry has https links and required text', () {
    for (final item in items) {
      expect(item.url, startsWith('https://'), reason: item.id);
      expect(item.summary.trim(), isNotEmpty, reason: item.id);
      expect(item.tip.trim(), isNotEmpty, reason: item.id);
      expect(item.tags, isNotEmpty, reason: item.id);
      expect(item.verifiedAt, matches(RegExp(r'^\d{4}-\d{2}-\d{2}$')), reason: item.id);
    }
  });

  test('every category resolves to a translated label and a group', () {
    const en = AppStrings(AppLanguage.en);
    const de = AppStrings(AppLanguage.de);
    for (final item in items) {
      // `category()` falls back to the raw key when the generated map has no
      // entry, and that fallback is the actual failure mode. Comparing the
      // label against the key cannot detect it, because several categories are
      // already English in the source data ("AI skills", "Workflow skills"),
      // so the check asks the generated maps directly instead.
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
        expect(const ['gallery', 'site', 'demo', 'code'], contains(showcase.kind),
            reason: '${item.id}: ${showcase.kind}');
      }
    }
    expect(cards, greaterThanOrEqualTo(90));
  });

  test('paid entries carry a price and a pricing link', () {
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
}
