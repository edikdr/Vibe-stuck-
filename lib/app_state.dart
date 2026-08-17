import 'dart:async';

import 'package:flutter/widgets.dart';

import 'i18n/app_strings.dart';
import 'models/catalog_item.dart';
import 'services/ai_context.dart';
import 'services/catalog_repository.dart';
import 'services/image_cache_service.dart';
import 'services/live_update_service.dart';
import 'services/search_engine.dart';
import 'services/showcase_resolver.dart';
import 'services/sync/background_worker.dart';
import 'services/sync/daily_sync_scheduler.dart';

class AppState extends ChangeNotifier {
  AppState({
    CatalogRepository? repository,
    SearchEngine? searchEngine,
    LiveUpdateService? liveUpdateService,
    DailySyncScheduler? scheduler,
    ImageCacheService? imageCache,
  })  : _repository = repository ?? CatalogRepository(),
        _searchEngine = searchEngine ?? SearchEngine(),
        _liveUpdateService = liveUpdateService ?? LiveUpdateService(),
        _scheduler = scheduler ?? DailySyncScheduler(),
        imageCache = imageCache ?? ImageCacheService() {
    _scheduler.onDue = () => syncCatalog(silent: true);
  }

  final CatalogRepository _repository;
  final SearchEngine _searchEngine;
  final LiveUpdateService _liveUpdateService;
  final DailySyncScheduler _scheduler;
  final ImageCacheService imageCache;
  final ShowcaseResolver showcases = const ShowcaseResolver();
  final AiContextBuilder aiContext = const AiContextBuilder();
  final List<CatalogItem> _bundled = [];
  final List<CatalogItem> _feed = [];
  final List<CatalogItem> _live = [];
  final List<CatalogItem> _custom = [];
  final Set<String> _favorites = {};
  final Map<String, String> _versions = {};
  final Map<String, bool> _linkHealth = {};
  final List<UpdateEvent> _events = [];

  AppLanguage language = AppLanguage.en;
  String query = '';
  ItemKind? kindFilter;
  AccessType? accessFilter;
  String compatibilityFilter = 'all';
  String categoryFilter = 'all';
  String groupFilter = 'all';
  String priceFilter = 'any';
  bool onlyExamples = false;
  bool onlyLive = false;
  bool onlyFavorites = false;
  String sortMode = 'relevance';
  String? selectedItemId;
  int syncHour = 9;
  int syncMinute = 0;
  PreviewProvider previewProvider = PreviewProvider.mshots;
  String backgroundResult = '';
  String feedUrl = '';
  bool autoSync = true;
  bool syncMcp = true;
  bool syncSkills = true;
  bool syncVersions = true;
  bool syncLinks = true;
  bool syncing = false;
  DateTime? lastSync;
  String? syncMessage;
  List<String> syncErrors = [];

  AppStrings get s => AppStrings(language);

  List<CatalogItem> get allItems {
    final merged = <String, CatalogItem>{};
    for (final source in [_bundled, _live, _feed, _custom]) {
      for (final item in source) {
        merged[item.id] = item;
      }
    }
    return merged.values.toList(growable: false);
  }

  List<CatalogItem> get visibleItems => _searchEngine.search(
        allItems,
        query,
        kind: kindFilter,
        access: accessFilter,
        compatibility: compatibilityFilter,
        category: categoryFilter,
        group: groupFilter,
        price: priceFilter,
        onlyExamples: onlyExamples,
        onlyLive: onlyLive,
        onlyFavorites: onlyFavorites,
        favorites: _favorites,
        sort: sortMode,
      );

  int get activeFilterCount => [
        kindFilter != null,
        accessFilter != null,
        compatibilityFilter != 'all',
        categoryFilter != 'all',
        groupFilter != 'all',
        priceFilter != 'any',
        onlyExamples,
        onlyLive,
        onlyFavorites,
      ].where((value) => value).length;

  CatalogItem? get selectedItem {
    if (selectedItemId == null) return null;
    for (final item in allItems) {
      if (item.id == selectedItemId) return item;
    }
    return null;
  }

  /// Categories that still have at least one match under the current section.
  List<String> get categoriesInScope {
    final source = groupFilter == 'all'
        ? allItems
        : allItems.where((item) => item.categoryGroup == groupFilter);
    return (source.map((item) => item.category).toSet().toList()..sort());
  }

  Map<String, int> get countByGroup {
    final counts = <String, int>{};
    for (final item in allItems) {
      counts[item.categoryGroup] = (counts[item.categoryGroup] ?? 0) + 1;
    }
    return counts;
  }

  BackgroundMode get backgroundMode => _scheduler.mode;
  DateTime get nextScheduledSync => _scheduler.nextRun;

  List<Showcase> examplesFor(CatalogItem item, String officialLabel) =>
      showcases.examplesFor(item, officialLabel: officialLabel);

  String previewUrlFor(Showcase showcase) =>
      showcases.imageUrl(showcase, previewProvider);

  /// Whether card previews can produce an image at all. Grid layouts ask this
  /// before reserving vertical space: with previews off, a card that reserves
  /// room for an image it will never load is just a hole in the list.
  bool get previewsEnabled => previewProvider != PreviewProvider.off;

  List<CatalogItem> get favoriteItems => allItems.where((item) => _favorites.contains(item.id)).toList(growable: false);
  List<CatalogItem> get customItems => List.unmodifiable(_custom);
  List<UpdateEvent> get events => List.unmodifiable(_events);
  Set<String> get categories => allItems.map((item) => item.category).toSet();
  bool isFavorite(String id) => _favorites.contains(id);
  String? versionFor(String id) => _versions[id];
  bool? healthFor(String id) => _linkHealth[id];

  Future<void> load() async {
    final results = await Future.wait<Object?>([
      _repository.loadBundled(),
      _repository.loadFeedItems(),
      _repository.loadLiveItems(),
      _repository.loadCustom(),
      _repository.loadFavorites(),
      _repository.loadFeedUrl(),
      _repository.loadAutoSync(),
      _repository.loadLastSync(),
      _repository.loadLanguage(),
      _repository.loadVersions(),
      _repository.loadLinkHealth(),
      _repository.loadEvents(),
      _repository.loadSyncMcp(),
      _repository.loadSyncSkills(),
      _repository.loadSyncVersions(),
      _repository.loadSyncLinks(),
      _repository.loadSyncHour(),
      _repository.loadSyncMinute(),
      _repository.loadPreviewProvider(),
      _repository.loadSortMode(),
      _repository.loadBackgroundResult(),
    ]);
    _bundled
      ..clear()
      ..addAll(results[0]! as List<CatalogItem>);
    _feed
      ..clear()
      ..addAll(results[1]! as List<CatalogItem>);
    _live
      ..clear()
      ..addAll(results[2]! as List<CatalogItem>);
    _custom
      ..clear()
      ..addAll(results[3]! as List<CatalogItem>);
    _favorites
      ..clear()
      ..addAll(results[4]! as Set<String>);
    feedUrl = results[5]! as String;
    autoSync = results[6]! as bool;
    lastSync = results[7] as DateTime?;
    language = results[8]! as AppLanguage;
    _versions
      ..clear()
      ..addAll(results[9]! as Map<String, String>);
    _linkHealth
      ..clear()
      ..addAll(results[10]! as Map<String, bool>);
    _events
      ..clear()
      ..addAll(results[11]! as List<UpdateEvent>);
    syncMcp = results[12]! as bool;
    syncSkills = results[13]! as bool;
    syncVersions = results[14]! as bool;
    syncLinks = results[15]! as bool;
    syncHour = results[16]! as int;
    syncMinute = results[17]! as int;
    previewProvider = PreviewProviderInfo.fromCode(results[18]! as String);
    sortMode = results[19]! as String;
    backgroundResult = results[20]! as String;
    notifyListeners();

    await _scheduler.configure(enabled: autoSync, hour: syncHour, minute: syncMinute);
    if (_scheduler.isOverdue(lastSync)) unawaited(syncCatalog(silent: true));
  }

  /// Called when the app returns to the foreground: a background run may have
  /// written new entries while the UI was not listening.
  Future<void> handleResume() async {
    final storedSync = await _repository.loadLastSync();
    final storedResult = await _repository.loadBackgroundResult();
    final changed = storedSync != null && (lastSync == null || storedSync.isAfter(lastSync!));
    if (changed) {
      _live
        ..clear()
        ..addAll(await _repository.loadLiveItems());
      _events
        ..clear()
        ..addAll(await _repository.loadEvents());
      lastSync = storedSync;
      backgroundResult = storedResult;
      notifyListeners();
    }
    if (_scheduler.isOverdue(lastSync)) unawaited(syncCatalog(silent: true));
  }

  void setGroup(String value) {
    groupFilter = value;
    if (value != 'all' && categoryFilter != 'all' && !categoriesInScope.contains(categoryFilter)) {
      categoryFilter = 'all';
    }
    notifyListeners();
  }

  void setPrice(String value) {
    priceFilter = value;
    notifyListeners();
  }

  void toggleOnlyExamples([bool? value]) {
    onlyExamples = value ?? !onlyExamples;
    notifyListeners();
  }

  void toggleOnlyLive([bool? value]) {
    onlyLive = value ?? !onlyLive;
    notifyListeners();
  }

  void toggleOnlyFavorites([bool? value]) {
    onlyFavorites = value ?? !onlyFavorites;
    notifyListeners();
  }

  Future<void> setSort(String value) async {
    sortMode = value;
    notifyListeners();
    await _repository.saveSortMode(value);
  }

  void select(String? id) {
    selectedItemId = id;
    notifyListeners();
  }

  Future<void> setSyncTime(int hour, int minute) async {
    syncHour = hour;
    syncMinute = minute;
    notifyListeners();
    await Future.wait([_repository.saveSyncHour(hour), _repository.saveSyncMinute(minute)]);
    await _scheduler.configure(enabled: autoSync, hour: hour, minute: minute);
    notifyListeners();
  }

  Future<void> setPreviewProvider(PreviewProvider value) async {
    previewProvider = value;
    notifyListeners();
    await _repository.savePreviewProvider(value.code);
  }

  Future<void> clearImageCache() => imageCache.clear();

  String contextForItem(CatalogItem item) =>
      aiContext.buildItem(item, languageCode: language.code);

  String contextForVisible() =>
      aiContext.buildList(visibleItems, languageCode: language.code);

  void setQuery(String value) {
    query = value;
    notifyListeners();
  }

  void setKind(ItemKind? value) {
    kindFilter = value;
    notifyListeners();
  }

  void setAccess(AccessType? value) {
    accessFilter = value;
    notifyListeners();
  }

  void setCompatibility(String value) {
    compatibilityFilter = value;
    notifyListeners();
  }

  void setCategory(String value) {
    categoryFilter = value;
    notifyListeners();
  }

  void clearFilters() {
    kindFilter = null;
    accessFilter = null;
    compatibilityFilter = 'all';
    categoryFilter = 'all';
    groupFilter = 'all';
    priceFilter = 'any';
    onlyExamples = false;
    onlyLive = false;
    onlyFavorites = false;
    notifyListeners();
  }

  Future<void> setLanguage(AppLanguage value) async {
    language = value;
    notifyListeners();
    await _repository.saveLanguage(value);
  }

  Future<void> toggleFavorite(String id) async {
    if (!_favorites.add(id)) _favorites.remove(id);
    notifyListeners();
    await _repository.saveFavorites(_favorites);
  }

  Future<void> addCustom(CatalogItem item) async {
    _custom.removeWhere((existing) => existing.id == item.id);
    _custom.add(item);
    notifyListeners();
    await _repository.saveCustom(_custom);
  }

  Future<void> removeCustom(String id) async {
    _custom.removeWhere((item) => item.id == id);
    _favorites.remove(id);
    notifyListeners();
    await Future.wait([_repository.saveCustom(_custom), _repository.saveFavorites(_favorites)]);
  }

  Future<void> updateFeedUrl(String value) async {
    feedUrl = value.trim();
    notifyListeners();
    await _repository.saveFeedUrl(feedUrl);
  }

  Future<void> updateAutoSync(bool value) async {
    autoSync = value;
    notifyListeners();
    await _repository.saveAutoSync(value);
    await _scheduler.configure(enabled: value, hour: syncHour, minute: syncMinute);
    notifyListeners();
  }

  Future<void> updateSyncMcp(bool value) async {
    syncMcp = value;
    notifyListeners();
    await _repository.saveSyncMcp(value);
  }

  Future<void> updateSyncSkills(bool value) async {
    syncSkills = value;
    notifyListeners();
    await _repository.saveSyncSkills(value);
  }

  Future<void> updateSyncVersions(bool value) async {
    syncVersions = value;
    notifyListeners();
    await _repository.saveSyncVersions(value);
  }

  Future<void> updateSyncLinks(bool value) async {
    syncLinks = value;
    notifyListeners();
    await _repository.saveSyncLinks(value);
  }

  Future<void> syncCatalog({bool silent = false}) async {
    if (syncing) return;
    syncing = true;
    if (!silent) syncMessage = null;
    syncErrors = [];
    notifyListeners();
    var feedCount = 0;
    try {
      if (feedUrl.isNotEmpty) {
        final feedResult = await _repository.syncFeed(feedUrl);
        if (feedResult.changed) {
          _feed
            ..clear()
            ..addAll(await _repository.loadFeedItems());
          feedCount = feedResult.count;
        } else if (feedResult.message.isNotEmpty) {
          syncErrors.add(feedResult.message);
        }
      }

      final live = await _liveUpdateService.sync(
        existing: allItems,
        currentVersions: _versions,
        lastSync: lastSync,
        syncMcp: syncMcp,
        syncSkills: syncSkills,
        syncVersions: syncVersions,
        syncLinks: syncLinks,
      );
      final mergedLive = <String, CatalogItem>{for (final item in _live) item.id: item};
      for (final item in live.items) {
        mergedLive[item.id] = item;
      }
      _live
        ..clear()
        ..addAll(mergedLive.values);
      _versions.addAll(live.versions);
      _linkHealth.addAll(live.linkHealth);
      _events.insertAll(0, live.events.reversed);
      if (_events.length > 150) _events.removeRange(150, _events.length);
      syncErrors.addAll(live.errors);
      lastSync = DateTime.now();
      syncMessage = s.t('liveUpdateSummary', {
        'added': live.addedCount + feedCount,
        'versions': live.versions.length,
        'links': live.linkHealth.length,
      });
      await Future.wait([
        _repository.saveLiveItems(_live),
        _repository.saveVersions(_versions),
        _repository.saveLinkHealth(_linkHealth),
        _repository.saveEvents(_events),
        _repository.saveLastSync(lastSync!),
      ]);
    } catch (error) {
      syncMessage = s.t('syncFailed', {'error': error.runtimeType});
    } finally {
      syncing = false;
      notifyListeners();
    }
  }

  String createBackup() => _repository.createBackup(
        customItems: _custom,
        favorites: _favorites,
        language: language,
        feedUrl: feedUrl,
        settings: {
          'autoSync': autoSync,
          'syncMcp': syncMcp,
          'syncSkills': syncSkills,
          'syncVersions': syncVersions,
          'syncLinks': syncLinks,
        },
      );

  Future<void> importBackup(String raw) async {
    final data = _repository.parseBackup(raw);
    _custom
      ..clear()
      ..addAll(data.customItems);
    _favorites
      ..clear()
      ..addAll(data.favorites);
    language = data.language;
    feedUrl = data.feedUrl;
    autoSync = data.settings['autoSync'] ?? true;
    syncMcp = data.settings['syncMcp'] ?? true;
    syncSkills = data.settings['syncSkills'] ?? true;
    syncVersions = data.settings['syncVersions'] ?? true;
    syncLinks = data.settings['syncLinks'] ?? true;
    notifyListeners();
    await Future.wait([
      _repository.saveCustom(_custom),
      _repository.saveFavorites(_favorites),
      _repository.saveLanguage(language),
      _repository.saveFeedUrl(feedUrl),
      _repository.saveAutoSync(autoSync),
      _repository.saveSyncMcp(syncMcp),
      _repository.saveSyncSkills(syncSkills),
      _repository.saveSyncVersions(syncVersions),
      _repository.saveSyncLinks(syncLinks),
    ]);
  }

  @override
  void dispose() {
    _scheduler.dispose();
    super.dispose();
  }

  Future<void> clearEvents() async {
    _events.clear();
    notifyListeners();
    await _repository.saveEvents(_events);
  }
}

class AppScope extends InheritedNotifier<AppState> {
  const AppScope({required AppState state, required super.child, super.key}) : super(notifier: state);

  static AppState of(BuildContext context) {
    final scope = context.dependOnInheritedWidgetOfExactType<AppScope>();
    assert(scope != null, 'AppScope not found');
    return scope!.notifier!;
  }
}
