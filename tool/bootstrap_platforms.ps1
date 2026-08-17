# Windows equivalent of bootstrap_platforms.sh
$ErrorActionPreference = "Stop"

flutter create --project-name vibestack_atlas --platforms=android,windows,linux,macos .
dart run tool/post_create.dart
flutter pub get
flutter analyze
flutter test

Write-Host ""
Write-Host "Next steps:"
Write-Host "  flutter build windows --release"
Write-Host "  powershell -File tool\desktop\register_task_windows.ps1 -ExePath <path to exe>"
