#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Checks the issue forms in .github/ISSUE_TEMPLATE against the form schema.

A template is only validated by the forge once it is on the default branch,
and a broken one is reported there as "invalid templates were ignored" — long
after the pull request that introduced it was merged. This catches it before.

The failure that motivated this: an unquoted option containing ": " parses as
a YAML mapping rather than a string, so the file is valid YAML and an invalid
issue form. Loading it proves nothing; the shape has to be checked.

    python3 tool/check_templates.py
"""
from __future__ import annotations

import pathlib
import sys

import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent
TEMPLATES = ROOT / '.github' / 'ISSUE_TEMPLATE'
BLOCK_TYPES = {'markdown', 'input', 'textarea', 'dropdown', 'checkboxes'}
NEEDS_ID = {'input', 'textarea', 'dropdown', 'checkboxes'}


def check(path: pathlib.Path) -> list[str]:
    problems: list[str] = []
    where = path.relative_to(ROOT).as_posix()
    try:
        document = yaml.safe_load(path.read_text(encoding='utf-8'))
    except yaml.YAMLError as error:
        return [f'{where}: not valid YAML ({error.__class__.__name__})']

    if not isinstance(document, dict):
        return [f'{where}: top level must be a mapping']
    for key in ('name', 'description', 'body'):
        if key not in document:
            problems.append(f'{where}: missing {key}')

    for index, block in enumerate(document.get('body') or []):
        kind = block.get('type')
        label = f'{where}: body[{index}]({kind})'
        if kind not in BLOCK_TYPES:
            problems.append(f'{label}: unknown type')
            continue
        if kind in NEEDS_ID and not block.get('id'):
            problems.append(f'{label}: has no id')

        attributes = block.get('attributes') or {}
        if kind != 'markdown' and not attributes.get('label'):
            problems.append(f'{label}: has no label')

        options = attributes.get('options') or []
        if kind == 'dropdown':
            if not options:
                problems.append(f'{label}: has no options')
            for position, option in enumerate(options):
                if not isinstance(option, str):
                    problems.append(
                        f'{label}, option[{position}]: should be a string, got '
                        f'{type(option).__name__} — quote it if it contains ": "')
        if kind == 'checkboxes':
            for position, option in enumerate(options):
                if not isinstance(option, dict) or not isinstance(option.get('label'), str):
                    problems.append(f'{label}, option[{position}]: needs a string label')
    return problems


def main() -> int:
    files = sorted(TEMPLATES.glob('*.yml')) + sorted(TEMPLATES.glob('*.yaml'))
    if not files:
        print(f'no issue forms in {TEMPLATES.relative_to(ROOT).as_posix()}')
        return 0

    problems: list[str] = []
    for path in files:
        found = check(path)
        problems += found
        print(f'{"fail" if found else "ok  "}  {path.relative_to(ROOT).as_posix()}')

    if problems:
        print(f'\n{len(problems)} problem(s):', file=sys.stderr)
        for problem in problems:
            print(f'  - {problem}', file=sys.stderr)
        return 1
    print('\nok — every issue form matches the schema')
    return 0


if __name__ == '__main__':
    sys.exit(main())
