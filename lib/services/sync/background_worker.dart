import 'package:flutter/foundation.dart';
import 'package:workmanager/workmanager.dart';

import '../../platform/platform_info.dart';
import 'sync_runner.dart';

/// How the daily update is actually being executed right now.
enum BackgroundMode {
  /// The operating system wakes the app up (Android WorkManager).
  systemScheduler,

  /// Only an in-app timer plus a catch-up run on launch and resume.
  inApp,

  /// Nothing scheduled: the user turned auto-update off.
  disabled,
}

/// Unique work name. Registering the same name twice replaces the old work
/// instead of stacking a second daily job.
const backgroundTaskName = 'vibestack-atlas-daily-sync';

/// Entry point executed inside a separate isolate by WorkManager.
///
/// It must be a top level function annotated with `vm:entry-point`, otherwise
/// tree shaking removes it from release builds and the daily job silently
/// stops working.
@pragma('vm:entry-point')
void backgroundCallbackDispatcher() {
  Workmanager().executeTask((task, inputData) async {
    try {
      final result = await SyncRunner().run();
      debugPrint('[atlas] background sync: $result');
      return true;
    } catch (error) {
      debugPrint('[atlas] background sync failed: $error');
      // Returning true prevents WorkManager from retrying immediately on a
      // dead network; the next daily run will pick the update up anyway.
      return true;
    }
  });
}

/// Thin wrapper over the platform scheduler.
///
/// Every call is defensive: if the plugin is missing (desktop) or its API
/// changed, the app degrades to the in-app timer instead of crashing at start.
class BackgroundWorker {
  const BackgroundWorker();

  bool get isSupported => PlatformInfo.hasBackgroundWorker;

  Future<bool> initialize() async {
    if (!isSupported) return false;
    try {
      await Workmanager().initialize(backgroundCallbackDispatcher);
      return true;
    } catch (error) {
      debugPrint('[atlas] background worker unavailable: $error');
      return false;
    }
  }

  /// Registers a repeating daily job. [initialDelay] is used to line the first
  /// run up with the hour the user picked.
  Future<bool> schedule({required Duration initialDelay}) async {
    if (!isSupported) return false;
    try {
      await Workmanager().cancelByUniqueName(backgroundTaskName);
      await Workmanager().registerPeriodicTask(
        backgroundTaskName,
        'dailySync',
        frequency: const Duration(hours: 24),
        initialDelay: initialDelay.isNegative ? Duration.zero : initialDelay,
      );
      return true;
    } catch (error) {
      debugPrint('[atlas] could not schedule background sync: $error');
      return false;
    }
  }

  Future<void> cancel() async {
    if (!isSupported) return;
    try {
      await Workmanager().cancelByUniqueName(backgroundTaskName);
    } catch (_) {
      // Nothing scheduled is an acceptable outcome of cancelling.
    }
  }
}
