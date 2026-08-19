import 'package:flutter/widgets.dart';

import '../../app_info.dart';
import '../../models/catalog_item.dart';
import '../catalog_repository.dart';
import '../catalog_update_service.dart';
import '../live_update_service.dart';

/// Result of one headless run, small enough to store in preferences and show
/// in the Updates screen on the next launch.
class HeadlessSyncResult {
  const HeadlessSyncResult({
    required this.ran,
    this.added = 0,
    this.versions = 0,
    this.links = 0,
    this.stars = 0,
    this.reason = '',
    this.catalogVersion = '',
  });

  final bool ran;
  final int added;
  final int versions;
  final int links;
  final int stars;
  final String reason;

  /// Catalog version installed by this run, empty when nothing was installed.
  final String catalogVersion;

  @override
  String toString() => ran
      ? 'ok added=$added versions=$versions links=$links'
          '${catalogVersion.isEmpty ? '' : ' catalog=$catalogVersion'}'
      : 'skipped ($reason)';
}

/// Runs a catalog update with no UI attached.
///
/// Three callers share this code path:
///   * the Android background worker,
///   * a desktop system task started with `--headless-sync`,
///   * the in-app scheduler when the app happens to be open at the due time.
///
/// It only touches [CatalogRepository] and [LiveUpdateService], never widgets,
/// so it is safe inside a background isolate.
class SyncRunner {
  SyncRunner({CatalogRepository? repository, LiveUpdateService? liveUpdateService})
      : _repository = repository ?? CatalogRepository(),
        _liveUpdateService = liveUpdateService ?? LiveUpdateService();

  final CatalogRepository _repository;
  final LiveUpdateService _liveUpdateService;

  /// [force] ignores both the auto-update switch and the 20 hour guard.
  Future<HeadlessSyncResult> run({bool force = false}) async {
    WidgetsFlutterBinding.ensureInitialized();

    final autoSync = await _repository.loadAutoSync();
    if (!autoSync && !force) {
      return const HeadlessSyncResult(ran: false, reason: 'auto-sync disabled');
    }

    final lastSync = await _repository.loadLastSync();
    if (!force && lastSync != null && DateTime.now().difference(lastSync).inHours < 20) {
      return const HeadlessSyncResult(ran: false, reason: 'already fresh');
    }

    // A published catalog release is installed before anything else runs, so
    // the rest of the sync compares against the newest entries there are.
    var catalogVersion = '';
    try {
      final catalog = await CatalogUpdateService(database: _repository.catalog)
          .update(appVersion: appVersion);
      if (catalog.changed) catalogVersion = catalog.version;
    } catch (_) {
      // No network, or a release that cannot be read: the installed catalog
      // stays exactly as it was, which is the whole point of shipping one.
    }

    final existing = <CatalogItem>[];
    try {
      existing.addAll(await _repository.loadBundled());
    } catch (_) {
      // Opening the database can fail in a background isolate on some OEM
      // builds. Deduplication then falls back to persisted entries only.
    }
    final live = await _repository.loadLiveItems();
    existing
      ..addAll(live)
      ..addAll(await _repository.loadCustom())
      ..addAll(await _repository.loadFeedItems());

    final versions = await _repository.loadVersions();
    final health = await _repository.loadLinkHealth();
    final stars = await _repository.loadStars();
    final events = await _repository.loadEvents();

    final result = await _liveUpdateService.sync(
      existing: existing,
      currentVersions: versions,
      currentStars: stars,
      lastSync: lastSync,
      syncMcp: await _repository.loadSyncMcp(),
      syncSkills: await _repository.loadSyncSkills(),
      syncVersions: await _repository.loadSyncVersions(),
      syncLinks: await _repository.loadSyncLinks(),
      syncStars: await _repository.loadSyncStars(),
    );

    final merged = <String, CatalogItem>{for (final item in live) item.id: item};
    for (final item in result.items) {
      merged[item.id] = item;
    }
    versions.addAll(result.versions);
    health.addAll(result.linkHealth);
    stars.addAll(result.stars);
    events.insertAll(0, result.events.reversed);
    if (events.length > 150) events.removeRange(150, events.length);

    final now = DateTime.now();
    await _repository.saveLiveItems(merged.values.toList(growable: false));
    await _repository.saveVersions(versions);
    await _repository.saveLinkHealth(health);
    await _repository.saveStars(stars);
    await _repository.saveEvents(events);
    await _repository.saveLastSync(now);
    await _repository.saveBackgroundResult(
      'added=${result.addedCount};versions=${result.versions.length};'
      'links=${result.linkHealth.length};at=${now.toIso8601String()}',
    );

    return HeadlessSyncResult(
      ran: true,
      added: result.addedCount,
      versions: result.versions.length,
      links: result.linkHealth.length,
      stars: result.stars.length,
      catalogVersion: catalogVersion,
    );
  }
}
