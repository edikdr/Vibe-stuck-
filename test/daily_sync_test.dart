import 'package:flutter_test/flutter_test.dart';
import 'package:vibestack_atlas/services/sync/background_worker.dart';
import 'package:vibestack_atlas/services/sync/daily_sync_scheduler.dart';

/// A worker stub so the scheduling rules can be tested without WorkManager,
/// which is exactly what happens on desktop at runtime.
class _FakeWorker implements BackgroundWorker {
  _FakeWorker({this.supported = false, this.canSchedule = true});

  final bool supported;
  final bool canSchedule;
  Duration? lastDelay;
  int cancelCount = 0;

  @override
  bool get isSupported => supported;

  @override
  Future<bool> initialize() async => supported;

  @override
  Future<bool> schedule({required Duration initialDelay}) async {
    lastDelay = initialDelay;
    return canSchedule;
  }

  @override
  Future<void> cancel() async => cancelCount++;
}

void main() {
  test('next run is today when the hour is still ahead', () async {
    final scheduler = DailySyncScheduler(worker: _FakeWorker());
    await scheduler.configure(enabled: true, hour: 23, minute: 30);
    final now = DateTime(2026, 8, 16, 9, 0);
    expect(scheduler.nextRunAfter(now), DateTime(2026, 8, 16, 23, 30));
    scheduler.dispose();
  });

  test('next run rolls over to tomorrow once the hour has passed', () async {
    final scheduler = DailySyncScheduler(worker: _FakeWorker());
    await scheduler.configure(enabled: true, hour: 9, minute: 0);
    final now = DateTime(2026, 8, 16, 9, 1);
    expect(scheduler.nextRunAfter(now), DateTime(2026, 8, 17, 9, 0));
    scheduler.dispose();
  });

  test('a run is overdue after 24 hours and never before', () async {
    final scheduler = DailySyncScheduler(worker: _FakeWorker());
    await scheduler.configure(enabled: true, hour: 9, minute: 0);
    final now = DateTime(2026, 8, 16, 12, 0);
    expect(scheduler.isOverdue(null, now: now), isTrue);
    expect(scheduler.isOverdue(now.subtract(const Duration(hours: 3)), now: now), isFalse);
    expect(scheduler.isOverdue(now.subtract(const Duration(hours: 30)), now: now), isTrue);
    scheduler.dispose();
  });

  test('falls back to the in-app timer when no system worker is available', () async {
    final worker = _FakeWorker(supported: false);
    final scheduler = DailySyncScheduler(worker: worker);
    await scheduler.configure(enabled: true, hour: 7, minute: 15);
    expect(scheduler.mode, BackgroundMode.inApp);
    scheduler.dispose();
  });

  test('uses the system scheduler when the platform grants it', () async {
    final worker = _FakeWorker(supported: true);
    final scheduler = DailySyncScheduler(worker: worker);
    await scheduler.configure(enabled: true, hour: 7, minute: 15);
    expect(scheduler.mode, BackgroundMode.systemScheduler);
    expect(worker.lastDelay, isNotNull);
    expect(worker.lastDelay!.isNegative, isFalse);
    scheduler.dispose();
  });

  /// The degradation path the README promises: the plugin exists and starts,
  /// but refuses the periodic job. The app must fall back rather than claim a
  /// background worker it does not have.
  test('degrades to the in-app timer when the system job is refused', () async {
    final worker = _FakeWorker(supported: true, canSchedule: false);
    final scheduler = DailySyncScheduler(worker: worker);
    await scheduler.configure(enabled: true, hour: 7, minute: 15);
    expect(scheduler.mode, BackgroundMode.inApp);
    expect(worker.lastDelay, isNotNull);
    scheduler.dispose();
  });

  test('turning auto update off cancels the system job', () async {
    final worker = _FakeWorker(supported: true);
    final scheduler = DailySyncScheduler(worker: worker);
    await scheduler.configure(enabled: false, hour: 9, minute: 0);
    expect(scheduler.mode, BackgroundMode.disabled);
    expect(worker.cancelCount, 1);
    expect(scheduler.isOverdue(null), isFalse);
    scheduler.dispose();
  });
}
