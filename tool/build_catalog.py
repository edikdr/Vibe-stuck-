#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Builds every generated catalog artifact from `data/catalog/*.jsonl`.

    python3 tool/build_catalog.py            # database + generated Dart
    python3 tool/build_catalog.py --release  # also dist/catalog.db.gz + manifest

Outputs:
    assets/catalog.db        shipped inside the app, seeds first run
    lib/i18n/categories.dart compiled category and group labels
    lib/app_info.dart        app version, bundled catalog version and schema
    dist/catalog.db.gz       release asset, downloaded by an installed app
    dist/manifest.json       what the app reads before deciding to download
"""
from __future__ import annotations

import argparse
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from catalog import CatalogError, load_catalog  # noqa: E402
from catalog.database import build_database  # noqa: E402
from catalog.appinfo import write_app_info  # noqa: E402
from catalog.dartgen import write_categories  # noqa: E402
from catalog.release import (  # noqa: E402
    ARCHIVE_NAME, MANIFEST_NAME, MIN_APP_VERSION, compress, write_manifest)

ROOT = pathlib.Path(__file__).resolve().parent.parent


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', action='store_true',
                        help='also write dist/catalog.db.gz and dist/manifest.json')
    parser.add_argument('--download-url', default='',
                        help='URL the manifest points at for the archive')
    parser.add_argument('--min-app-version', default=MIN_APP_VERSION,
                        help='oldest app release able to read this catalog')
    parser.add_argument('--signature', default='',
                        help='detached signature to record in the manifest')
    args = parser.parse_args(argv)

    try:
        catalog = load_catalog(ROOT)
    except CatalogError as error:
        print(f'{len(error.problems)} problem(s) in data/catalog:', file=sys.stderr)
        for problem in error.problems:
            print(f'  - {problem}', file=sys.stderr)
        return 1

    database = build_database(
        catalog, ROOT / 'assets' / 'catalog.db', built_at=catalog.verified_at)
    write_categories(catalog, ROOT / 'lib' / 'i18n' / 'categories.dart')
    write_app_info(catalog, ROOT)

    print(f'entries        {len(catalog.items)}')
    print(f'catalogVersion {catalog.version}')
    print(f'database       {database.relative_to(ROOT)} '
          f'({database.stat().st_size / 1_048_576:.2f} MB)')

    if args.release:
        dist = ROOT / 'dist'
        archive = compress(database, dist / ARCHIVE_NAME)
        manifest = write_manifest(
            catalog,
            database=database,
            archive=archive,
            target=dist / MANIFEST_NAME,
            min_app_version=args.min_app_version,
            download_url=args.download_url,
            signature=args.signature,
        )
        print(f'archive        dist/{ARCHIVE_NAME} '
              f'({manifest["size"] / 1_048_576:.2f} MB, sha256 {manifest["sha256"][:12]}…)')
        print(f'manifest       dist/{MANIFEST_NAME}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
