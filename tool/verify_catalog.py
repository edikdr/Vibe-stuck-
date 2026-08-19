#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Checks the catalog contract without a Flutter SDK.

Validates the JSONL sources, then — if `assets/catalog.db` exists — checks that
the built database still matches them, so a stale artifact cannot pass review:

    python3 tool/verify_catalog.py
"""
from __future__ import annotations

import pathlib
import sqlite3
import sys
from collections import Counter

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from catalog import CATALOG_SCHEMA_VERSION, CatalogError, load_catalog  # noqa: E402
from catalog.database import content_digest  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
DATABASE = ROOT / 'assets' / 'catalog.db'
GENERATED_DART = ROOT / 'lib' / 'i18n' / 'categories.dart'


def check_database(catalog) -> list[str]:
    """The shipped database must be a build of the sources as they are now."""
    problems: list[str] = []
    connection = sqlite3.connect(f'file:{DATABASE}?mode=ro', uri=True)
    try:
        meta = dict(connection.execute('SELECT key, value FROM meta').fetchall())
        if meta.get('schemaVersion') != str(CATALOG_SCHEMA_VERSION):
            problems.append(
                f'database schemaVersion is {meta.get("schemaVersion")}, '
                f'sources are {CATALOG_SCHEMA_VERSION} — rebuild')
        if meta.get('catalogVersion') != catalog.version:
            problems.append(
                f'database catalogVersion is {meta.get("catalogVersion")}, '
                f'sources are {catalog.version} — rebuild')

        # The one check that makes the rest redundant, and the one CI relies
        # on: the committed database is a build of the committed sources.
        expected = content_digest(catalog)
        if meta.get('contentHash') != expected:
            problems.append(
                'assets/catalog.db does not match data/catalog — '
                'run: python3 tool/build_catalog.py')

        stored = connection.execute('SELECT id FROM items').fetchall()
        stored_ids = {row[0] for row in stored}
        source_ids = {item.id for item in catalog.items}
        for missing in sorted(source_ids - stored_ids)[:10]:
            problems.append(f'{missing}: in the sources but not in the database — rebuild')
        for extra in sorted(stored_ids - source_ids)[:10]:
            problems.append(f'{extra}: in the database but not in the sources — rebuild')

        cards = connection.execute('SELECT count(*) FROM showcases').fetchone()[0]
        expected_cards = sum(len(item.showcases) for item in catalog.items)
        if cards != expected_cards:
            problems.append(f'database has {cards} showcase cards, sources have {expected_cards}')
    finally:
        connection.close()
    return problems


def check_generated_dart(catalog) -> list[str]:
    if not GENERATED_DART.exists():
        return ['lib/i18n/categories.dart is missing — run tool/build_catalog.py']
    text = GENERATED_DART.read_text(encoding='utf-8')
    return [
        f'{entry["name"]!r} has no label in the generated Dart — rebuild'
        for entry in catalog.taxonomy.categories
        if f"'{entry['name']}'" not in text
    ]


def main() -> int:
    try:
        catalog = load_catalog(ROOT)
    except CatalogError as error:
        print(f'{len(error.problems)} problem(s) in data/catalog:', file=sys.stderr)
        for problem in error.problems:
            print(f'  - {problem}', file=sys.stderr)
        return 1

    items = catalog.items
    print(f'entries      {len(items)}')
    print(f'by kind      {dict(Counter(item.kind for item in items))}')
    print(f'by group     {dict(Counter(item.group for item in items))}')
    print(f'by access    {dict(Counter(item.access for item in items))}')
    print(f'categories   {len({item.category for item in items})}')
    print(f'with price   {sum(1 for item in items if item.pricing.get("from"))}')
    print(f'showcases    {sum(len(item.showcases) for item in items)} cards '
          f'on {sum(1 for item in items if item.showcases)} entries')

    problems = check_generated_dart(catalog)
    if DATABASE.exists():
        problems += check_database(catalog)
    else:
        print('\nnote: assets/catalog.db not built yet, database checks skipped')

    if problems:
        print(f'\n{len(problems)} problem(s):', file=sys.stderr)
        for problem in problems:
            print(f'  - {problem}', file=sys.stderr)
        return 1
    print('\nok — catalog matches the integrity contract')
    return 0


if __name__ == '__main__':
    sys.exit(main())
