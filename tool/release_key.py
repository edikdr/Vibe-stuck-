#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generates and inspects the key that signs catalog releases.

    python3 tool/release_key.py generate     # new keypair, once, ever
    python3 tool/release_key.py show         # what the app currently trusts

Turning signing on is three steps and cannot be half-done safely:

    1. `generate` prints a private key and a public key;
    2. put the private key in the release secret `CATALOG_SIGNING_KEY`;
    3. write the public key to data/release-key.pub and commit it.

Order matters. The public key is compiled into the app, and an app that has
one refuses any unsigned catalog — so publishing the public key before the
release job can sign would stop updates for everyone who installs that build.
"""
from __future__ import annotations

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from catalog.signing import generate_keypair, public_key_of  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
PUBLIC_KEY_FILE = ROOT / 'data' / 'release-key.pub'


def generate() -> int:
    private, public = generate_keypair()
    print('Private key — the release secret CATALOG_SIGNING_KEY.')
    print('Store it now; it is not written anywhere and cannot be recovered.\n')
    print(f'  {private}\n')
    print('Public key — write it to data/release-key.pub and commit it,')
    print('but only once the release job can actually sign:\n')
    print(f'  {public}\n')
    print(f'  echo {public} > {PUBLIC_KEY_FILE.relative_to(ROOT).as_posix()}')
    return 0


def show() -> int:
    if not PUBLIC_KEY_FILE.exists():
        print('No data/release-key.pub.')
        print('Releases are unsigned; the app verifies SHA-256 and trusts HTTPS'
              ' to the forge. See SECURITY.md.')
        return 0
    key = PUBLIC_KEY_FILE.read_text(encoding='utf-8').strip()
    print(f'Trusted release key: {key}')
    print('Apps built from this commit refuse a catalog that is not signed by it.')
    return 0


def check(private_key: str) -> int:
    """Confirms a private key matches the committed public key."""
    if not PUBLIC_KEY_FILE.exists():
        print('No data/release-key.pub to check against', file=sys.stderr)
        return 1
    expected = PUBLIC_KEY_FILE.read_text(encoding='utf-8').strip()
    actual = public_key_of(private_key)
    if actual != expected:
        print(f'key mismatch: secret holds {actual}, repository trusts {expected}',
              file=sys.stderr)
        return 1
    print('ok — the signing secret matches data/release-key.pub')
    return 0


def main(argv: list[str]) -> int:
    command = argv[0] if argv else 'show'
    if command == 'generate':
        return generate()
    if command == 'show':
        return show()
    if command == 'check':
        if len(argv) < 2:
            print('usage: release_key.py check <private-key-base64>', file=sys.stderr)
            return 2
        return check(argv[1])
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
