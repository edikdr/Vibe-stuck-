import '../models/catalog_item.dart';
import 'app_language.dart';
import 'categories.dart';

export 'app_language.dart';

class AppStrings {
  const AppStrings(this.language);

  final AppLanguage language;

  static const _values = <String, Map<AppLanguage, String>>{
    'appName': {AppLanguage.en: 'VibeStack Atlas', AppLanguage.ru: 'VibeStack Atlas', AppLanguage.de: 'VibeStack Atlas'},
    'navCatalog': {AppLanguage.en: 'Catalog', AppLanguage.ru: 'Каталог', AppLanguage.de: 'Katalog'},
    'navFavorites': {AppLanguage.en: 'Favorites', AppLanguage.ru: 'Избранное', AppLanguage.de: 'Favoriten'},
    'navAdd': {AppLanguage.en: 'Add', AppLanguage.ru: 'Добавить', AppLanguage.de: 'Hinzufügen'},
    'navUpdates': {AppLanguage.en: 'Updates', AppLanguage.ru: 'Обновления', AppLanguage.de: 'Updates'},
    'catalogKicker': {AppLanguage.en: 'TOOLS FOR VIBE CODING', AppLanguage.ru: 'ИНСТРУМЕНТЫ ДЛЯ VIBE CODING', AppLanguage.de: 'TOOLS FÜR VIBE CODING'},
    'catalogTitle': {AppLanguage.en: 'Find the right stack', AppLanguage.ru: 'Найди стек под задачу', AppLanguage.de: 'Finde den passenden Stack'},
    'catalogSubtitle': {AppLanguage.en: '{count} curated APIs, libraries, MCP servers and skills. Offline-first.', AppLanguage.ru: '{count} проверенных API, библиотек, MCP и skills. Работает офлайн.', AppLanguage.de: '{count} kuratierte APIs, Bibliotheken, MCP-Server und Skills. Offline-first.'},
    'searchHint': {AppLanguage.en: 'Try “free database”, “build UI” or “test an API”', AppLanguage.ru: 'Например: «бесплатная база», «сделать UI», «проверить API»', AppLanguage.de: 'Zum Beispiel „kostenlose Datenbank“, „UI bauen“ oder „API testen“'},
    'clear': {AppLanguage.en: 'Clear', AppLanguage.ru: 'Очистить', AppLanguage.de: 'Leeren'},
    'filters': {AppLanguage.en: 'Filters', AppLanguage.ru: 'Фильтры', AppLanguage.de: 'Filter'},
    'all': {AppLanguage.en: 'All', AppLanguage.ru: 'Все', AppLanguage.de: 'Alle'},
    'libraries': {AppLanguage.en: 'Libraries', AppLanguage.ru: 'Библиотеки', AppLanguage.de: 'Bibliotheken'},
    'skills': {AppLanguage.en: 'Skills', AppLanguage.ru: 'Skills', AppLanguage.de: 'Skills'},
    'withoutKey': {AppLanguage.en: 'No key', AppLanguage.ru: 'Без ключа', AppLanguage.de: 'Ohne Schlüssel'},
    'found': {AppLanguage.en: 'Found: {count}', AppLanguage.ru: 'Найдено: {count}', AppLanguage.de: 'Gefunden: {count}'},
    'resetFilters': {AppLanguage.en: 'Reset filters', AppLanguage.ru: 'Сбросить фильтры', AppLanguage.de: 'Filter zurücksetzen'},
    'compatibility': {AppLanguage.en: 'Compatibility', AppLanguage.ru: 'Совместимость', AppLanguage.de: 'Kompatibilität'},
    'access': {AppLanguage.en: 'Access', AppLanguage.ru: 'Доступ', AppLanguage.de: 'Zugang'},
    'any': {AppLanguage.en: 'Any', AppLanguage.ru: 'Любой', AppLanguage.de: 'Beliebig'},
    'mixed': {AppLanguage.en: 'Free option', AppLanguage.ru: 'Смешанный', AppLanguage.de: 'Kostenlose Option'},
    'category': {AppLanguage.en: 'Category', AppLanguage.ru: 'Категория', AppLanguage.de: 'Kategorie'},
    'allCategories': {AppLanguage.en: 'All categories', AppLanguage.ru: 'Все категории', AppLanguage.de: 'Alle Kategorien'},
    'showResults': {AppLanguage.en: 'Show results', AppLanguage.ru: 'Показать результаты', AppLanguage.de: 'Ergebnisse anzeigen'},
    'savedCount': {AppLanguage.en: '{count} saved', AppLanguage.ru: '{count} в избранном', AppLanguage.de: '{count} gespeichert'},
    'emptyTitle': {AppLanguage.en: 'Nothing found', AppLanguage.ru: 'Ничего не найдено', AppLanguage.de: 'Nichts gefunden'},
    'emptyHint': {AppLanguage.en: 'Shorten the query or reset filters', AppLanguage.ru: 'Сократи запрос или сбрось фильтры', AppLanguage.de: 'Suchanfrage kürzen oder Filter zurücksetzen'},
    'favoriteTitle': {AppLanguage.en: 'Favorites', AppLanguage.ru: 'Избранное', AppLanguage.de: 'Favoriten'},
    'favoriteSubtitle': {AppLanguage.en: '{count} saved tools', AppLanguage.ru: '{count} сохранённых инструментов', AppLanguage.de: '{count} gespeicherte Tools'},
    'favoriteEmpty': {AppLanguage.en: 'Bookmark useful cards to keep them here', AppLanguage.ru: 'Сохраняй полезные карточки закладкой', AppLanguage.de: 'Speichere nützliche Karten als Favoriten'},
    'addFavorite': {AppLanguage.en: 'Add to favorites', AppLanguage.ru: 'В избранное', AppLanguage.de: 'Zu Favoriten hinzufügen'},
    'removeFavorite': {AppLanguage.en: 'Remove from favorites', AppLanguage.ru: 'Убрать из избранного', AppLanguage.de: 'Aus Favoriten entfernen'},
    'forBoth': {AppLanguage.en: 'Codex + Claude', AppLanguage.ru: 'Codex + Claude', AppLanguage.de: 'Codex + Claude'},
    'verified': {AppLanguage.en: 'Verified', AppLanguage.ru: 'Проверено', AppLanguage.de: 'Geprüft'},
    'worksWith': {AppLanguage.en: 'Works with', AppLanguage.ru: 'Подходит для', AppLanguage.de: 'Geeignet für'},
    'quickStart': {AppLanguage.en: 'Quick start', AppLanguage.ru: 'Быстрый старт', AppLanguage.de: 'Schnellstart'},
    'copy': {AppLanguage.en: 'Copy', AppLanguage.ru: 'Копировать', AppLanguage.de: 'Kopieren'},
    'copied': {AppLanguage.en: 'Command copied', AppLanguage.ru: 'Команда скопирована', AppLanguage.de: 'Befehl kopiert'},
    'openOfficial': {AppLanguage.en: 'Open official source', AppLanguage.ru: 'Открыть официальный источник', AppLanguage.de: 'Offizielle Quelle öffnen'},
    'getKey': {AppLanguage.en: 'Official page / get a key', AppLanguage.ru: 'Официальная страница / получить ключ', AppLanguage.de: 'Offizielle Seite / Schlüssel erhalten'},
    'removeCustom': {AppLanguage.en: 'Delete my entry', AppLanguage.ru: 'Удалить мою запись', AppLanguage.de: 'Eigenen Eintrag löschen'},
    'fallbackTip': {AppLanguage.en: 'Start with the smallest official example and add one integration at a time.', AppLanguage.ru: 'Начни с минимального примера и добавляй интеграцию по одному шагу.', AppLanguage.de: 'Beginne mit dem kleinsten offiziellen Beispiel und ergänze jeweils nur eine Integration.'},
    'latestVersion': {AppLanguage.en: 'Latest version', AppLanguage.ru: 'Последняя версия', AppLanguage.de: 'Neueste Version'},
    'linkStatus': {AppLanguage.en: 'Link status', AppLanguage.ru: 'Состояние ссылки', AppLanguage.de: 'Linkstatus'},
    'reachable': {AppLanguage.en: 'Available', AppLanguage.ru: 'Доступна', AppLanguage.de: 'Erreichbar'},
    'unreachable': {AppLanguage.en: 'Check manually', AppLanguage.ru: 'Проверь вручную', AppLanguage.de: 'Manuell prüfen'},
    'liveSource': {AppLanguage.en: 'Live source', AppLanguage.ru: 'Живой источник', AppLanguage.de: 'Live-Quelle'},
    'addTitle': {AppLanguage.en: 'Add your own', AppLanguage.ru: 'Добавить своё', AppLanguage.de: 'Eigenen Eintrag hinzufügen'},
    'addSubtitle': {AppLanguage.en: 'Stored only on this device and included in smart search.', AppLanguage.ru: 'Запись хранится только на этом устройстве и участвует в умном поиске.', AppLanguage.de: 'Wird nur auf diesem Gerät gespeichert und in die intelligente Suche einbezogen.'},
    'type': {AppLanguage.en: 'Type', AppLanguage.ru: 'Тип', AppLanguage.de: 'Typ'},
    'nameRequired': {AppLanguage.en: 'Name *', AppLanguage.ru: 'Название *', AppLanguage.de: 'Name *'},
    'summaryRequired': {AppLanguage.en: 'What it does *', AppLanguage.ru: 'Что делает *', AppLanguage.de: 'Was es macht *'},
    'officialUrl': {AppLanguage.en: 'Official HTTPS URL *', AppLanguage.ru: 'Официальная HTTPS-ссылка *', AppLanguage.de: 'Offizielle HTTPS-URL *'},
    'categoryRequired': {AppLanguage.en: 'Category *', AppLanguage.ru: 'Категория *', AppLanguage.de: 'Kategorie *'},
    'categoryExample': {AppLanguage.en: 'For example: UI and frontend', AppLanguage.ru: 'Например: UI и frontend', AppLanguage.de: 'Zum Beispiel: UI und Frontend'},
    'tagsLabel': {AppLanguage.en: 'Comma-separated tags', AppLanguage.ru: 'Теги через запятую', AppLanguage.de: 'Tags, durch Kommas getrennt'},
    'tagsExample': {AppLanguage.en: 'ui, react, design', AppLanguage.ru: 'ui, react, design', AppLanguage.de: 'ui, react, design'},
    'miniTip': {AppLanguage.en: 'Mini tip', AppLanguage.ru: 'Мини-совет', AppLanguage.de: 'Kurztipp'},
    'tipExample': {AppLanguage.en: 'The best way to use the tool', AppLanguage.ru: 'Как лучше использовать инструмент', AppLanguage.de: 'So nutzt man das Tool am besten'},
    'installCommand': {AppLanguage.en: 'Install command', AppLanguage.ru: 'Команда установки', AppLanguage.de: 'Installationsbefehl'},
    'addToCatalog': {AppLanguage.en: 'Add to catalog', AppLanguage.ru: 'Добавить в каталог', AppLanguage.de: 'Zum Katalog hinzufügen'},
    'fillField': {AppLanguage.en: 'Fill in this field', AppLanguage.ru: 'Заполни поле', AppLanguage.de: 'Dieses Feld ausfüllen'},
    'invalidHttps': {AppLanguage.en: 'Enter a valid HTTPS URL', AppLanguage.ru: 'Нужна корректная HTTPS-ссылка', AppLanguage.de: 'Gültige HTTPS-URL eingeben'},
    'entryAdded': {AppLanguage.en: 'Entry added', AppLanguage.ru: 'Запись добавлена', AppLanguage.de: 'Eintrag hinzugefügt'},
    'both': {AppLanguage.en: 'Both', AppLanguage.ru: 'Оба', AppLanguage.de: 'Beide'},
    'updatesTitle': {AppLanguage.en: 'Updates', AppLanguage.ru: 'Обновления', AppLanguage.de: 'Updates'},
    'updatesSubtitle': {AppLanguage.en: 'Refresh official skills, MCP servers, package versions and API links without reinstalling.', AppLanguage.ru: 'Обновляй официальные skills, MCP, версии пакетов и ссылки API без переустановки.', AppLanguage.de: 'Offizielle Skills, MCP-Server, Paketversionen und API-Links ohne Neuinstallation aktualisieren.'},
    'language': {AppLanguage.en: 'Language', AppLanguage.ru: 'Язык', AppLanguage.de: 'Sprache'},
    'languageHint': {AppLanguage.en: 'English is used on first launch.', AppLanguage.ru: 'При первом запуске используется английский.', AppLanguage.de: 'Beim ersten Start wird Englisch verwendet.'},
    'automaticSources': {AppLanguage.en: 'Automatic sources', AppLanguage.ru: 'Автоматические источники', AppLanguage.de: 'Automatische Quellen'},
    'officialMcp': {AppLanguage.en: 'Official MCP Registry', AppLanguage.ru: 'Официальный MCP Registry', AppLanguage.de: 'Offizielle MCP Registry'},
    'officialMcpHint': {AppLanguage.en: 'Incremental updates from the stable v0.1 API.', AppLanguage.ru: 'Инкрементальные обновления через стабильный API v0.1.', AppLanguage.de: 'Inkrementelle Updates über die stabile v0.1-API.'},
    'skillRepos': {AppLanguage.en: 'Official and trusted skill repositories', AppLanguage.ru: 'Официальные и доверенные репозитории skills', AppLanguage.de: 'Offizielle und vertrauenswürdige Skill-Repositories'},
    'skillReposHint': {AppLanguage.en: 'OpenAI, Anthropic, Vercel, Expo, Hugging Face and Remotion.', AppLanguage.ru: 'OpenAI, Anthropic, Vercel, Expo, Hugging Face и Remotion.', AppLanguage.de: 'OpenAI, Anthropic, Vercel, Expo, Hugging Face und Remotion.'},
    'packageVersions': {AppLanguage.en: 'Package versions', AppLanguage.ru: 'Версии пакетов', AppLanguage.de: 'Paketversionen'},
    'packageVersionsHint': {AppLanguage.en: 'Checks npm and PyPI using public metadata APIs.', AppLanguage.ru: 'Проверяет npm и PyPI через публичные metadata API.', AppLanguage.de: 'Prüft npm und PyPI über öffentliche Metadaten-APIs.'},
    'apiLinks': {AppLanguage.en: 'API link health', AppLanguage.ru: 'Доступность ссылок API', AppLanguage.de: 'Erreichbarkeit der API-Links'},
    'apiLinksHint': {AppLanguage.en: 'Checks a rotating batch to avoid unnecessary traffic.', AppLanguage.ru: 'Проверяет небольшую очередь, чтобы не создавать лишний трафик.', AppLanguage.de: 'Prüft eine rotierende Auswahl, um unnötigen Datenverkehr zu vermeiden.'},
    'autoLaunch': {AppLanguage.en: 'Check on launch', AppLanguage.ru: 'Проверять при запуске', AppLanguage.de: 'Beim Start prüfen'},
    'autoLaunchHint': {AppLanguage.en: 'At most once every 24 hours; offline mode always remains available.', AppLanguage.ru: 'Не чаще раза в 24 часа; офлайн-режим всегда остаётся доступен.', AppLanguage.de: 'Höchstens einmal in 24 Stunden; der Offline-Modus bleibt immer verfügbar.'},
    'customFeed': {AppLanguage.en: 'Additional catalog feed', AppLanguage.ru: 'Дополнительная лента каталога', AppLanguage.de: 'Zusätzlicher Katalog-Feed'},
    'customFeedHint': {AppLanguage.en: 'Optional HTTPS catalog.json URL', AppLanguage.ru: 'Необязательный HTTPS-адрес catalog.json', AppLanguage.de: 'Optionale HTTPS-URL zu catalog.json'},
    'checkNow': {AppLanguage.en: 'Check now', AppLanguage.ru: 'Проверить сейчас', AppLanguage.de: 'Jetzt prüfen'},
    'checking': {AppLanguage.en: 'Checking…', AppLanguage.ru: 'Проверяю…', AppLanguage.de: 'Wird geprüft…'},
    'lastSync': {AppLanguage.en: 'Last sync: {date}', AppLanguage.ru: 'Последняя синхронизация: {date}', AppLanguage.de: 'Letzte Synchronisierung: {date}'},
    'never': {AppLanguage.en: 'Never', AppLanguage.ru: 'Никогда', AppLanguage.de: 'Nie'},
    'dataTransfer': {AppLanguage.en: 'Backup', AppLanguage.ru: 'Резервная копия', AppLanguage.de: 'Sicherung'},
    'dataTransferHint': {AppLanguage.en: 'Export or import custom entries, favorites and settings.', AppLanguage.ru: 'Экспортируй или импортируй свои записи, избранное и настройки.', AppLanguage.de: 'Eigene Einträge, Favoriten und Einstellungen exportieren oder importieren.'},
    'export': {AppLanguage.en: 'Export', AppLanguage.ru: 'Экспорт', AppLanguage.de: 'Exportieren'},
    'import': {AppLanguage.en: 'Import', AppLanguage.ru: 'Импорт', AppLanguage.de: 'Importieren'},
    'exported': {AppLanguage.en: 'Backup exported', AppLanguage.ru: 'Резервная копия экспортирована', AppLanguage.de: 'Sicherung exportiert'},
    'imported': {AppLanguage.en: 'Backup imported', AppLanguage.ru: 'Резервная копия импортирована', AppLanguage.de: 'Sicherung importiert'},
    'invalidBackup': {AppLanguage.en: 'This is not a valid VibeStack Atlas backup', AppLanguage.ru: 'Это не резервная копия VibeStack Atlas', AppLanguage.de: 'Dies ist keine gültige VibeStack-Atlas-Sicherung'},
    'updateLog': {AppLanguage.en: 'Update log', AppLanguage.ru: 'Журнал обновлений', AppLanguage.de: 'Update-Protokoll'},
    'updateLogEmpty': {AppLanguage.en: 'No update events yet', AppLanguage.ru: 'Событий обновления пока нет', AppLanguage.de: 'Noch keine Update-Ereignisse'},
    'clearLog': {AppLanguage.en: 'Clear log', AppLanguage.ru: 'Очистить журнал', AppLanguage.de: 'Protokoll leeren'},
    'keySafetyTitle': {AppLanguage.en: 'API key safety', AppLanguage.ru: 'Безопасность API-ключей', AppLanguage.de: 'Sicherheit von API-Schlüsseln'},
    'keySafetyText': {AppLanguage.en: 'The app never distributes shared secret keys. Cards link to official registration pages and distinguish no-key APIs, free tiers and trials. Free-tier terms cannot be verified reliably by automation, so check the official page before production use.', AppLanguage.ru: 'Приложение не раздаёт общие секретные ключи. Карточки ведут на официальную регистрацию и разделяют API без ключа, free tier и trial. Условия тарифов нельзя надёжно проверить автоматически — перед продакшеном проверь официальную страницу.', AppLanguage.de: 'Die App verteilt keine gemeinsamen geheimen Schlüssel. Die Karten verlinken auf offizielle Registrierungsseiten und unterscheiden APIs ohne Schlüssel, Free-Tiers und Testangebote. Tarifbedingungen lassen sich nicht zuverlässig automatisiert prüfen; vor dem Produktionseinsatz ist die offizielle Seite maßgeblich.'},
    'catalogDatabase': {AppLanguage.en: 'Catalog database', AppLanguage.ru: 'База каталога', AppLanguage.de: 'Katalog-Datenbank'},
    'catalogDatabaseHint': {AppLanguage.en: 'The whole catalog is one SQLite file. An update downloads a new one from the releases page and swaps it in; your favorites and entries are stored separately and never touched.', AppLanguage.ru: 'Весь каталог — один файл SQLite. Обновление скачивает новый со страницы релизов и подменяет его целиком; избранное и твои записи хранятся отдельно и не затрагиваются.', AppLanguage.de: 'Der gesamte Katalog ist eine SQLite-Datei. Ein Update lädt eine neue von der Release-Seite und tauscht sie aus; Favoriten und eigene Einträge liegen getrennt und bleiben unberührt.'},
    'catalogVersion': {AppLanguage.en: 'Installed: {version} · {count} entries', AppLanguage.ru: 'Установлена: {version} · {count} записей', AppLanguage.de: 'Installiert: {version} · {count} Einträge'},
    'catalogUpToDate': {AppLanguage.en: 'Catalog {version} is the newest published', AppLanguage.ru: 'Каталог {version} — самый свежий опубликованный', AppLanguage.de: 'Katalog {version} ist der neueste veröffentlichte'},
    'catalogInstalled': {AppLanguage.en: 'Catalog updated to {version}', AppLanguage.ru: 'Каталог обновлён до {version}', AppLanguage.de: 'Katalog auf {version} aktualisiert'},
    'catalogAppTooOld': {AppLanguage.en: 'Catalog {version} needs a newer version of the app', AppLanguage.ru: 'Каталогу {version} нужна более новая версия приложения', AppLanguage.de: 'Katalog {version} benötigt eine neuere App-Version'},
    'catalogUpdateFailed': {AppLanguage.en: 'Catalog update failed: {error}', AppLanguage.ru: 'Не удалось обновить каталог: {error}', AppLanguage.de: 'Katalog-Update fehlgeschlagen: {error}'},
    'catalogSource': {AppLanguage.en: 'Catalog release feed', AppLanguage.ru: 'Источник релизов каталога', AppLanguage.de: 'Release-Feed des Katalogs'},
    'catalogSourceHint': {AppLanguage.en: 'Leave empty to use the official releases page. A custom HTTPS manifest.json is for forks and testing.', AppLanguage.ru: 'Оставь пустым — используется официальная страница релизов. Свой HTTPS-адрес manifest.json нужен для форков и тестов.', AppLanguage.de: 'Leer lassen für die offizielle Release-Seite. Eine eigene HTTPS-manifest.json ist für Forks und Tests gedacht.'},
    'catalogCheckNow': {AppLanguage.en: 'Check for a new catalog', AppLanguage.ru: 'Проверить новую базу', AppLanguage.de: 'Nach neuem Katalog suchen'},
    'liveUpdateSummary': {AppLanguage.en: '{added} new entries, {versions} package versions and {links} links checked', AppLanguage.ru: '{added} новых записей, {versions} версий пакетов и {links} ссылок проверено', AppLanguage.de: '{added} neue Einträge, {versions} Paketversionen und {links} Links geprüft'},
    'syncFailed': {AppLanguage.en: 'Update failed: {error}', AppLanguage.ru: 'Ошибка обновления: {error}', AppLanguage.de: 'Update fehlgeschlagen: {error}'},
    'online': {AppLanguage.en: 'Live', AppLanguage.ru: 'Live', AppLanguage.de: 'Live'},
    'sortBy': {AppLanguage.en: 'Sort', AppLanguage.ru: 'Сортировка', AppLanguage.de: 'Sortierung'},
    'sortRelevance': {AppLanguage.en: 'Relevance', AppLanguage.ru: 'По релевантности', AppLanguage.de: 'Relevanz'},
    'sortName': {AppLanguage.en: 'Name A–Z', AppLanguage.ru: 'По названию', AppLanguage.de: 'Name A–Z'},
    'sortNew': {AppLanguage.en: 'Recently verified', AppLanguage.ru: 'Недавно проверенные', AppLanguage.de: 'Zuletzt geprüft'},
    'sortFreeFirst': {AppLanguage.en: 'Free first', AppLanguage.ru: 'Сначала бесплатные', AppLanguage.de: 'Kostenlose zuerst'},
    'sortPopular': {AppLanguage.en: 'Most popular', AppLanguage.ru: 'Самые популярные', AppLanguage.de: 'Beliebteste'},
    'mostUsed': {AppLanguage.en: 'Most used', AppLanguage.ru: 'Самые используемые', AppLanguage.de: 'Meistgenutzt'},
    'starsLabel': {AppLanguage.en: 'GitHub stars', AppLanguage.ru: 'звёзд на GitHub', AppLanguage.de: 'GitHub-Sterne'},
    'syncStars': {AppLanguage.en: 'Track GitHub popularity', AppLanguage.ru: 'Отслеживать популярность на GitHub', AppLanguage.de: 'GitHub-Popularität verfolgen'},
    'syncStarsHint': {
      AppLanguage.en: 'Refreshes star counts for a slice of the catalogue each day, powering the "most popular" sort.',
      AppLanguage.ru: 'Обновляет число звёзд для части каталога каждый день — на этом работает сортировка «самые популярные».',
      AppLanguage.de: 'Aktualisiert täglich die Sternzahlen für einen Teil des Katalogs, Grundlage der Sortierung „Beliebteste".',
    },
    'section': {AppLanguage.en: 'Section', AppLanguage.ru: 'Раздел', AppLanguage.de: 'Bereich'},
    'allSections': {AppLanguage.en: 'All sections', AppLanguage.ru: 'Все разделы', AppLanguage.de: 'Alle Bereiche'},
    'pricingTitle': {AppLanguage.en: 'Pricing', AppLanguage.ru: 'Стоимость', AppLanguage.de: 'Preise'},
    'priceFrom': {AppLanguage.en: 'From', AppLanguage.ru: 'От', AppLanguage.de: 'Ab'},
    'freeQuota': {AppLanguage.en: 'Free quota', AppLanguage.ru: 'Бесплатный лимит', AppLanguage.de: 'Kostenloses Kontingent'},
    'pricingModel': {AppLanguage.en: 'Billing', AppLanguage.ru: 'Модель оплаты', AppLanguage.de: 'Abrechnung'},
    'openPricing': {AppLanguage.en: 'Open pricing page', AppLanguage.ru: 'Открыть страницу цен', AppLanguage.de: 'Preisseite öffnen'},
    'pricingDisclaimer': {AppLanguage.en: 'Prices change often. Confirm on the official page before you rely on them.', AppLanguage.ru: 'Цены часто меняются. Перед использованием сверься с официальной страницей.', AppLanguage.de: 'Preise ändern sich häufig. Prüfe sie vor der Nutzung auf der offiziellen Seite.'},
    'priceChecked': {AppLanguage.en: 'Checked', AppLanguage.ru: 'Проверено', AppLanguage.de: 'Geprüft'},
    'priceFilter': {AppLanguage.en: 'Price', AppLanguage.ru: 'Цена', AppLanguage.de: 'Preis'},
    'anyPrice': {AppLanguage.en: 'Any', AppLanguage.ru: 'Любая', AppLanguage.de: 'Beliebig'},
    'freeOnly': {AppLanguage.en: 'Free', AppLanguage.ru: 'Бесплатно', AppLanguage.de: 'Kostenlos'},
    'freemiumOnly': {AppLanguage.en: 'Free tier', AppLanguage.ru: 'Free tier', AppLanguage.de: 'Free Tier'},
    'paidOnly': {AppLanguage.en: 'Paid', AppLanguage.ru: 'Платно', AppLanguage.de: 'Kostenpflichtig'},
    'withPricing': {AppLanguage.en: 'With prices', AppLanguage.ru: 'С ценами', AppLanguage.de: 'Mit Preisen'},
    'examples': {AppLanguage.en: 'Examples', AppLanguage.ru: 'Примеры', AppLanguage.de: 'Beispiele'},
    'examplesHint': {AppLanguage.en: 'Real projects, sites and galleries built with this tool.', AppLanguage.ru: 'Реальные проекты, сайты и галереи, сделанные на этом инструменте.', AppLanguage.de: 'Echte Projekte, Seiten und Galerien mit diesem Tool.'},
    'officialPage': {AppLanguage.en: 'Official page', AppLanguage.ru: 'Официальная страница', AppLanguage.de: 'Offizielle Seite'},
    'noExamples': {AppLanguage.en: 'No curated examples yet', AppLanguage.ru: 'Пока нет подобранных примеров', AppLanguage.de: 'Noch keine kuratierten Beispiele'},
    'openExample': {AppLanguage.en: 'Open', AppLanguage.ru: 'Открыть', AppLanguage.de: 'Öffnen'},
    'withExamples': {AppLanguage.en: 'With examples', AppLanguage.ru: 'С примерами', AppLanguage.de: 'Mit Beispielen'},
    'liveOnly': {AppLanguage.en: 'Live entries', AppLanguage.ru: 'Только live', AppLanguage.de: 'Nur Live'},
    'favoritesOnly': {AppLanguage.en: 'Favorites only', AppLanguage.ru: 'Только избранное', AppLanguage.de: 'Nur Favoriten'},
    'copyForAi': {AppLanguage.en: 'Copy for AI', AppLanguage.ru: 'Скопировать для ИИ', AppLanguage.de: 'Für KI kopieren'},
    'copyForAiHint': {AppLanguage.en: 'A ready context block: path, install command, docs link and caveats.', AppLanguage.ru: 'Готовый блок контекста: путь, команда установки, ссылка на доки и предупреждения.', AppLanguage.de: 'Fertiger Kontextblock: Pfad, Installationsbefehl, Doku-Link und Hinweise.'},
    'copiedForAi': {AppLanguage.en: 'Context copied for the agent', AppLanguage.ru: 'Контекст скопирован для агента', AppLanguage.de: 'Kontext für den Agenten kopiert'},
    'copyVisible': {AppLanguage.en: 'Copy all results', AppLanguage.ru: 'Скопировать всё найденное', AppLanguage.de: 'Alle Treffer kopieren'},
    'copiedCount': {AppLanguage.en: '{count} entries copied', AppLanguage.ru: 'Скопировано записей: {count}', AppLanguage.de: '{count} Einträge kopiert'},
    'dailyUpdate': {AppLanguage.en: 'Daily auto-update', AppLanguage.ru: 'Ежедневное автообновление', AppLanguage.de: 'Tägliches Auto-Update'},
    'dailyUpdateHint': {AppLanguage.en: 'Runs once a day in the background, even when the app is closed.', AppLanguage.ru: 'Запускается раз в сутки в фоне, даже когда приложение закрыто.', AppLanguage.de: 'Läuft einmal täglich im Hintergrund, auch bei geschlossener App.'},
    'updateHour': {AppLanguage.en: 'Update time', AppLanguage.ru: 'Время обновления', AppLanguage.de: 'Update-Zeit'},
    'nextUpdate': {AppLanguage.en: 'Next run', AppLanguage.ru: 'Следующий запуск', AppLanguage.de: 'Nächster Lauf'},
    'backgroundStatus': {AppLanguage.en: 'Background worker', AppLanguage.ru: 'Фоновый воркер', AppLanguage.de: 'Hintergrund-Worker'},
    'bgActive': {AppLanguage.en: 'Active (system scheduler)', AppLanguage.ru: 'Активен (системный планировщик)', AppLanguage.de: 'Aktiv (System-Scheduler)'},
    'bgInApp': {AppLanguage.en: 'In-app timer only', AppLanguage.ru: 'Только таймер внутри приложения', AppLanguage.de: 'Nur In-App-Timer'},
    'bgUnavailable': {AppLanguage.en: 'Unavailable on this platform', AppLanguage.ru: 'Недоступен на этой платформе', AppLanguage.de: 'Auf dieser Plattform nicht verfügbar'},
    'bgDesktopHint': {AppLanguage.en: 'On desktop, install the generated system task to update while the app is closed.', AppLanguage.ru: 'На desktop установи сгенерированную системную задачу, чтобы обновляться при закрытом приложении.', AppLanguage.de: 'Installiere auf dem Desktop die generierte Systemaufgabe für Updates bei geschlossener App.'},
    'imagesSection': {AppLanguage.en: 'Example images', AppLanguage.ru: 'Изображения примеров', AppLanguage.de: 'Beispielbilder'},
    'imagesHint': {AppLanguage.en: 'Previews are fetched once and cached on this device.', AppLanguage.ru: 'Превью загружаются один раз и кэшируются на устройстве.', AppLanguage.de: 'Vorschauen werden einmal geladen und lokal gespeichert.'},
    'imageProvider': {AppLanguage.en: 'Preview provider', AppLanguage.ru: 'Провайдер превью', AppLanguage.de: 'Vorschau-Anbieter'},
    'providerOff': {AppLanguage.en: 'Off (no external requests)', AppLanguage.ru: 'Выключено (без внешних запросов)', AppLanguage.de: 'Aus (keine externen Anfragen)'},
    'clearImageCache': {AppLanguage.en: 'Clear image cache', AppLanguage.ru: 'Очистить кэш изображений', AppLanguage.de: 'Bildcache leeren'},
    'imageCacheCleared': {AppLanguage.en: 'Image cache cleared', AppLanguage.ru: 'Кэш изображений очищен', AppLanguage.de: 'Bildcache geleert'},
    'cachedImages': {AppLanguage.en: 'Cached previews: {count}', AppLanguage.ru: 'Кэшировано превью: {count}', AppLanguage.de: 'Zwischengespeicherte Vorschauen: {count}'},
    'loadPreview': {AppLanguage.en: 'Load preview', AppLanguage.ru: 'Загрузить превью', AppLanguage.de: 'Vorschau laden'},
    'previewFailed': {AppLanguage.en: 'Preview unavailable', AppLanguage.ru: 'Превью недоступно', AppLanguage.de: 'Vorschau nicht verfügbar'},
    'shortcuts': {AppLanguage.en: 'Keyboard shortcuts', AppLanguage.ru: 'Горячие клавиши', AppLanguage.de: 'Tastenkürzel'},
    'detailPane': {AppLanguage.en: 'Select an entry to see details', AppLanguage.ru: 'Выбери запись, чтобы увидеть детали', AppLanguage.de: 'Wähle einen Eintrag für Details'},
    'backgroundDone': {AppLanguage.en: 'Updated in the background', AppLanguage.ru: 'Обновлено в фоне', AppLanguage.de: 'Im Hintergrund aktualisiert'},
    'filtersActive': {AppLanguage.en: 'Filters: {count}', AppLanguage.ru: 'Фильтров: {count}', AppLanguage.de: 'Filter: {count}'},
    'moreFilters': {AppLanguage.en: 'More filters', AppLanguage.ru: 'Ещё фильтры', AppLanguage.de: 'Weitere Filter'},
    'tagsTitle': {AppLanguage.en: 'Popular tags', AppLanguage.ru: 'Популярные теги', AppLanguage.de: 'Beliebte Tags'},
    'scrollTop': {AppLanguage.en: 'Back to top', AppLanguage.ru: 'Наверх', AppLanguage.de: 'Nach oben'},
    'scrollBack': {AppLanguage.en: 'Back down', AppLanguage.ru: 'Вернуться', AppLanguage.de: 'Zurück nach unten'},
  };

  String t(String key, [Map<String, Object> args = const {}]) {
    var value = _values[key]?[language] ?? _values[key]?[AppLanguage.en] ?? key;
    for (final entry in args.entries) {
      value = value.replaceAll('{${entry.key}}', entry.value.toString());
    }
    return value;
  }

  String kind(ItemKind kind) => switch (kind) {
        ItemKind.api => 'API',
        ItemKind.library => switch (language) {
            AppLanguage.en => 'Library',
            AppLanguage.ru => 'Библиотека',
            AppLanguage.de => 'Bibliothek',
          },
        ItemKind.mcp => 'MCP',
        ItemKind.skill => 'Skill',
      };

  String access(AccessType access) => switch (access) {
        AccessType.noKey => t('withoutKey'),
        AccessType.freeTier => switch (language) {
            AppLanguage.en => 'Free tier',
            AppLanguage.ru => 'Бесплатный тариф',
            AppLanguage.de => 'Kostenloser Tarif',
          },
        AccessType.openSource => switch (language) {
            AppLanguage.en => 'Open source',
            AppLanguage.ru => 'Открытый код',
            AppLanguage.de => 'Open Source',
          },
        AccessType.free => language == AppLanguage.de ? 'Kostenlos' : language == AppLanguage.ru ? 'Бесплатно' : 'Free',
        AccessType.mixed => t('mixed'),
        AccessType.paid => t('paidOnly'),
      };

  String category(String raw) => categoryLabels[raw]?[language] ?? raw;

  String group(String raw) => groupLabels[raw]?[language] ?? raw;

  String groupOfCategory(String raw) => categoryGroupOf[raw] ?? 'devtools';

  String pricingModel(String model) => switch ((model, language)) {
        ('free', AppLanguage.ru) => 'Бесплатно',
        ('free', AppLanguage.de) => 'Kostenlos',
        ('free', _) => 'Free',
        ('openSource', AppLanguage.ru) => 'Открытый код',
        ('openSource', _) => 'Open source',
        ('freemium', AppLanguage.ru) => 'Free tier + платные планы',
        ('freemium', AppLanguage.de) => 'Free Tier + kostenpflichtige Pläne',
        ('freemium', _) => 'Free tier plus paid plans',
        ('usage', AppLanguage.ru) => 'Оплата по потреблению',
        ('usage', AppLanguage.de) => 'Nutzungsbasiert',
        ('usage', _) => 'Pay as you go',
        ('subscription', AppLanguage.ru) => 'Подписка',
        ('subscription', AppLanguage.de) => 'Abonnement',
        ('subscription', _) => 'Subscription',
        ('credits', AppLanguage.ru) => 'Кредиты',
        ('credits', AppLanguage.de) => 'Guthaben',
        ('credits', _) => 'Credits',
        ('commission', AppLanguage.ru) => 'Комиссия с транзакции',
        ('commission', AppLanguage.de) => 'Transaktionsgebühr',
        ('commission', _) => 'Per transaction fee',
        ('hybrid', AppLanguage.ru) => 'Смешанная модель',
        ('hybrid', AppLanguage.de) => 'Gemischtes Modell',
        _ => 'Mixed',
      };

  String summary(CatalogItem item) {
    final explicit = item.localizedSummary[language.code];
    if (explicit != null && explicit.isNotEmpty) return explicit;
    if (language == AppLanguage.ru && item.sourceLanguage == 'ru') return item.summary;
    if (language == AppLanguage.en && item.sourceLanguage == 'en') return item.summary;
    final topics = item.tags.take(4).join(', ');
    return switch (language) {
      AppLanguage.en => '${item.name} is a ${kind(item.kind).toLowerCase()} for ${category(item.category).toLowerCase()}. Main areas: $topics.',
      AppLanguage.ru => '${item.name} — ${kind(item.kind).toLowerCase()} для категории «${category(item.category)}». Основные темы: $topics.',
      AppLanguage.de => '${item.name} ist ein Tool vom Typ ${kind(item.kind)} für ${category(item.category)}. Schwerpunkte: $topics.',
    };
  }

  String tip(CatalogItem item) {
    final explicit = item.localizedTip[language.code];
    if (explicit != null && explicit.isNotEmpty) return explicit;
    if (language == AppLanguage.ru && item.sourceLanguage == 'ru' && item.tip.isNotEmpty) return item.tip;
    return switch ((language, item.kind)) {
      (AppLanguage.en, ItemKind.api) => item.access == AccessType.noKey
          ? 'Start with the official minimal request, cache responses and respect the published rate limits.'
          : 'Create your own key, keep it outside client code and add explicit rate-limit handling.',
      (AppLanguage.en, ItemKind.library) => 'Pin a known working version, start with the official minimal example and add tests before upgrading.',
      (AppLanguage.en, ItemKind.mcp) => 'Review every permission and start in read-only mode with the smallest possible scope.',
      (AppLanguage.en, ItemKind.skill) => 'Read the full SKILL.md and every bundled script before installing or invoking it.',
      (AppLanguage.de, ItemKind.api) => item.access == AccessType.noKey
          ? 'Beginne mit der kleinsten offiziellen Anfrage, cache Antworten und beachte die veröffentlichten Limits.'
          : 'Erstelle einen eigenen Schlüssel, halte ihn aus dem Client-Code heraus und behandle Rate-Limits ausdrücklich.',
      (AppLanguage.de, ItemKind.library) => 'Fixiere eine funktionierende Version, beginne mit dem offiziellen Minimalbeispiel und teste vor jedem Upgrade.',
      (AppLanguage.de, ItemKind.mcp) => 'Prüfe alle Berechtigungen und beginne schreibgeschützt mit dem kleinstmöglichen Umfang.',
      (AppLanguage.de, ItemKind.skill) => 'Lies die vollständige SKILL.md und alle enthaltenen Skripte vor der Installation.',
      (AppLanguage.ru, ItemKind.api) => item.access == AccessType.noKey
          ? 'Начни с минимального официального запроса, кэшируй ответы и соблюдай лимиты.'
          : 'Заведи свой ключ, держи его вне клиентского кода и обрабатывай rate limit явно.',
      (AppLanguage.ru, ItemKind.library) => 'Зафиксируй рабочую версию, начни с официального минимального примера и добавь тесты до апгрейда.',
      (AppLanguage.ru, ItemKind.mcp) => 'Проверь все разрешения и начни в read-only режиме с минимальной областью доступа.',
      // Every (language, kind) pair above is covered, so there is deliberately
      // no default arm: a new language or kind should fail to compile here
      // rather than silently fall back to English.
      (AppLanguage.ru, ItemKind.skill) => 'Прочитай весь SKILL.md и вложенные скрипты до установки и запуска.',
    };
  }

}
