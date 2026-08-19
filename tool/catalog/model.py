# -*- coding: utf-8 -*-
"""Loading and validation of the JSONL catalog sources in `data/catalog/`.

One entry is one line of `data/catalog/<kind>.jsonl`; the file name carries the
kind, so a record never repeats it. Everything a client needs but no human
should have to keep in sync — the category group, the default pricing model,
the normalized search text — is derived here rather than stored.
"""
from __future__ import annotations

import json
import pathlib
import re
import unicodedata
from collections import Counter
from dataclasses import dataclass, field

# Bumped when the on-disk SQLite layout changes in a way an older client
# cannot read. The client refuses a downloaded database with a higher value.
CATALOG_SCHEMA_VERSION = 4

KIND_FILES = {
    'apis.jsonl': 'api',
    'libs.jsonl': 'library',
    'mcp.jsonl': 'mcp',
    'skills.jsonl': 'skill',
}

ACCESS_TYPES = {'noKey', 'freeTier', 'openSource', 'free', 'mixed', 'paid'}
SHOWCASE_KINDS = {'gallery', 'site', 'demo', 'code'}
PRICING_MODELS = {
    'free', 'freemium', 'openSource', 'usage', 'subscription',
    'hybrid', 'commission', 'credits', 'one-time', 'custom',
}
LANGUAGES = ('en', 'ru')

# Floors that catch a source file failing to load, not quality targets.
MIN_ITEMS = 2000
MIN_PER_KIND = 20
MIN_SHOWCASE_CARDS = 90

# Access type -> pricing model when an entry carries no explicit `pricing`.
DEFAULT_PRICING_MODEL = {
    'noKey': 'free',
    'free': 'free',
    'openSource': 'openSource',
    'freeTier': 'freemium',
    'mixed': 'freemium',
    'paid': 'usage',
}

# Ids whose name is an acronym or a rename, checked once and kept on purpose.
ID_NAME_EXEMPT = {
    'api-csm-ai', 'api-dnd5e', 'api-fcm', 'api-fec', 'api-fmp', 'api-hibp',
    'api-nws', 'api-qrserver', 'api-x-twitter', 'lib-aif360',
    'lib-asyncio-tools', 'lib-bundle-analyzer', 'lib-cva', 'lib-json-schema',
    'lib-kmp', 'lib-msgpack', 'lib-msw', 'lib-mturk', 'lib-mui', 'lib-opa',
    'lib-popmotion', 'lib-protobuf', 'lib-pyarrow', 'lib-pyg', 'lib-tflite',
    'lib-tgi', 'lib-wandb', 'mcp-azure', 'mcp-wandb', 'skill-ddia',
    'skill-semver', 'skill-use-examples',
}

_ID_RE = re.compile(r'^[a-z0-9-]+$')
# Kept character-for-character in step with SearchEngine._normalize in Dart:
# the database ships the normalized text so the client never runs this regex.
_NOISE_RE = re.compile(r'[^a-zäöüßа-я0-9+#.\- ]')
_SPACE_RE = re.compile(r'\s+')


class CatalogError(Exception):
    """Raised with every problem found, not just the first one."""

    def __init__(self, problems: list[str]):
        self.problems = problems
        super().__init__(f'{len(problems)} catalog problem(s)')


def normalize(value: str) -> str:
    """Lowercase, fold `ё`, drop punctuation, collapse whitespace."""
    folded = value.lower().replace('ё', 'е')
    return _SPACE_RE.sub(' ', _NOISE_RE.sub(' ', folded)).strip()


@dataclass(frozen=True)
class Showcase:
    title: str
    url: str
    kind: str
    note: dict[str, str]
    image: str = ''


@dataclass
class Item:
    id: str
    name: str
    kind: str
    category: str
    access: str
    url: str
    tags: list[str]
    summary: dict[str, str]
    tip: dict[str, str]
    verified_at: str
    install: str = ''
    compatibility: list[str] = field(default_factory=lambda: ['both'])
    source: str = 'curated'
    pricing: dict[str, str] = field(default_factory=dict)
    showcases: list[Showcase] = field(default_factory=list)
    group: str = ''

    @property
    def search_haystack(self) -> str:
        """Everything the in-app search matches against, before normalizing."""
        parts = [
            self.name, self.kind, self.category,
            self.summary.get('en', ''), self.summary.get('ru', ''),
            self.access, *self.compatibility, *self.tags,
            self.tip.get('en', ''), self.tip.get('ru', ''),
        ]
        return ' '.join(part for part in parts if part)


@dataclass
class Taxonomy:
    groups: list[dict]
    categories: list[dict]

    @property
    def group_ids(self) -> list[str]:
        return [group['id'] for group in self.groups]

    def group_of(self, category: str) -> str | None:
        for entry in self.categories:
            if entry['name'] == category:
                return entry['group']
        return None


@dataclass
class Catalog:
    items: list[Item]
    taxonomy: Taxonomy
    verified_at: str

    @property
    def version(self) -> str:
        """Catalog version derived from the newest verification date."""
        return self.verified_at.replace('-', '.')


def _require(condition: bool, problems: list[str], message: str) -> bool:
    if not condition:
        problems.append(message)
    return condition


def _read_taxonomy(root: pathlib.Path, problems: list[str]) -> Taxonomy:
    path = root / 'data' / 'catalog' / 'taxonomy.json'
    raw = json.loads(path.read_text(encoding='utf-8'))
    taxonomy = Taxonomy(groups=raw['groups'], categories=raw['categories'])
    known_groups = set(taxonomy.group_ids)
    seen_categories: set[str] = set()
    for entry in taxonomy.categories:
        name = entry['name']
        _require(name not in seen_categories, problems, f'duplicate category {name}')
        seen_categories.add(name)
        _require(entry['group'] in known_groups, problems,
                 f'category {name} points at unknown group {entry["group"]}')
        for language in ('en', 'ru', 'de'):
            _require(bool(entry['labels'].get(language)), problems,
                     f'category {name} has no {language} label')
    for group in taxonomy.groups:
        for language in ('en', 'ru', 'de'):
            _require(bool(group['labels'].get(language)), problems,
                     f'group {group["id"]} has no {language} label')
    return taxonomy


def _parse_item(raw: dict, kind: str, source_file: str, problems: list[str]) -> Item | None:
    item_id = raw.get('id', '')
    where = f'{source_file}:{item_id or "<no id>"}'
    if not _require(bool(_ID_RE.match(item_id)), problems, f'{where}: bad id'):
        return None
    for required in ('name', 'category', 'access', 'url', 'verifiedAt'):
        if not _require(bool(raw.get(required)), problems, f'{where}: missing {required}'):
            return None

    summary = {lang: text for lang, text in (raw.get('summary') or {}).items() if text}
    tip = {lang: text for lang, text in (raw.get('tip') or {}).items() if text}
    _require(bool(summary), problems, f'{where}: no summary in any language')
    for language in list(summary) + list(tip):
        _require(language in LANGUAGES, problems, f'{where}: unsupported language {language}')

    showcases = []
    for index, entry in enumerate(raw.get('showcases') or []):
        note = {lang: text for lang, text in (entry.get('note') or {}).items() if text}
        _require(entry.get('kind') in SHOWCASE_KINDS, problems,
                 f'{where}: showcase {index} has kind {entry.get("kind")!r}')
        _require(str(entry.get('url', '')).startswith('https://'), problems,
                 f'{where}: showcase {index} is not https')
        showcases.append(Showcase(
            title=entry['title'], url=entry['url'], kind=entry['kind'],
            note=note, image=entry.get('image', ''),
        ))

    pricing = dict(raw.get('pricing') or {})
    if pricing:
        _require(pricing.get('model') in PRICING_MODELS, problems,
                 f'{where}: unknown pricing model {pricing.get("model")!r}')
        url = pricing.get('url', '')
        _require(not url or url.startswith('https://'), problems,
                 f'{where}: pricing url is not https')

    return Item(
        id=item_id,
        name=raw['name'],
        kind=kind,
        category=raw['category'],
        access=raw['access'],
        url=raw['url'],
        tags=[tag.strip() for tag in raw.get('tags', []) if tag.strip()],
        summary=summary,
        tip=tip,
        verified_at=raw['verifiedAt'],
        install=raw.get('install', ''),
        compatibility=raw.get('compatibility') or ['both'],
        source=raw.get('source', 'curated'),
        pricing=pricing,
        showcases=showcases,
    )


def load_catalog(root: pathlib.Path) -> Catalog:
    """Read every JSONL source, validate the contract, return the catalog.

    Raises `CatalogError` listing every problem, so one run shows all the work
    instead of one error per run.
    """
    problems: list[str] = []
    taxonomy = _read_taxonomy(root, problems)
    source_dir = root / 'data' / 'catalog'

    items: list[Item] = []
    for file_name, kind in sorted(KIND_FILES.items()):
        path = source_dir / file_name
        if not path.exists():
            problems.append(f'missing source file {file_name}')
            continue
        for line_number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            if not line.strip():
                continue
            try:
                raw = json.loads(line)
            except json.JSONDecodeError as error:
                problems.append(f'{file_name}:{line_number}: invalid JSON ({error.msg})')
                continue
            parsed = _parse_item(raw, kind, file_name, problems)
            if parsed is not None:
                items.append(parsed)

    _validate_items(items, taxonomy, problems)
    if problems:
        raise CatalogError(sorted(set(problems)))

    for item in items:
        item.group = taxonomy.group_of(item.category) or ''
        if not item.pricing:
            item.pricing = {
                'model': DEFAULT_PRICING_MODEL[item.access],
                'checkedAt': item.verified_at,
            }

    items.sort(key=lambda item: (item.kind, item.group, item.category, item.name.lower()))
    verified_at = max(item.verified_at for item in items)
    return Catalog(items=items, taxonomy=taxonomy, verified_at=verified_at)


def _validate_items(items: list[Item], taxonomy: Taxonomy, problems: list[str]) -> None:
    known_categories = {entry['name'] for entry in taxonomy.categories}
    by_id: dict[str, str] = {}
    by_name: dict[str, list[str]] = {}

    for item in items:
        if item.id in by_id:
            problems.append(f'duplicate id {item.id} (also in {by_id[item.id]})')
        by_id[item.id] = item.kind
        # Two entries with the same display name are indistinguishable in the
        # list, whatever their ids say.
        key = _fold_name(item.name)
        by_name.setdefault(key, []).append(item.id)

        _require(item.access in ACCESS_TYPES, problems,
                 f'{item.id}: unknown access {item.access!r}')
        _require(item.url.startswith('https://'), problems,
                 f'{item.id}: url is not https')
        _require(item.category in known_categories, problems,
                 f'{item.id}: category {item.category!r} is not in the taxonomy')
        _require(bool(re.fullmatch(r'\d{4}-\d{2}-\d{2}', item.verified_at)), problems,
                 f'{item.id}: verifiedAt {item.verified_at!r} is not YYYY-MM-DD')
        # The id is what a person types in a bug report; it has to look like
        # the name it belongs to.
        _require(_id_matches_name(item.id, item.name), problems,
                 f'{item.id}: id does not match name {item.name!r}')
        _require(bool(item.tags), problems, f'{item.id}: no tags')
        _require(bool(item.tip), problems, f'{item.id}: no tip in any language')

        if item.access == 'paid':
            _require(bool(item.pricing.get('from')), problems,
                     f'{item.id}: access is paid but no starting price is given')
            _require(item.pricing.get('model') != 'free', problems,
                     f'{item.id}: access is paid but priced as free')
        if item.pricing.get('from'):
            _require(bool(item.pricing.get('url')), problems,
                     f'{item.id}: has a price but no link to the price list')

    for key, ids in by_name.items():
        if len(ids) > 1:
            problems.append(f'duplicate name {key!r}: {sorted(ids)}')

    # Floors, not targets: they catch a source file that silently failed to
    # load far better than any single-entry check can.
    _require(len(items) >= MIN_ITEMS, problems,
             f'only {len(items)} entries, expected at least {MIN_ITEMS}')
    per_kind = Counter(item.kind for item in items)
    for kind in KIND_FILES.values():
        _require(per_kind[kind] > MIN_PER_KIND, problems,
                 f'only {per_kind[kind]} entries of kind {kind}')
    cards = sum(len(item.showcases) for item in items)
    _require(cards >= MIN_SHOWCASE_CARDS, problems,
             f'only {cards} showcase cards, expected at least {MIN_SHOWCASE_CARDS}')


def _fold_name(name: str) -> str:
    return normalize(unicodedata.normalize('NFKC', name))


def _words(text: str) -> set[str]:
    plain = unicodedata.normalize('NFKD', text.lower())
    return {word for word in re.split(r'[^a-z0-9]+', plain) if len(word) > 2}


def _id_matches_name(item_id: str, name: str) -> bool:
    """Whether `api-open-meteo` plausibly belongs to `Open-Meteo`.

    An id copied from a neighbouring entry while writing a batch (`lib-rambda`
    holding Ramda) silently blocks the real product's id later. Abbreviations
    are legitimate, so this only rejects an id sharing no word at all with its
    own name; the deliberate acronyms are listed in `ID_NAME_EXEMPT`.
    """
    if item_id in ID_NAME_EXEMPT:
        return True
    slug = _words(re.sub(r'^(api|lib|mcp|skill|paid)-', '', item_id))
    label = _words(name)
    if not slug or not label:
        return True
    return bool(slug & label) or any(
        left.startswith(right) or right.startswith(left)
        for left in slug for right in label)
