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
    data = json.loads((ROOT / 'assets' / 'catalog.json').read_text(encoding='utf-8'))
    return {item['name'].strip().lower(): item['id'] for item in data['items']}


def names_in_pack(path):
    """Second tuple element of every row in every top-level list literal."""
    tree = ast.parse(pathlib.Path(path).read_text(encoding='utf-8'))
    out = []
    for node in ast.walk(tree):
        if isinstance(node, (ast.Tuple, ast.List)) and len(getattr(node, 'elts', [])) >= 2:
            second = node.elts[1]
            first = node.elts[0]
            if isinstance(first, ast.Constant) and isinstance(second, ast.Constant) \
                    and isinstance(first.value, str) and isinstance(second.value, str):
                out.append(second.value)
    return out


def main(argv):
    if argv[:1] == ['--file']:
        queries = names_in_pack(argv[1])
    else:
        queries = argv
    catalog = known()
    hits = [(q, catalog[q.strip().lower()]) for q in queries if q.strip().lower() in catalog]
    for name, iid in hits:
        print(f'TAKEN  {name}  ->  {iid}')
    free = len(queries) - len(hits)
    print(f'{free}/{len(queries)} name(s) available')
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
