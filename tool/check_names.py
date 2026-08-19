#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Answer 'is this already in the catalog?' before a new pack is built.

Reads the pack sources rather than the built catalog.json, so the answer does
not depend on whether the pack under test has been built yet — a pack that was
already built would otherwise match every one of its own rows against itself.

Usage: python3 tool/check_names.py "Redis" "Bubble Tea" ...
       python3 tool/check_names.py --file catalog_src/data_libs_5x.py
"""
import ast
import pathlib
import sys

TOOL = pathlib.Path(__file__).resolve().parent
SRC = TOOL / 'catalog_src'


def merged_ids():
    """paid-* rows listed in build_catalog's MERGE are pricing sidecars.

    They are folded onto an existing entry instead of becoming items of their
    own, so sharing that entry's name is the intended arrangement, not a clash.
    Parsed out of the source rather than imported, to avoid running a build.
    """
    tree = ast.parse((TOOL / 'build_catalog.py').read_text(encoding='utf-8'))
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign) and isinstance(node.value, ast.Dict) \
                and any(getattr(t, 'id', None) == 'MERGE' for t in node.targets):
            return {k.value for k in node.value.keys if isinstance(k, ast.Constant)}
    return set()


def rows_in_pack(path):
    """(id, name) of every entry row in a pack file.

    Entry rows are tuples whose first two elements are string literals; the
    module docstring and any helper constants have no such shape.
    """
    tree = ast.parse(pathlib.Path(path).read_text(encoding='utf-8'))
    rows = []
    for node in ast.walk(tree):
        elts = getattr(node, 'elts', None)
        if isinstance(node, (ast.Tuple, ast.List)) and elts and len(elts) >= 2:
            first, second = elts[0], elts[1]
            if isinstance(first, ast.Constant) and isinstance(second, ast.Constant) \
                    and isinstance(first.value, str) and isinstance(second.value, str):
                rows.append((first.value, second.value))
    return rows


def catalog_excluding(pack):
    """Every (id, name) defined by the packs other than `pack`."""
    skip = pathlib.Path(pack).resolve() if pack else None
    sidecars = merged_ids()
    by_name, ids = {}, set()
    for path in sorted(SRC.glob('data_*.py')):
        if skip and path.resolve() == skip:
            continue
        for iid, name in rows_in_pack(path):
            if iid in sidecars:
                continue
            by_name.setdefault(name.strip().lower(), iid)
            ids.add(iid)
    return by_name, ids


def main(argv):
    pack = argv[1] if argv[:1] == ['--file'] else None
    rows = rows_in_pack(pack) if pack else [(None, q) for q in argv]
    by_name, taken_ids = catalog_excluding(pack)

    sidecars = merged_ids()
    hits = []
    seen_here = {}
    for iid, name in rows:
        if iid in sidecars:
            continue
        key = name.strip().lower()
        if key in by_name:
            hits.append(f'TAKEN name  {name}  ->  {by_name[key]}')
        elif iid is not None and iid in taken_ids:
            hits.append(f'TAKEN id    {iid}  ({name})')
        elif key in seen_here:
            hits.append(f'DUPE  name  {name}  (twice in this pack)')
        else:
            seen_here[key] = iid
    for line in hits:
        print(line)
    checked = len(seen_here) + len(hits)
    print(f'{checked - len(hits)}/{checked} entr(ies) available')
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
