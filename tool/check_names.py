#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Answer 'is this already in the catalog?' before a new pack is written.

Reads the last built assets/catalog.json rather than re-running the generator,
so it stays usable while a pack is mid-edit and the build would fail.
Usage: python3 tool/check_names.py "Redis" "Bubble Tea" ...
       python3 tool/check_names.py --file some_pack.py   (checks every name in it)
"""
import ast
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def known():
    """Both lookups: name -> id, and the set of ids already taken.

    Names and ids collide independently — an entry can reuse an existing id
    while spelling its name differently (or vice versa), and the build rejects
    either. Checking only one of them lets the other through to the build.
    """
    data = json.loads((ROOT / 'assets' / 'catalog.json').read_text(encoding='utf-8'))
    by_name = {item['name'].strip().lower(): item['id'] for item in data['items']}
    ids = {item['id'] for item in data['items']}
    return by_name, ids


def rows_in_pack(path):
    """(id, name) of every row in every list literal in the pack."""
    tree = ast.parse(pathlib.Path(path).read_text(encoding='utf-8'))
    out = []
    for node in ast.walk(tree):
        if isinstance(node, (ast.Tuple, ast.List)) and len(getattr(node, 'elts', [])) >= 2:
            first, second = node.elts[0], node.elts[1]
            if isinstance(first, ast.Constant) and isinstance(second, ast.Constant) \
                    and isinstance(first.value, str) and isinstance(second.value, str):
                out.append((first.value, second.value))
    return out


def main(argv):
    by_name, taken_ids = known()
    rows = rows_in_pack(argv[1]) if argv[:1] == ['--file'] else [(None, q) for q in argv]

    # A pack that already built is present in catalog.json, so every row matches
    # itself. Only a match pointing at a *different* id is a real collision.
    hits = []
    for iid, name in rows:
        owner = by_name.get(name.strip().lower())
        if owner is not None and owner != iid:
            hits.append(f'TAKEN name  {name}  ->  {owner}')
        elif owner is None and iid in taken_ids:
            hits.append(f'TAKEN id    {iid}  ({name})')
    for line in hits:
        print(line)
    print(f'{len(rows) - len(hits)}/{len(rows)} entr(ies) available')
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
