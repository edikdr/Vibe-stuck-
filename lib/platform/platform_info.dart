import 'dart:io' show Platform;

import 'package:flutter/foundation.dart';

/// Single place that answers "where am I running" so the rest of the app
/// never touches dart:io Platform directly. Desktop work (window sizing,
/// tray icon, system scheduler, file paths) branches from here.
class PlatformInfo {
  const PlatformInfo._();

  static bool get isWeb => kIsWeb;
  static bool get isAndroid => !kIsWeb && Platform.isAndroid;
  static bool get isIOS => !kIsWeb && Platform.isIOS;
  static bool get isWindows => !kIsWeb && Platform.isWindows;
  static bool get isLinux => !kIsWeb && Platform.isLinux;
  static bool get isMacOS => !kIsWeb && Platform.isMacOS;

  static bool get isMobile => isAndroid || isIOS;
  static bool get isDesktop => isWindows || isLinux || isMacOS;

  /// Android WorkManager can wake the app while it is closed.
  /// Desktop needs an OS level task (Task Scheduler / systemd timer) instead.
  static bool get hasBackgroundWorker => isAndroid;

  /// Desktop can run a headless sync from the command line, which is what the
  /// generated system task calls.
  static bool get supportsHeadlessRun => isDesktop;

  static bool get supportsKeyboardShortcuts => isDesktop || isWeb;

  static String get label => switch (true) {
        _ when isAndroid => 'Android',
        _ when isIOS => 'iOS',
        _ when isWindows => 'Windows',
        _ when isLinux => 'Linux',
        _ when isMacOS => 'macOS',
        _ when isWeb => 'Web',
        _ => 'Unknown',
      };
}

/// Layout breakpoints shared by every screen so mobile and desktop stay
/// consistent while the desktop rework lands.
class Breakpoints {
  const Breakpoints._();

  static const compact = 620.0;
  static const medium = 900.0;
  static const expanded = 1180.0;
  static const wide = 1480.0;

  static bool isCompact(double width) => width < compact;
  static bool isMedium(double width) => width >= compact && width < expanded;

  /// Wide enough for the master–detail layout instead of a modal sheet.
  static bool isMasterDetail(double width) => width >= expanded;

  static int gridColumns(double width) {
    if (width >= wide) return 4;
    if (width >= expanded) return 3;
    if (width >= medium) return 3;
    if (width >= compact) return 2;
    return 1;
  }
}
