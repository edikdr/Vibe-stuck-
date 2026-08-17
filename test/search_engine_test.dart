import 'package:flutter_test/flutter_test.dart';
import 'package:vibestack_atlas/models/catalog_item.dart';
import 'package:vibestack_atlas/services/search_engine.dart';

void main() {
  const items = [
    CatalogItem(
      id: 'db',
      name: 'Postgres Tool',
      kind: ItemKind.library,
      category: 'Базы данных и ORM',
      summary: 'Типизированная работа с SQL',
      url: 'https://example.com/db',
      access: AccessType.openSource,
      compatibility: ['both'],
      tags: ['database', 'postgres', 'sql'],
      tip: 'Use migrations',
      verifiedAt: '2026-08-12',
    ),
    CatalogItem(
      id: 'ui',
      name: 'UI Kit',
      kind: ItemKind.library,
      category: 'UI и frontend',
      summary: 'Компоненты интерфейса',
      url: 'https://example.com/ui',
      access: AccessType.openSource,
      compatibility: ['both'],
      tags: ['ui', 'frontend', 'components'],
      tip: 'Use tokens',
      verifiedAt: '2026-08-12',
    ),
  ];

  test('expands Russian intent terms', () {
    final result = SearchEngine().search(items, 'бесплатная база данных');
    expect(result.first.id, 'db');
  });

  test('expands English and German intent terms', () {
    expect(SearchEngine().search(items, 'free database').first.id, 'db');
    expect(SearchEngine().search(items, 'kostenlose Datenbank').first.id, 'db');
    expect(SearchEngine().search(items, 'Oberfläche bauen').first.id, 'ui');
  });

  test('respects kind and compatibility filters', () {
    final result = SearchEngine().search(
      items,
      '',
      kind: ItemKind.library,
      compatibility: 'codex',
    );
    expect(result, hasLength(2));
  });
}
