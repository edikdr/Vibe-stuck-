# -*- coding: utf-8 -*-
"""Builds the SQLite catalog the application ships and updates.

The database is read-only for the client: an update replaces the whole file
atomically, so nothing the user owns (favourites, custom entries, settings)
may ever live in here.
"""
from __future__ import annotations

import hashlib
import json
import pathlib
import sqlite3

from .model import CATALOG_SCHEMA_VERSION, Catalog, normalize

SCHEMA = """
PRAGMA journal_mode = DELETE;
PRAGMA foreign_keys = ON;

CREATE TABLE meta (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE groups (
    id       TEXT PRIMARY KEY,
    position INTEGER NOT NULL,
    label_en TEXT NOT NULL,
    label_ru TEXT NOT NULL,
    label_de TEXT NOT NULL
);

CREATE TABLE categories (
    name     TEXT PRIMARY KEY,
    group_id TEXT NOT NULL REFERENCES groups(id),
    label_en TEXT NOT NULL,
    label_ru TEXT NOT NULL,
    label_de TEXT NOT NULL
);

CREATE TABLE items (
    id                 TEXT PRIMARY KEY,
    name               TEXT NOT NULL,
    kind               TEXT NOT NULL,
    category           TEXT NOT NULL REFERENCES categories(name),
    category_group     TEXT NOT NULL REFERENCES groups(id),
    access             TEXT NOT NULL,
    url                TEXT NOT NULL,
    install            TEXT NOT NULL DEFAULT '',
    compatibility      TEXT NOT NULL DEFAULT 'both',
    tags               TEXT NOT NULL DEFAULT '',
    summary_en         TEXT NOT NULL DEFAULT '',
    summary_ru         TEXT NOT NULL DEFAULT '',
    tip_en             TEXT NOT NULL DEFAULT '',
    tip_ru             TEXT NOT NULL DEFAULT '',
    verified_at        TEXT NOT NULL,
    source             TEXT NOT NULL DEFAULT 'curated',
    pricing_model      TEXT NOT NULL,
    pricing_from       TEXT NOT NULL DEFAULT '',
    pricing_free_quota TEXT NOT NULL DEFAULT '',
    pricing_url        TEXT NOT NULL DEFAULT '',
    pricing_currency   TEXT NOT NULL DEFAULT '',
    pricing_checked_at TEXT NOT NULL DEFAULT '',
    -- Normalized once here so the client never runs the normalizing regex
    -- over every entry on every keystroke.
    norm_name          TEXT NOT NULL DEFAULT '',
    norm_category      TEXT NOT NULL DEFAULT '',
    norm_tags          TEXT NOT NULL DEFAULT '',
    norm_text          TEXT NOT NULL DEFAULT '',
    sort_index         INTEGER NOT NULL
);

CREATE INDEX idx_items_kind     ON items(kind);
CREATE INDEX idx_items_group    ON items(category_group);
CREATE INDEX idx_items_category ON items(category);
CREATE INDEX idx_items_access   ON items(access);

CREATE TABLE showcases (
    item_id  TEXT NOT NULL REFERENCES items(id),
    position INTEGER NOT NULL,
    title    TEXT NOT NULL,
    url      TEXT NOT NULL,
    kind     TEXT NOT NULL,
    note_en  TEXT NOT NULL DEFAULT '',
    note_ru  TEXT NOT NULL DEFAULT '',
    image    TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (item_id, position)
);
"""


def content_digest(catalog: Catalog) -> str:
    """Fingerprint of everything the database is supposed to contain.

    Stored in `meta` and recomputed by the verifier, so "is the committed
    database a build of the committed sources?" is answerable without
    rebuilding — and without depending on two machines producing byte-identical
    SQLite files, which different library versions do not guarantee.
    """
    digest = hashlib.sha256()
    payload = {
        'schema': CATALOG_SCHEMA_VERSION,
        'groups': catalog.taxonomy.groups,
        'categories': catalog.taxonomy.categories,
        'items': [
            {
                'id': item.id, 'name': item.name, 'kind': item.kind,
                'category': item.category, 'group': item.group,
                'access': item.access, 'url': item.url, 'install': item.install,
                'compatibility': item.compatibility, 'tags': item.tags,
                'summary': item.summary, 'tip': item.tip,
                'verifiedAt': item.verified_at, 'source': item.source,
                'pricing': item.pricing,
                'showcases': [
                    [s.title, s.url, s.kind, s.note, s.image] for s in item.showcases
                ],
            }
            for item in catalog.items
        ],
    }
    digest.update(json.dumps(payload, ensure_ascii=False, sort_keys=True).encode('utf-8'))
    return digest.hexdigest()


def build_database(catalog: Catalog, path: pathlib.Path, *, built_at: str) -> pathlib.Path:
    """Write `catalog` to `path`, replacing whatever was there."""
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        path.unlink()

    connection = sqlite3.connect(path)
    try:
        connection.executescript(SCHEMA)
        _fill(connection, catalog, built_at=built_at)
        connection.commit()
        # Rebuilding into a fresh file keeps the shipped database free of the
        # free pages an insert-heavy build leaves behind.
        connection.execute('VACUUM')
        connection.commit()
    finally:
        connection.close()
    return path


def _fill(connection: sqlite3.Connection, catalog: Catalog, *, built_at: str) -> None:
    connection.executemany(
        'INSERT INTO meta(key, value) VALUES (?, ?)',
        [
            ('schemaVersion', str(CATALOG_SCHEMA_VERSION)),
            ('catalogVersion', catalog.version),
            ('verifiedAt', catalog.verified_at),
            ('builtAt', built_at),
            ('itemCount', str(len(catalog.items))),
            ('contentHash', content_digest(catalog)),
        ],
    )
    connection.executemany(
        'INSERT INTO groups(id, position, label_en, label_ru, label_de) VALUES (?, ?, ?, ?, ?)',
        [
            (group['id'], position, group['labels']['en'], group['labels']['ru'], group['labels']['de'])
            for position, group in enumerate(catalog.taxonomy.groups)
        ],
    )
    connection.executemany(
        'INSERT INTO categories(name, group_id, label_en, label_ru, label_de) VALUES (?, ?, ?, ?, ?)',
        [
            (entry['name'], entry['group'],
             entry['labels']['en'], entry['labels']['ru'], entry['labels']['de'])
            for entry in catalog.taxonomy.categories
        ],
    )

    item_rows, showcase_rows = [], []
    for sort_index, item in enumerate(catalog.items):
        tags = ','.join(item.tags)
        item_rows.append((
            item.id, item.name, item.kind, item.category, item.group,
            item.access, item.url, item.install, ','.join(item.compatibility), tags,
            item.summary.get('en', ''), item.summary.get('ru', ''),
            item.tip.get('en', ''), item.tip.get('ru', ''),
            item.verified_at, item.source,
            item.pricing.get('model', ''), item.pricing.get('from', ''),
            item.pricing.get('freeQuota', ''), item.pricing.get('url', ''),
            item.pricing.get('currency', ''), item.pricing.get('checkedAt', ''),
            normalize(item.name), normalize(item.category),
            # Normalized per tag and rejoined, so the client can still tell one
            # tag from the next: normalizing the joined string would not.
            ','.join(normalize(tag) for tag in item.tags),
            normalize(item.search_haystack),
            sort_index,
        ))
        for position, showcase in enumerate(item.showcases):
            showcase_rows.append((
                item.id, position, showcase.title, showcase.url, showcase.kind,
                showcase.note.get('en', ''), showcase.note.get('ru', ''), showcase.image,
            ))

    connection.executemany(
        'INSERT INTO items VALUES (' + ', '.join(['?'] * 27) + ')', item_rows)
    connection.executemany(
        'INSERT INTO showcases VALUES (?, ?, ?, ?, ?, ?, ?, ?)', showcase_rows)


def read_meta(path: pathlib.Path) -> dict[str, str]:
    """Metadata of a built database, for verification and release notes."""
    connection = sqlite3.connect(f'file:{path}?mode=ro', uri=True)
    try:
        return dict(connection.execute('SELECT key, value FROM meta').fetchall())
    finally:
        connection.close()
