# -*- coding: utf-8 -*-
"""Catalog toolchain: load JSONL sources, build SQLite, verify the contract.

The repository stores catalog data as newline-delimited JSON so that a change
to one entry is one readable line in a diff. Everything the application ships
is derived from it by `tool/build_catalog.py`; nothing generated is edited by
hand and nothing generated is committed except the artifacts listed in
`tool/catalog/build.py`.
"""

from .model import (
    CATALOG_SCHEMA_VERSION,
    CatalogError,
    Item,
    Taxonomy,
    load_catalog,
    normalize,
)

__all__ = [
    'CATALOG_SCHEMA_VERSION',
    'CatalogError',
    'Item',
    'Taxonomy',
    'load_catalog',
    'normalize',
]
