import 'package:flutter_test/flutter_test.dart';
import 'package:vibestack_atlas/i18n/app_strings.dart';
import 'package:vibestack_atlas/models/catalog_item.dart';

void main() {
  const item = CatalogItem(
    id: 'sample',
    name: 'Sample Tool',
    kind: ItemKind.library,
    category: 'Базы данных и ORM',
    summary: 'Работа с базой данных',
    url: 'https://example.com',
    access: AccessType.openSource,
    compatibility: ['both'],
    tags: ['database', 'sql'],
    tip: 'Начни с миграций.',
    verifiedAt: '2026-08-12',
  );

  test('English is the default language enum value', () {
    expect(AppLanguage.values.first, AppLanguage.en);
  });

  test('navigation and catalog metadata are translated', () {
    expect(const AppStrings(AppLanguage.en).t('navCatalog'), 'Catalog');
    expect(const AppStrings(AppLanguage.ru).t('navCatalog'), 'Каталог');
    expect(const AppStrings(AppLanguage.de).t('navCatalog'), 'Katalog');
    expect(const AppStrings(AppLanguage.en).category(item.category), 'Databases and ORM');
    expect(const AppStrings(AppLanguage.de).category(item.category), 'Datenbanken und ORM');
  });

  test('seed content keeps Russian text and gets localized fallbacks', () {
    expect(const AppStrings(AppLanguage.ru).summary(item), item.summary);
    expect(const AppStrings(AppLanguage.en).summary(item), contains('database'));
    expect(const AppStrings(AppLanguage.de).summary(item), contains('Datenbanken'));
  });
}
