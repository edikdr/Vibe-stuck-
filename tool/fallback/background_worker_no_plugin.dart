// Fallback version of lib/services/sync/background_worker.dart with no
// workmanager dependency.
//
// Use it if `flutter pub get` cannot resolve workmanager, or if the plugin
// breaks against a newer Flutter/AGP version and you need a build today:
//
//   cp tool/fallback/background_worker_no_plugin.dart \
//      lib/services/sync/background_worker.dart
//   # then delete the `workmanager:` line from pubspec.yaml
//   flutter pub get && flutter analyze && flutter test
//
// What you lose: layer 1 only. The app can no longer wake itself up on Android
// while it is closed. Layer 2 (in-app timer plus catch-up on launch and resume)
// and layer 3 (the desktop system task calling `--headless-sync`) keep working
// untouched, and the Updates screen honestly reports "in-app" instead of
// "system", because DailySyncScheduler reads isSupported from here.
//
// The public API is identical to the real file, so nothing else in the project
// needs to change.

import 'package:flutter/foundation.dart';

/// How the daily update is actually being executed right now.
enum BackgroundMode {
  /// The operating system wakes the app up (Android WorkManager).
  systemScheduler,

  /// Only an in-app timer plus a catch-up run on launch and resume.
  inApp,

  /// Nothing scheduled: the user turned auto-update off.
  disabled,
}

/// Kept so the constant stays referable from tests and logs.
const backgroundTaskName = 'vibestack-atlas-daily-sync';

/// No-op entry point. The real file registers this with WorkManager; here it
/// exists only so imports of this symbol keep compiling.
@pragma('vm:entry-point')
void backgroundCallbackDispatcher() {
  debugPrint('[atlas] background worker disabled in this build');
}

/// Always reports "unsupported", which makes [DailySyncScheduler] fall back to
/// the in-app timer without any other code change.
class BackgroundWorker {
  const BackgroundWorker();

  bool get isSupported => false;

  Future<bool> initialize() async => false;

  Future<bool> schedule({required Duration initialDelay}) async => false;

  Future<void> cancel() async {}
}
