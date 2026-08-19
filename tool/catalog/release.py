# -*- coding: utf-8 -*-
"""Release artifacts for the catalog: the compressed database and its manifest.

The client downloads `manifest.json` first, decides from it whether an update
is worth the traffic, and only then pulls `catalog.db.gz`. Every field the
client needs to make that decision without downloading anything else lives in
the manifest.
"""
from __future__ import annotations

import gzip
import hashlib
import json
import pathlib

from .model import CATALOG_SCHEMA_VERSION, Catalog

MANIFEST_NAME = 'manifest.json'
ARCHIVE_NAME = 'catalog.db.gz'

# Oldest app release able to read the current schema. Bump this only when
# CATALOG_SCHEMA_VERSION changes: an app is refused an update it could have
# read perfectly well if this tracks the current version instead.
MIN_APP_VERSION = '0.7.0'


def sha256_of(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def compress(database: pathlib.Path, target: pathlib.Path) -> pathlib.Path:
    """gzip the database. mtime=0 keeps the archive byte-identical per input."""
    target.parent.mkdir(parents=True, exist_ok=True)
    raw = database.read_bytes()
    with gzip.GzipFile(filename='', mode='wb', fileobj=target.open('wb'),
                       compresslevel=9, mtime=0) as handle:
        handle.write(raw)
    return target


def write_manifest(
    catalog: Catalog,
    *,
    database: pathlib.Path,
    archive: pathlib.Path,
    target: pathlib.Path,
    min_app_version: str = MIN_APP_VERSION,
    download_url: str = '',
    signature: str = '',
) -> dict:
    """Write the update manifest next to the archive and return it."""
    manifest = {
        'schemaVersion': CATALOG_SCHEMA_VERSION,
        'catalogVersion': catalog.version,
        'verifiedAt': catalog.verified_at,
        'itemCount': len(catalog.items),
        'file': archive.name,
        'url': download_url,
        'size': archive.stat().st_size,
        'sha256': sha256_of(archive),
        # Of the decompressed database, so the client can check what it is
        # about to install and not only what it downloaded.
        'databaseSize': database.stat().st_size,
        'databaseSha256': sha256_of(database),
        # A client older than this cannot read the schema and must not try.
        'minAppVersion': min_app_version,
        # Reserved: filled in by the release workflow when a signing key is
        # configured. The client verifies sha256 today and ignores this field.
        'signature': signature,
    }
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return manifest
