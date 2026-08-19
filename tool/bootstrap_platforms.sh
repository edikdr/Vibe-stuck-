#!/usr/bin/env bash
set -euo pipefail

# Builds the catalog, generates the platform folders this repository
# intentionally does not track, applies the project specific patches and runs
# the full check suite.

# assets/catalog.db is a build artifact rather than a checked-in file, and
# nothing below works without it: it is the catalog.
python3 tool/build_catalog.py

flutter create --project-name vibestack_atlas --platforms=android,windows,linux,macos .
dart run tool/post_create.dart
flutter pub get
flutter analyze
flutter test

echo
echo "Next steps:"
echo "  flutter build apk --release            # Android"
echo "  flutter build windows --release        # Windows"
echo "  flutter build linux --release          # Linux"
echo "  see tool/desktop/README.md for the daily system task"
