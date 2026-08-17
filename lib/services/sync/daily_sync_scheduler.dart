import 'dart:async';

import 'package:flutter/foundation.dart';

import '../../platform/platform_info.dart';
import 'background_worker.dart';

/// Owns the "update once a day" promise.
///
/// Layer 1 — Android: WorkManager wakes the app up even when it is closed.
/// Layer 2 — every platform: a timer inside the running app fires at the due
///            time, and a catch-up run happens on launch and on resume.
/// Layer 3 — desktop: an OS task (Task Scheduler / systemd timer) calls the
///            binary with `--headless-sync`; templates live in tool/desktop.
///
/// Layer 2 alone is what most apps call "auto update"; it only works while the
/// app is open. Layers 1 and 3 are what make the daily promise real.
class DailySyncScheduler {
  DailySyncScheduler({BackgroundWorker? worker, Future<void> Function()? onDue})
      : _worker = worker ?? const BackgroundWorker(),
        _onDue = onDue;

  final BackgroundWorker _worker;
  Future<void> Function()? _onDue;

  Timer? _timer;
  BackgroundMode _mode = BackgroundMode.disabled;
  int _hour = 9;
  int _minute = 0;
  bool _enabled = true;

  BackgroundMode get mode => _mode;
  int get hour => _hour;
  int get minute => _minute;
  bool get enabled => _enabled;

  set onDue(Future<void> Function()? callback) => _onDue = callback;

  /// Next moment the update is expected to run, in local time.
  DateTime get nextRun => nextRunAfter(DateTime.now());

  DateTime nextRunAfter(DateTime now) {
    final today = DateTime(now.year, now.month, now.day, _hour, _minute);
    return today.isAfter(now) ? today : today.add(const Duration(days: 1));
  }

  /// Called once at start-up and again whenever the user changes the schedule.
  Future<void> configure({
    required bool enabled,
    required int hour,
    required int minute,
  }) async {
    _enabled = enabled;
    _hour = hour.clamp(0, 23);
    _minute = minute.clamp(0, 59);
    _timer?.cancel();
    _timer = null;

    if (!enabled) {
      _mode = BackgroundMode.disabled;
      await _worker.cancel();
      return;
    }

    var scheduled = false;
    if (_worker.isSupported) {
      final ready = await _worker.initialize();
      if (ready) {
        scheduled = await _worker.schedule(
          initialDelay: nextRun.difference(DateTime.now()),
        );
      }
    }
    _mode = scheduled ? BackgroundMode.systemScheduler : BackgroundMode.inApp;
    _startInAppTimer();
  }

  /// The in-app timer runs in every mode: WorkManager is allowed to shift a
  /// periodic job by a long window, so a foreground app should not wait for it.
  void _startInAppTimer() {
    _timer?.cancel();
    final delay = nextRun.difference(DateTime.now());
    _timer = Timer(delay.isNegative ? Duration.zero : delay, () async {
      await _fire();
      _startInAppTimer();
    });
  }

  Future<void> _fire() async {
    final callback = _onDue;
    if (callback == null) return;
    try {
      await callback();
    } catch (error) {
      debugPrint('[atlas] scheduled sync failed: $error');
    }
  }

  /// Catch-up used on launch and on app resume: if the app was closed at the
  /// scheduled moment, run as soon as it is open again.
  bool isOverdue(DateTime? lastSync, {DateTime? now}) {
    if (!_enabled) return false;
    final moment = now ?? DateTime.now();
    if (lastSync == null) return true;
    return moment.difference(lastSync).inHours >= 24;
  }

  String describeMode() => switch (_mode) {
        BackgroundMode.systemScheduler => 'system',
        BackgroundMode.inApp => PlatformInfo.isDesktop ? 'in-app + optional system task' : 'in-app',
        BackgroundMode.disabled => 'disabled',
      };

  void dispose() {
    _timer?.cancel();
    _timer = null;
  }
}
