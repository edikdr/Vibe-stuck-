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
  stdout.writeln('post_create: done');
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
