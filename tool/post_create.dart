import 'dart:io';

/// Applies the project settings that `flutter create` cannot know about.
///
/// Run it once after generating the android/ windows/ linux/ folders:
///   dart run tool/post_create.dart
///
/// Everything here is idempotent, so running it again after a Flutter upgrade
/// is safe.
void main() {
  _patchAndroidManifest();
  _patchWindowsTitle();
  _patchLinuxTitle();
  _patchMacosTitle();
  _removeGeneratedTemplateTest();
  stdout.writeln('post_create: done');
}

/// `flutter create` writes its counter-app smoke test whenever the file is
/// absent, and that test fails against this app. Left in place it breaks
/// `flutter test` in CI, where the runner directories are generated on every
/// run, so the generated file is removed rather than committed.
void _removeGeneratedTemplateTest() {
  final generated = File('test/widget_test.dart');
  if (!generated.existsSync()) return;
  if (!generated.readAsStringSync().contains('Counter increments smoke test')) {
    stdout.writeln('post_create: test/widget_test.dart is not the template, keeping it');
    return;
  }
  generated.deleteSync();
  stdout.writeln('post_create: removed generated template test');
}

void _patchAndroidManifest() {
  final manifest = File('android/app/src/main/AndroidManifest.xml');
  if (!manifest.existsSync()) {
    stdout.writeln('post_create: no AndroidManifest.xml, skipping');
    return;
  }
  var value = manifest.readAsStringSync();
  const anchor = '<manifest xmlns:android="http://schemas.android.com/apk/res/android">';

  // INTERNET  — catalog sync and preview images.
  // WAKE_LOCK + RECEIVE_BOOT_COMPLETED — WorkManager needs both to keep the
  // daily job alive across reboots; without them the job silently stops after
  // the first restart of the phone.
  const permissions = <String>[
    'android.permission.INTERNET',
    'android.permission.WAKE_LOCK',
    'android.permission.RECEIVE_BOOT_COMPLETED',
  ];

  final missing = permissions.where((name) => !value.contains(name)).toList();
  if (missing.isNotEmpty && value.contains(anchor)) {
    final lines = missing.map((name) => '    <uses-permission android:name="$name" />').join('\n');
    value = value.replaceFirst(anchor, '$anchor\n$lines');
  }

  value = value.replaceAll('android:label="vibestack_atlas"', 'android:label="VibeStack Atlas"');
  manifest.writeAsStringSync(value);
  stdout.writeln('post_create: android manifest patched (${missing.length} permissions added)');
}

void _patchWindowsTitle() {
  final main = File('windows/runner/main.cpp');
  if (!main.existsSync()) return;
  var value = main.readAsStringSync();
  value = value.replaceAll('L"vibestack_atlas"', 'L"VibeStack Atlas"');
  // A catalog with a detail pane needs room: 1280x820 puts the app straight
  // into the master-detail breakpoint instead of the phone layout.
  value = value.replaceAll('Win32Window::Size size(1280, 720);', 'Win32Window::Size size(1280, 820);');
  main.writeAsStringSync(value);
  stdout.writeln('post_create: windows runner patched');
}

void _patchMacosTitle() {
  // Product name lives in the xcconfig; the window geometry lives in Swift.
  final config = File('macos/Runner/Configs/AppInfo.xcconfig');
  if (config.existsSync()) {
    var value = config.readAsStringSync();
    value = value.replaceAll('PRODUCT_NAME = vibestack_atlas', 'PRODUCT_NAME = VibeStack Atlas');
    config.writeAsStringSync(value);
  }

  final window = File('macos/Runner/MainFlutterWindow.swift');
  if (!window.existsSync()) {
    stdout.writeln('post_create: no macOS runner, skipping');
    return;
  }
  var value = window.readAsStringSync();
  // flutter create restores a frame captured from the storyboard, so the size
  // has to be set *after* that call or it is immediately overwritten. This puts
  // the app straight into the master-detail breakpoint, as on Windows and Linux.
  const anchor = 'self.setFrame(windowFrame, display: true)';
  if (!value.contains('NSSize(width: 1280') && value.contains(anchor)) {
    value = value.replaceFirst(
      anchor,
      '$anchor\n    self.setContentSize(NSSize(width: 1280, height: 820))',
    );
  }
  window.writeAsStringSync(value);
  stdout.writeln('post_create: macos runner patched');
}

void _patchLinuxTitle() {
  final window = File('linux/runner/my_application.cc');
  if (!window.existsSync()) return;
  var value = window.readAsStringSync();
  value = value.replaceAll('"vibestack_atlas"', '"VibeStack Atlas"');
  value = value.replaceAll('gtk_window_set_default_size(window, 1280, 720);',
      'gtk_window_set_default_size(window, 1280, 820);');
  window.writeAsStringSync(value);
  stdout.writeln('post_create: linux runner patched');
}
