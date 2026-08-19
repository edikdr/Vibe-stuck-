import 'dart:convert';

import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

import '../i18n/app_strings.dart';
import '../models/catalog_item.dart';
import 'catalog_database.dart';

AppLanguage _languageFromCode(String code) {
  for (final value in AppLanguage.values) {
    if (value.code == code) return value;
  }
  return AppLanguage.en;
}

class SyncResult {
  const SyncResult({required this.changed, required this.count, this.message = ''});
  final bool changed;
  final int count;
  final String message;
}

class BackupData {
  const BackupData({required this.customItems, required this.favorites, required this.language, required this.feedUrl, required this.settings});
  final List<CatalogItem> customItems;
  final Set<String> favorites;
  final AppLanguage language;
  final String feedUrl;
  final Map<String, bool> settings;
}

class CatalogRepository {
  CatalogRepository({CatalogDatabase? catalog})
      : catalog = catalog ?? CatalogDatabase();

  /// The shipped catalog. Read-only, replaced wholesale by an update — which
  /// is exactly why none of the user's own data below is kept in it.
  final CatalogDatabase catalog;

  static const _customKey = 'custom_items_v1';
  static const _feedItemsKey = 'remote_items_v1';
  static const _liveItemsKey = 'live_items_v2';
  static const _favoritesKey = 'favorites_v1';
  static const _feedKey = 'catalog_feed_url_v1';
  static const _autoSyncKey = 'auto_sync_v1';
  static const _lastSyncKey = 'last_sync_v1';
  static const _languageKey = 'language_v2';
  static const _versionsKey = 'package_versions_v2';
  static const _healthKey = 'link_health_v2';
  static const _eventsKey = 'update_events_v2';
  static const _syncMcpKey = 'sync_mcp_v2';
  static const _syncSkillsKey = 'sync_skills_v2';
  static const _syncVersionsKey = 'sync_versions_v2';
  static const _syncLinksKey = 'sync_links_v2';
  static const _syncStarsKey = 'sync_stars_v1';
  static const _syncHourKey = 'sync_hour_v1';
  static const _syncMinuteKey = 'sync_minute_v1';
  static const _previewProviderKey = 'preview_provider_v1';
  static const _backgroundResultKey = 'background_result_v1';
  static const _sortModeKey = 'sort_mode_v1';
  static const _starsKey = 'github_stars_v1';
  static const _catalogUrlKey = 'catalog_manifest_url_v1';
  static const _catalogCheckKey = 'catalog_checked_v1';

  /// The curated catalog, read from SQLite instead of parsed from a JSON asset.
  Future<List<CatalogItem>> loadBundled() async {
    await catalog.ensureOpen();
    return catalog.loadItems();
  }

  /// Version of the installed catalog file, or an empty string if it has not
  /// been opened yet.
  String get catalogVersion =>
      catalog.isOpen ? catalog.metadata.catalogVersion : '';

  /// Where catalog updates are published. Empty means the built-in release
  /// feed; a custom value is for forks and for testing a release privately.
  Future<String> loadCatalogManifestUrl() => _loadString(_catalogUrlKey, '');
  Future<void> saveCatalogManifestUrl(String value) =>
      _saveString(_catalogUrlKey, value.trim());

  Future<DateTime?> loadCatalogCheckedAt() async {
    final prefs = await SharedPreferences.getInstance();
    final value = prefs.getString(_catalogCheckKey);
    return value == null ? null : DateTime.tryParse(value);
  }

  Future<void> saveCatalogCheckedAt(DateTime value) =>
      _saveString(_catalogCheckKey, value.toIso8601String());

  Future<List<CatalogItem>> _loadItems(String key) async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(key);
    if (raw == null || raw.isEmpty) return [];
    try {
      return (jsonDecode(raw) as List).map((item) => CatalogItem.fromJson(item as Map<String, dynamic>)).toList();
    } catch (_) {
      return [];
    }
  }

  Future<void> _saveItems(String key, List<CatalogItem> items) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(key, jsonEncode(items.map((item) => item.toJson()).toList()));
  }

  Future<List<CatalogItem>> loadCustom() => _loadItems(_customKey);
  Future<List<CatalogItem>> loadFeedItems() => _loadItems(_feedItemsKey);
  Future<List<CatalogItem>> loadLiveItems() => _loadItems(_liveItemsKey);
  Future<void> saveCustom(List<CatalogItem> items) => _saveItems(_customKey, items);
  Future<void> saveFeedItems(List<CatalogItem> items) => _saveItems(_feedItemsKey, items);
  Future<void> saveLiveItems(List<CatalogItem> items) => _saveItems(_liveItemsKey, items);

  Future<Set<String>> loadFavorites() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getStringList(_favoritesKey)?.toSet() ?? <String>{};
  }

  Future<void> saveFavorites(Set<String> ids) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setStringList(_favoritesKey, ids.toList()..sort());
  }

  Future<String> loadFeedUrl() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString(_feedKey) ?? '';
  }

  Future<void> saveFeedUrl(String value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_feedKey, value.trim());
  }

  Future<AppLanguage> loadLanguage() async {
    final prefs = await SharedPreferences.getInstance();
    final code = prefs.getString(_languageKey) ?? 'en';
    return _languageFromCode(code);
  }

  Future<void> saveLanguage(AppLanguage value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_languageKey, value.code);
  }

  Future<bool> loadAutoSync() => _loadBool(_autoSyncKey, true);
  Future<void> saveAutoSync(bool value) => _saveBool(_autoSyncKey, value);
  Future<bool> loadSyncMcp() => _loadBool(_syncMcpKey, true);
  Future<void> saveSyncMcp(bool value) => _saveBool(_syncMcpKey, value);
  Future<bool> loadSyncSkills() => _loadBool(_syncSkillsKey, true);
  Future<void> saveSyncSkills(bool value) => _saveBool(_syncSkillsKey, value);
  Future<bool> loadSyncVersions() => _loadBool(_syncVersionsKey, true);
  Future<void> saveSyncVersions(bool value) => _saveBool(_syncVersionsKey, value);
  Future<bool> loadSyncLinks() => _loadBool(_syncLinksKey, true);
  Future<void> saveSyncLinks(bool value) => _saveBool(_syncLinksKey, value);

  Future<bool> loadSyncStars() => _loadBool(_syncStarsKey, true);
  Future<void> saveSyncStars(bool value) => _saveBool(_syncStarsKey, value);

  Future<bool> _loadBool(String key, bool fallback) async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getBool(key) ?? fallback;
  }

  Future<void> _saveBool(String key, bool value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setBool(key, value);
  }

  Future<int> _loadInt(String key, int fallback) async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getInt(key) ?? fallback;
  }

  Future<void> _saveInt(String key, int value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setInt(key, value);
  }

  Future<String> _loadString(String key, String fallback) async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString(key) ?? fallback;
  }

  Future<void> _saveString(String key, String value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(key, value);
  }

  /// Hour and minute of the daily update, in device local time.
  Future<int> loadSyncHour() => _loadInt(_syncHourKey, 9);
  Future<void> saveSyncHour(int value) => _saveInt(_syncHourKey, value);
  Future<int> loadSyncMinute() => _loadInt(_syncMinuteKey, 0);
  Future<void> saveSyncMinute(int value) => _saveInt(_syncMinuteKey, value);

  Future<String> loadPreviewProvider() => _loadString(_previewProviderKey, 'mshots');
  Future<void> savePreviewProvider(String value) => _saveString(_previewProviderKey, value);

  /// Older builds stored 'freeFirst', which the search engine never matched,
  /// so that sort silently behaved like relevance. Map it onto the real value.
  Future<String> loadSortMode() async {
    final stored = await _loadString(_sortModeKey, 'relevance');
    return stored == 'freeFirst' ? 'free' : stored;
  }
  Future<void> saveSortMode(String value) => _saveString(_sortModeKey, value);

  /// Set by the headless runner so the UI can report what happened while the
  /// app was closed.
  Future<String> loadBackgroundResult() => _loadString(_backgroundResultKey, '');
  Future<void> saveBackgroundResult(String value) => _saveString(_backgroundResultKey, value);

  Future<DateTime?> loadLastSync() async {
    final prefs = await SharedPreferences.getInstance();
    final value = prefs.getString(_lastSyncKey);
    return value == null ? null : DateTime.tryParse(value);
  }

  Future<void> saveLastSync(DateTime value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_lastSyncKey, value.toIso8601String());
  }

  Future<Map<String, String>> loadVersions() => _loadStringMap(_versionsKey);
  Future<void> saveVersions(Map<String, String> value) => _saveStringMap(_versionsKey, value);

  Future<Map<String, bool>> loadLinkHealth() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_healthKey);
    if (raw == null) return {};
    try {
      return Map<String, bool>.from(jsonDecode(raw) as Map);
    } catch (_) {
      return {};
    }
  }

  Future<void> saveLinkHealth(Map<String, bool> value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_healthKey, jsonEncode(value));
  }

  /// Star counts per entry id, refreshed a slice at a time by the daily sync.
  ///
  /// Stored separately from the catalog so a popularity refresh never rewrites
  /// entry data, and so a cleared cache degrades to "no ranking" rather than a
  /// broken catalog.
  Future<Map<String, int>> loadStars() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_starsKey);
    if (raw == null) return {};
    try {
      return Map<String, int>.from(jsonDecode(raw) as Map);
    } catch (_) {
      return {};
    }
  }

  Future<void> saveStars(Map<String, int> value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_starsKey, jsonEncode(value));
  }

  Future<Map<String, String>> _loadStringMap(String key) async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(key);
    if (raw == null) return {};
    try {
      return Map<String, String>.from(jsonDecode(raw) as Map);
    } catch (_) {
      return {};
    }
  }

  Future<void> _saveStringMap(String key, Map<String, String> value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(key, jsonEncode(value));
  }

  Future<List<UpdateEvent>> loadEvents() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_eventsKey);
    if (raw == null) return [];
    try {
      return (jsonDecode(raw) as List).map((item) => UpdateEvent.fromJson(item as Map<String, dynamic>)).toList();
    } catch (_) {
      return [];
    }
  }

  Future<void> saveEvents(List<UpdateEvent> events) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_eventsKey, jsonEncode(events.take(150).map((item) => item.toJson()).toList()));
  }

  Future<SyncResult> syncFeed(String feedUrl) async {
    if (feedUrl.trim().isEmpty) return const SyncResult(changed: false, count: 0);
    final uri = Uri.tryParse(feedUrl.trim());
    if (uri == null || uri.scheme != 'https') return const SyncResult(changed: false, count: 0, message: 'Invalid HTTPS URL');
    final response = await http.get(uri).timeout(const Duration(seconds: 12));
    if (response.statusCode != 200) return SyncResult(changed: false, count: 0, message: 'HTTP ${response.statusCode}');
    final decoded = jsonDecode(utf8.decode(response.bodyBytes));
    final list = decoded is List ? decoded : (decoded as Map<String, dynamic>)['items'];
    if (list is! List) return const SyncResult(changed: false, count: 0, message: 'Missing items array');
    final items = list.map((item) => CatalogItem.fromJson(item as Map<String, dynamic>)).toList(growable: false);
    await saveFeedItems(items);
    return SyncResult(changed: true, count: items.length);
  }

  String createBackup({
    required List<CatalogItem> customItems,
    required Set<String> favorites,
    required AppLanguage language,
    required String feedUrl,
    required Map<String, bool> settings,
  }) {
    return const JsonEncoder.withIndent('  ').convert({
      'format': 'vibestack-atlas-backup',
      'version': 2,
      'exportedAt': DateTime.now().toUtc().toIso8601String(),
      'customItems': customItems.map((item) => item.toJson()).toList(),
      'favorites': favorites.toList(),
      'language': language.code,
      'feedUrl': feedUrl,
      'settings': settings,
    });
  }

  BackupData parseBackup(String raw) {
    final decoded = jsonDecode(raw) as Map<String, dynamic>;
    if (decoded['format'] != 'vibestack-atlas-backup' || decoded['version'] is! num) throw const FormatException('Invalid backup format');
    final code = decoded['language']?.toString() ?? 'en';
    return BackupData(
      customItems: (decoded['customItems'] as List? ?? const []).map((item) => CatalogItem.fromJson(item as Map<String, dynamic>)).toList(),
      favorites: Set<String>.from(decoded['favorites'] as List? ?? const []),
      language: _languageFromCode(code),
      feedUrl: decoded['feedUrl']?.toString() ?? '',
      settings: Map<String, bool>.from(decoded['settings'] as Map? ?? const {}),
    );
  }
}
