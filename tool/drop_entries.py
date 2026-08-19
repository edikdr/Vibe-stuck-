#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Remove whole entry tuples from a pack file by id.

Packs are hand-written tuples spanning several lines, so a plain sed cannot
delete one cleanly. Usage: python3 tool/drop_entries.py <pack.py> <id> [id ...]
"""
import pathlib
import re
import sys


def drop(path, ids):
    lines = pathlib.Path(path).read_text(encoding='utf-8').split('\n')
    kept, removed, i = [], [], 0
    while i < len(lines):
        head = re.match(r'\s*\("([A-Za-z0-9._-]+)"\s*,', lines[i])
        if head and head.group(1) in ids:
            removed.append(head.group(1))
            while i < len(lines) and not lines[i].rstrip().endswith('),'):
                i += 1
            i += 1
            continue
        kept.append(lines[i])
        i += 1
    pathlib.Path(path).write_text('\n'.join(kept), encoding='utf-8')
    return removed


if __name__ == '__main__':
    gone = drop(sys.argv[1], set(sys.argv[2:]))
    print(f'removed {len(gone)}: {", ".join(gone) or "nothing"}')
