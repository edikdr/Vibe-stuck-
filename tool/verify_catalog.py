# -*- coding: utf-8 -*-
"""Checks assets/catalog.json against the same contract as catalog_integrity_test.dart.

The Dart test is the source of truth in CI, but it needs a Flutter SDK. This
script needs only Python, so a catalog change can be verified anywhere the
generator itself runs:

    python3 tool/build_catalog.py && python3 tool/verify_catalog.py
"""
import json
import pathlib
import re
import sys
import unicodedata
from collections import Counter

ROOT = pathlib.Path(__file__).resolve().parent.parent

KINDS = {'api', 'library', 'mcp', 'skill'}
ACCESSES = {'noKey', 'freeTier', 'openSource', 'free', 'mixed', 'paid'}
SHOWCASE_KINDS = {'gallery', 'site', 'demo', 'code'}
DATE = re.compile(r'^\d{4}-\d{2}-\d{2}$')
ID = re.compile(r'^[a-z0-9-]+$')

# Mirrors the expectations in test/catalog_integrity_test.dart.
MIN_ITEMS = 380
MIN_SHOWCASE_CARDS = 90
MIN_PER_KIND = 20

# Ids are internal, but one copied from a neighbouring entry while writing a
# pack (`lib-rambda` holding Ramda) silently blocks the real product's id later.
# Abbreviations and acronyms are legitimate, so only flag an id that shares no
# word at all with its own name, and keep a list of the deliberate ones.
ID_NAME_EXEMPT = {
    'api-fcm', 'api-fec', 'api-fmp', 'api-hibp', 'api-nws', 'api-qrserver',
    'api-dnd5e', 'api-x-twitter', 'lib-wandb', 'mcp-wandb', 'mcp-azure',
    'lib-tflite', 'lib-tgi', 'lib-msgpack', 'lib-protobuf', 'lib-opa',
    'lib-pyarrow', 'lib-kmp', 'lib-msw', 'lib-cva', 'lib-mui', 'lib-popmotion',
    'lib-json-schema', 'lib-bundle-analyzer', 'lib-asyncio-tools',
    'skill-use-examples', 'lib-aif360', 'lib-pyg',
}


def words(text):
    plain = unicodedata.normalize('NFKD', text.lower())
    return {w for w in re.split(r'[^a-z0-9]+', plain) if len(w) > 2}


def id_matches_name(iid, name):
    """True when the id slug and the name share a word, or one extends the other."""
    slug = words(re.sub(r'^(api|lib|mcp|skill|paid)-', '', iid))
    label = words(name)
    if not slug or not label:
        return True
    return bool(slug & label) or any(
        a.startswith(b) or b.startswith(a) for a in slug for b in label)


def main():
    problems = []
    catalog = json.loads((ROOT / 'assets/catalog.json').read_text(encoding='utf-8'))
    items = catalog['items']
    categories = json.loads((ROOT / 'assets/catalog.schema.json').read_text(encoding='utf-8'))
    assert categories  # schema must at least parse

    if catalog.get('schemaVersion') != 3:
        problems.append(f"schemaVersion is {catalog.get('schemaVersion')}, expected 3")
    if len(items) < MIN_ITEMS:
        problems.append(f'only {len(items)} items, the Dart test requires >= {MIN_ITEMS}')

    # ---- generated Dart labels must cover every category in use -----------
    dart = (ROOT / 'lib/i18n/categories.dart').read_text(encoding='utf-8')
    labelled = set(re.findall(r"^  '([^']+)': \{AppLanguage\.en", dart, re.M))
    grouped = set(re.findall(r"^  '([^']+)': '(?:ai|frontend|backend|devtools|data|media|skills)',", dart, re.M))

    seen_ids, seen_names, showcase_cards = set(), set(), 0
    for item in items:
        iid = item.get('id', '(no id)')
        if iid in seen_ids:
            problems.append(f'duplicate id: {iid}')
        seen_ids.add(iid)

        name = item.get('name', '').lower()
        if name in seen_names:
            problems.append(f'duplicate name: {name}')
        seen_names.add(name)

        if not ID.match(iid):
            problems.append(f'bad id format: {iid}')
        if not item.get('url', '').startswith('https://'):
            problems.append(f'{iid}: url is not https')
        if not item.get('summary', '').strip():
            problems.append(f'{iid}: empty summary')
        if not item.get('tip', '').strip():
            problems.append(f'{iid}: empty tip')
        if not item.get('tags'):
            problems.append(f'{iid}: no tags')
        if not DATE.match(item.get('verifiedAt', '')):
            problems.append(f'{iid}: verifiedAt is not YYYY-MM-DD')
        if item.get('kind') not in KINDS:
            problems.append(f"{iid}: bad kind {item.get('kind')}")
        if item.get('access') not in ACCESSES:
            problems.append(f"{iid}: bad access {item.get('access')}")
        if not item.get('categoryGroup'):
            problems.append(f'{iid}: no categoryGroup')

        category = item.get('category', '')
        if category not in labelled:
            problems.append(f'{iid}: category has no translated label: {category}')
        if category not in grouped:
            problems.append(f'{iid}: category has no group mapping: {category}')

        for showcase in item.get('showcases', []):
            showcase_cards += 1
            if not showcase.get('title', '').strip():
                problems.append(f'{iid}: showcase without a title')
            if not showcase.get('url', '').startswith('https://'):
                problems.append(f'{iid}: showcase url is not https')
            if showcase.get('kind') not in SHOWCASE_KINDS:
                problems.append(f"{iid}: bad showcase kind {showcase.get('kind')}")

        if iid not in ID_NAME_EXEMPT and not id_matches_name(iid, name):
            problems.append(f'{iid}: id does not match its name {name!r}')

        pricing = item.get('pricing') or {}
        if item.get('access') == 'paid':
            if not pricing.get('from', '').strip():
                problems.append(f'{iid}: paid entry without a price')
            if pricing.get('model') == 'free':
                problems.append(f'{iid}: paid entry priced as free')
        if pricing.get('from') and not pricing.get('url'):
            problems.append(f'{iid}: price without a pricing link')

    if showcase_cards < MIN_SHOWCASE_CARDS:
        problems.append(f'only {showcase_cards} showcase cards, need >= {MIN_SHOWCASE_CARDS}')

    by_kind = Counter(item['kind'] for item in items)
    for kind in KINDS:
        if by_kind[kind] <= MIN_PER_KIND:
            problems.append(f'too few entries of kind {kind}: {by_kind[kind]}')

    # ---- report -----------------------------------------------------------
    print(f'items        {len(items)}')
    print(f'by kind      {dict(by_kind)}')
    print(f'by group     {dict(Counter(i["categoryGroup"] for i in items))}')
    print(f'by access    {dict(Counter(i["access"] for i in items))}')
    print(f'categories   {len({i["category"] for i in items})}')
    print(f'with price   {sum(1 for i in items if (i.get("pricing") or {}).get("from"))}')
    print(f'showcases    {showcase_cards} cards on {sum(1 for i in items if i.get("showcases"))} entries')

    if problems:
        print(f'\n{len(problems)} problem(s):', file=sys.stderr)
        for problem in problems:
            print(f'  - {problem}', file=sys.stderr)
        sys.exit(1)
    print('\nok — catalog matches the integrity contract')


main()
