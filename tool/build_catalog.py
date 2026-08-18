# -*- coding: utf-8 -*-
"""Builds assets/catalog.json (schemaVersion 3) and lib/i18n/categories.dart."""
import json, sys, re, datetime, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent / 'catalog_src'))

from data_apis import APIS
from data_libs import LIBS
from data_mcp_skills import MCPS, SKILLS
from data_paid import PAID
from data_showcases import SHOWCASES

# Stage 5+: expansion packs live in their own files rather than growing the
# stage 1-4 packs further. They are discovered by filename instead of being
# imported one by one, so adding `data_apis_5k.py` needs no edit here at all —
# drop the file in catalog_src/ and it is picked up on the next build.
#
# Convention: data_<kind>_<suffix>.py exporting <KIND>_<SUFFIX> in upper case,
# e.g. data_apis_5a.py -> APIS_5A, data_mcp_skills_5b.py -> MCPS_5B + SKILLS_5B.
import importlib

_PACK_SRC = pathlib.Path(__file__).parent / 'catalog_src'


def _load_packs(prefix, *symbols):
    """Import every data_<prefix>_*.py pack and concatenate the named lists."""
    found = {symbol: [] for symbol in symbols}
    for path in sorted(_PACK_SRC.glob(f'data_{prefix}_*.py')):
        module = importlib.import_module(path.stem)
        suffix = path.stem[len(f'data_{prefix}_'):].upper()
        for symbol in symbols:
            rows = getattr(module, f'{symbol}_{suffix}', None)
            if rows:
                found[symbol].extend(rows)
    return [found[symbol] for symbol in symbols]


(_apis_extra,) = _load_packs('apis', 'APIS')
(_libs_extra,) = _load_packs('libs', 'LIBS')
_mcps_extra, _skills_extra = _load_packs('mcp_skills', 'MCPS', 'SKILLS')
(_paid_extra,) = _load_packs('paid', 'PAID')

APIS = APIS + _apis_extra
LIBS = LIBS + _libs_extra
MCPS = MCPS + _mcps_extra
SKILLS = SKILLS + _skills_extra
PAID = PAID + _paid_extra

ROOT = str(pathlib.Path(__file__).resolve().parent.parent)
TODAY = '2026-08-17'

# ---------------------------------------------------------------- categories
GROUPS = {
    'ai':       {'en': 'AI and agents',        'ru': 'ИИ и агенты',            'de': 'KI und Agenten'},
    'frontend': {'en': 'Frontend and UI',      'ru': 'Frontend и UI',          'de': 'Frontend und UI'},
    'backend':  {'en': 'Backend and data',     'ru': 'Backend и данные',       'de': 'Backend und Daten'},
    'devtools': {'en': 'Developer tooling',    'ru': 'Инструменты разработки', 'de': 'Entwickler-Tools'},
    'data':     {'en': 'Data and science',     'ru': 'Данные и наука',         'de': 'Daten und Wissenschaft'},
    'media':    {'en': 'Media and content',    'ru': 'Медиа и контент',        'de': 'Medien und Inhalte'},
    'skills':   {'en': 'Skills and practices', 'ru': 'Skills и практики',      'de': 'Skills und Praktiken'},
}

# category -> (group, en, de)   (the key itself is the Russian label)
CATEGORIES = {
 'AI и агенты':                    ('ai', 'AI and agents', 'KI und Agenten'),
 'AI-фреймворки':                  ('ai', 'AI frameworks', 'KI-Frameworks'),
 'AI skills':                      ('skills', 'AI skills', 'KI-Skills'),
 'Локальные модели':               ('ai', 'Local models', 'Lokale Modelle'),
 'Векторные базы':                 ('ai', 'Vector databases', 'Vektordatenbanken'),
 'Планирование и reasoning':       ('ai', 'Planning and reasoning', 'Planung und Reasoning'),
 'Память и знания':                ('ai', 'Memory and knowledge', 'Gedächtnis und Wissen'),
 'Инфраструктура MCP':             ('ai', 'MCP infrastructure', 'MCP-Infrastruktur'),
 'UI и frontend':                  ('frontend', 'UI and frontend', 'UI und Frontend'),
 'Анимация и 3D':                  ('frontend', 'Animation and 3D', 'Animation und 3D'),
 'Графики и визуализация':         ('frontend', 'Charts and visualization', 'Diagramme und Visualisierung'),
 'Изображения и дизайн':           ('frontend', 'Images and design', 'Bilder und Design'),
 'Frontend skills':                ('skills', 'Frontend skills', 'Frontend-Skills'),
 'Сборка и bundlers':              ('devtools', 'Build and bundlers', 'Build und Bundler'),
 'Языки и рантаймы':               ('devtools', 'Languages and runtimes', 'Sprachen und Runtimes'),
 'Backend и API':                  ('backend', 'Backend and APIs', 'Backend und APIs'),
 'Базы данных и backend':          ('backend', 'Databases and backend', 'Datenbanken und Backend'),
 'Базы данных и ORM':              ('backend', 'Databases and ORM', 'Datenbanken und ORM'),
 'Состояние и данные':             ('backend', 'State and data', 'Zustand und Daten'),
 'Аутентификация':                 ('backend', 'Authentication', 'Authentifizierung'),
 'CMS и контент':                  ('backend', 'CMS and content', 'CMS und Inhalte'),
 'Платежи и commerce':             ('backend', 'Payments and commerce', 'Zahlungen und Commerce'),
 'Почта и уведомления':            ('backend', 'Email and notifications', 'E-Mail und Benachrichtigungen'),
 'Очереди и события':              ('backend', 'Queues and events', 'Queues und Events'),
 'Таблицы и no-code':              ('backend', 'Spreadsheets and no-code', 'Tabellen und No-Code'),
 'Безопасность и секреты':         ('devtools', 'Security and secrets', 'Sicherheit und Secrets'),
 'Инфраструктура как код':         ('devtools', 'Infrastructure as code', 'Infrastructure as Code'),
 'Локализация и i18n':             ('devtools', 'Localization and i18n', 'Lokalisierung und i18n'),
 'Документы и PDF':                ('media', 'Documents and PDF', 'Dokumente und PDF'),
 'Здоровье и медицина':            ('data', 'Health and medicine', 'Gesundheit und Medizin'),
 'Разработка и DevOps':            ('devtools', 'Development and DevOps', 'Entwicklung und DevOps'),
 'Тестирование и качество':        ('devtools', 'Testing and quality', 'Tests und Qualität'),
 'Git и совместная работа':        ('devtools', 'Git and collaboration', 'Git und Zusammenarbeit'),
 'Браузер и автоматизация':        ('devtools', 'Browser and automation', 'Browser und Automatisierung'),
 'Мониторинг и аналитика':         ('devtools', 'Monitoring and analytics', 'Monitoring und Analytik'),
 'Облако и деплой':                ('devtools', 'Cloud and deployment', 'Cloud und Deployment'),
 'Документация и контекст':        ('devtools', 'Documentation and context', 'Dokumentation und Kontext'),
 'Установка и миграция':           ('devtools', 'Installation and migration', 'Installation und Migration'),
 'Локальные инструменты':          ('devtools', 'Local tools', 'Lokale Tools'),
 'CLI и терминал':                 ('devtools', 'CLI and terminal', 'CLI und Terminal'),
 'Тестовые и mock API':            ('devtools', 'Test and mock APIs', 'Test- und Mock-APIs'),
 'Утилиты':                        ('devtools', 'Utilities', 'Dienstprogramme'),
 'Мобильная и desktop разработка': ('devtools', 'Mobile and desktop development', 'Mobile- und Desktop-Entwicklung'),
 'Наука и исследования':           ('data', 'Science and research', 'Wissenschaft und Forschung'),
 'Открытые данные':                ('data', 'Open data', 'Offene Daten'),
 'Данные и ML':                    ('data', 'Data and ML', 'Daten und ML'),
 'Поиск и retrieval':              ('data', 'Search and retrieval', 'Suche und Retrieval'),
 'Погода и геоданные':             ('data', 'Weather and geodata', 'Wetter und Geodaten'),
 'Карты и навигация':              ('data', 'Maps and navigation', 'Karten und Navigation'),
 'Транспорт и город':              ('data', 'Transport and city', 'Verkehr und Stadt'),
 'Финансы':                        ('data', 'Finance', 'Finanzen'),
 'Космос и наука':                 ('data', 'Space and science', 'Weltraum und Wissenschaft'),
 'Музыка и медиа':                 ('media', 'Music and media', 'Musik und Medien'),
 'Видео и медиа':                  ('media', 'Video and media', 'Video und Medien'),
 'Видео и медиа skills':           ('skills', 'Video and media skills', 'Video- und Medien-Skills'),
 'Аудио и голос':                  ('media', 'Audio and voice', 'Audio und Sprache'),
 'Игры и развлечения':             ('media', 'Games and entertainment', 'Spiele und Unterhaltung'),
 'Еда и быт':                      ('media', 'Food and lifestyle', 'Essen und Alltag'),
 'Книги и знания':                 ('media', 'Books and knowledge', 'Bücher und Wissen'),
 'Новости и сообщества':           ('media', 'News and communities', 'Nachrichten und Communities'),
 'Коммуникации':                   ('media', 'Communication', 'Kommunikation'),
 'Переводы и языки':               ('media', 'Translation and languages', 'Übersetzung und Sprachen'),
 'Словари и лексика':              ('media', 'Dictionaries and vocabulary', 'Wörterbücher und Wortschatz'),
 'Каталоги skills':                ('skills', 'Skill catalogs', 'Skill-Kataloge'),
 'Создание skills':                ('skills', 'Creating skills', 'Skills erstellen'),
 'Гайды и практики':               ('skills', 'Guides and practices', 'Leitfäden und Praktiken'),
 'Mobile skills':                  ('skills', 'Mobile skills', 'Mobile-Skills'),
 'Workflow skills':                ('skills', 'Workflow skills', 'Workflow-Skills'),

 # ==================== Stage 5 additions (4-5x expansion) ====================
 'Компьютерное зрение':            ('ai', 'Computer vision', 'Computer Vision'),
 'MLOps и эксперименты':           ('ai', 'MLOps and experiment tracking', 'MLOps und Experiment-Tracking'),
 'Голосовые агенты':               ('ai', 'Voice agents', 'Sprachagenten'),
 'AI-оценка и safety':             ('ai', 'AI evaluation and safety', 'KI-Evaluation und Sicherheit'),
 'NLP и обработка текста':         ('ai', 'NLP and text processing', 'NLP und Textverarbeitung'),
 'Рекомендательные системы':       ('ai', 'Recommendation engines', 'Empfehlungssysteme'),
 'Web3 и блокчейн':                ('backend', 'Web3 and blockchain', 'Web3 und Blockchain'),
 'IoT и встраиваемые системы':     ('devtools', 'IoT and embedded systems', 'IoT und eingebettete Systeme'),
 'Service mesh и Kubernetes':      ('devtools', 'Service mesh and Kubernetes', 'Service Mesh und Kubernetes'),
 'Feature flags и эксперименты':   ('devtools', 'Feature flags and experiments', 'Feature Flags und Experimente'),
 'CDP и маркетинг-данные':         ('data', 'CDP and marketing data', 'CDP und Marketingdaten'),
 'CRM и продажи':                  ('backend', 'CRM and sales', 'CRM und Vertrieb'),
 'Email-маркетинг':                ('backend', 'Email marketing', 'E-Mail-Marketing'),
 'Формы и опросы':                 ('backend', 'Forms and surveys', 'Formulare und Umfragen'),
 'Видеоконференции':               ('media', 'Video conferencing', 'Videokonferenzen'),
 'Стриминг видео':                 ('media', 'Video streaming', 'Video-Streaming'),
 'OCR и распознавание документов': ('ai', 'OCR and document AI', 'OCR und Dokumenten-KI'),
 'Идентификация и KYC':            ('backend', 'Identity verification and KYC', 'Identitätsprüfung und KYC'),
 'Логистика и доставка':           ('data', 'Logistics and shipping', 'Logistik und Versand'),
 'Недвижимость':                   ('data', 'Real estate', 'Immobilien'),
 'Юридические и налоговые API':    ('data', 'Legal and tax APIs', 'Rechts- und Steuer-APIs'),
 'Образование':                    ('data', 'Education', 'Bildung'),
 'HR и рекрутинг':                 ('backend', 'HR and recruiting', 'HR und Recruiting'),
 'Планирование и календари':       ('backend', 'Scheduling and calendars', 'Terminplanung und Kalender'),
 'A/B-тестирование':               ('devtools', 'A/B testing', 'A/B-Tests'),
 'Визуальное тестирование':        ('devtools', 'Visual regression testing', 'Visuelle Regressionstests'),
 'Доступность (a11y)':             ('devtools', 'Accessibility', 'Barrierefreiheit'),
 'Rust экосистема':                ('devtools', 'Rust ecosystem', 'Rust-Ökosystem'),
 'Go экосистема':                  ('devtools', 'Go ecosystem', 'Go-Ökosystem'),
 'JVM экосистема':                 ('devtools', 'JVM ecosystem', 'JVM-Ökosystem'),
 '.NET экосистема':                ('devtools', '.NET ecosystem', '.NET-Ökosystem'),
 'Elixir экосистема':              ('devtools', 'Elixir ecosystem', 'Elixir-Ökosystem'),
 'Игровые движки':                 ('media', 'Game engines', 'Game-Engines'),
 'Дизайн-системы':                 ('frontend', 'Design systems', 'Designsysteme'),
 'Наблюдаемость трасс':            ('devtools', 'Distributed tracing', 'Verteiltes Tracing'),
 'Секреты и конфигурация':         ('devtools', 'Secrets and configuration', 'Secrets und Konfiguration'),
 'Message queues и брокеры':       ('backend', 'Message queues and brokers', 'Message Queues und Broker'),
 'Data engineering':               ('data', 'Data engineering', 'Data Engineering'),
 'Умный дом и IoT':                ('data', 'Умный дом и IoT', 'Smart home & IoT'),
 'Робототехника':                  ('data', 'Robotics', 'Robotik'),
}

MERGE = {
 'paid-deel': 'api-deel', 'paid-remote-com': 'api-remote-com',
 'paid-weglot': 'api-weglot',
 'paid-openai': 'api-openai-platform', 'paid-anthropic': 'api-anthropic',
 'paid-gemini': 'api-google-gemini', 'paid-mistral': 'api-mistral',
 'paid-groq': 'api-groq', 'paid-openrouter': 'api-openrouter',
 'paid-deepseek': 'api-deepseek', 'paid-elevenlabs': 'api-elevenlabs',
 'paid-deepgram': 'api-deepgram', 'paid-assemblyai': 'api-assemblyai',
 'paid-tavily': 'api-tavily', 'paid-exa': 'api-exa',
 'paid-supabase': 'api-supabase', 'paid-neon': 'api-neon',
 'paid-upstash': 'api-upstash', 'paid-firebase': 'api-firebase',
 'paid-cloudinary': 'api-cloudinary', 'paid-sentry': 'api-sentry',
 'paid-posthog': 'api-posthog', 'paid-resend': 'api-resend',
 'paid-stripe-fees': 'api-stripe', 'paid-mapbox': 'api-mapbox',
 'paid-openweather': 'api-openweathermap', 'paid-deepl-pro': 'api-deepl',
 'paid-airtable': 'api-airtable', 'paid-vercel': 'api-vercel-rest',
 'paid-netlify': 'api-netlify',
 # stage 4
 'paid-revenuecat': 'api-revenuecat',
 'paid-polar': 'api-polar', 'paid-brevo': 'api-brevo',
 'paid-mailgun': 'api-mailgun', 'paid-langfuse': 'api-langfuse',
 'paid-helicone': 'api-helicone', 'paid-imagekit': 'api-imagekit',
 'paid-cloudconvert': 'api-cloudconvert', 'paid-maptiler': 'api-maptiler',
 'paid-uptimerobot': 'api-uptimerobot', 'paid-voyage': 'api-voyage',
 # stage 5
 'paid-docsumo': 'api-docsumo', 'paid-openai-realtime': 'api-openai-realtime',
 'paid-close-crm': 'api-close-crm', 'paid-salesforce': 'api-salesforce',
 'paid-workday': 'api-workday', 'paid-onfido': 'api-onfido',
 'paid-stripe-identity': 'api-stripe-identity', 'paid-veriff': 'api-veriff',
 'paid-attom-data': 'api-attom-data', 'paid-avalara': 'api-avalara',
 'paid-brightdata-mcp': 'mcp-brightdata', 'paid-datadog-mcp': 'mcp-datadog',
 'paid-redox': 'api-redox', 'paid-marqeta': 'api-marqeta',
 'paid-stripe-treasury': 'api-stripe-treasury', 'paid-yodlee': 'api-yodlee',
 'paid-optimizely': 'api-optimizely', 'paid-vwo': 'api-vwo',
 'paid-lightstep': 'api-lightstep', 'paid-aws-secrets-manager': 'api-aws-secrets-manager',
 'paid-statuspage-io': 'api-statuspage-io',
 'paid-drugbank': 'api-drugbank', 'paid-project44': 'api-project44',
 'paid-middesk': 'api-middesk', 'paid-postmark-templates': 'api-postmark-2',
 'paid-kagi-search': 'mcp-kagi-search',
 'paid-rippling': 'api-rippling', 'paid-planetscale-vitess': 'api-planetscale-2',
 'paid-dynatrace': 'api-dynatrace', 'paid-bunny-net': 'api-bunny-net',
 'paid-intercom': 'api-intercom', 'paid-front-app': 'api-front-app',
 'paid-hive-moderation': 'api-hive-moderation',
 'paid-openai-embeddings': 'api-openai-embeddings',
 'paid-isbndb': 'api-isbndb', 'paid-tecton': 'api-tecton',
 'paid-browserstack': 'api-browserstack', 'paid-saucelabs': 'api-saucelabs',
 'paid-shodan': 'api-shodan', 'paid-x-twitter': 'api-x-twitter',
 'paid-adyen': 'api-adyen', 'paid-github-copilot': 'api-github-copilot',
 'paid-clearbit': 'api-clearbit', 'paid-urlbox': 'api-urlbox',
 'paid-8thwall': 'api-8thwall', 'paid-vectorizer-ai': 'api-vectorizer-ai',
 'paid-lalal-ai': 'api-lalal-ai',
}

def build_showcases(item_id):
    out = []
    for entry in SHOWCASES.get(item_id, []):
        title, url, kind, note_en, note_ru = entry
        out.append({
            'title': title, 'url': url, 'kind': kind,
            'note': note_en, 'localizedNote': {'en': note_en, 'ru': note_ru},
        })
    return out

def make(kind, row, pricing=None):
    (iid, name, category, url, access, tags, install,
     summary_en, summary_ru, tip_en) = row
    item = {
        'id': iid, 'name': name, 'kind': kind, 'category': category,
        'summary': summary_en, 'url': url, 'access': access,
        'compatibility': ['both'],
        'tags': [t.strip() for t in tags.split(',') if t.strip()],
        'tip': tip_en, 'verifiedAt': TODAY, 'install': install,
        'source': 'curated', 'sourceLanguage': 'en',
        'localizedSummary': {'en': summary_en, 'ru': summary_ru},
        'localizedTip': {'en': tip_en},
    }
    if pricing:
        item['pricing'] = pricing
    sc = build_showcases(iid)
    if sc:
        item['showcases'] = sc
    return item

def main():
    # The seed is the frozen stage-2 catalog, never the generated output:
    # reading assets/catalog.json here would append the packs a second time
    # on every run and the id/name validation below would fail.
    with open(f'{ROOT}/tool/catalog_src/catalog.seed.json', encoding='utf-8') as f:
        base = json.load(f)
    items = base['items']

    # normalise legacy entries
    for it in items:
        it.setdefault('compatibility', ['both'])
        it.setdefault('install', '')
        it.setdefault('source', 'curated')
        it.setdefault('sourceLanguage', 'ru')
        it.setdefault('localizedSummary', {})
        it.setdefault('localizedTip', {})
        it.setdefault('verifiedAt', '2026-08-12')
        sc = build_showcases(it['id'])
        if sc:
            it['showcases'] = sc

    for row in APIS:
        items.append(make('api', row))
    for row in LIBS:
        items.append(make('library', row))
    for row in MCPS:
        items.append(make('mcp', row))
    for row in SKILLS:
        items.append(make('skill', row))
    by_id = {i['id']: i for i in items}
    for row in PAID:
        (iid, name, category, url, access, tags,
         summary_en, summary_ru, tip_en,
         model, price_from, free_quota, pricing_url) = row
        pricing = {
            'model': model, 'from': price_from, 'freeQuota': free_quota,
            'url': pricing_url, 'currency': 'USD', 'checkedAt': TODAY,
        }
        target = MERGE.get(iid)
        if target:
            if target not in by_id:
                raise SystemExit(f'merge target missing: {target}')
            existing = by_id[target]
            existing['pricing'] = pricing
            merged = list(dict.fromkeys(existing['tags'] + [t.strip() for t in tags.split(',') if t.strip()]))
            existing['tags'] = merged
            continue
        core = (iid, name, category, url, access, tags, '', summary_en, summary_ru, tip_en)
        item = make('api', core, pricing=pricing)
        items.append(item)
        by_id[iid] = item

    # ---- pricing defaults for non-paid entries -------------------------
    for it in items:
        if 'pricing' in it:
            continue
        access = it['access']
        model = {
            'noKey': 'free', 'free': 'free', 'openSource': 'openSource',
            'freeTier': 'freemium', 'mixed': 'freemium', 'paid': 'usage',
        }.get(access, 'free')
        it['pricing'] = {'model': model, 'checkedAt': it.get('verifiedAt', TODAY)}

    # ---- category groups ----------------------------------------------
    unknown = sorted({it['category'] for it in items} - set(CATEGORIES))
    if unknown:
        raise SystemExit(f'Categories without a group: {unknown}')
    for it in items:
        it['categoryGroup'] = CATEGORIES[it['category']][0]

    # ---- validation ----------------------------------------------------
    seen, errors = set(), []
    kinds = {'api', 'library', 'mcp', 'skill'}
    accesses = {'noKey', 'freeTier', 'openSource', 'free', 'mixed', 'paid'}
    for it in items:
        if it['id'] in seen:
            errors.append(f"duplicate id {it['id']}")
        seen.add(it['id'])
        if not it['url'].startswith('https://'):
            errors.append(f"non-https url {it['id']}")
        if it['kind'] not in kinds:
            errors.append(f"bad kind {it['id']}")
        if it['access'] not in accesses:
            errors.append(f"bad access {it['id']}")
        if not re.fullmatch(r'[a-z0-9-]+', it['id']):
            errors.append(f"bad id format {it['id']}")
        for sc in it.get('showcases', []):
            if not sc['url'].startswith('https://'):
                errors.append(f"non-https showcase {it['id']}")
    names = {}
    for it in items:
        key = it['name'].lower()
        names.setdefault(key, []).append(it['id'])
    for key, ids in names.items():
        if len(ids) > 1:
            errors.append(f"duplicate name {key}: {ids}")
    if errors:
        raise SystemExit('VALIDATION FAILED:\n' + '\n'.join(errors))

    items.sort(key=lambda i: (i['kind'], i['categoryGroup'], i['category'], i['name'].lower()))
    out = {
        'schemaVersion': 3,
        'catalogVersion': '2026.08.17',
        'verifiedAt': TODAY,
        'groups': [{'id': g, 'labels': GROUPS[g]} for g in GROUPS],
        'items': items,
    }
    with open(f'{ROOT}/assets/catalog.json', 'w', encoding='utf-8') as f:
        json.dump(out, f, ensure_ascii=False, indent=1)

    # ---- generate lib/i18n/categories.dart ------------------------------
    lines = [
        '// GENERATED by tool/build_catalog.py — do not edit by hand.',
        "import 'app_language.dart';", '',
        'const categoryGroupOf = <String, String>{',
    ]
    for cat, (grp, _en, _de) in sorted(CATEGORIES.items()):
        lines.append(f"  '{cat}': '{grp}',")
    lines += ['};', '', 'const groupLabels = <String, Map<AppLanguage, String>>{']
    for g, labels in GROUPS.items():
        lines.append(
            f"  '{g}': {{AppLanguage.en: '{labels['en']}', "
            f"AppLanguage.ru: '{labels['ru']}', AppLanguage.de: '{labels['de']}'}},")
    lines += ['};', '', 'const categoryLabels = <String, Map<AppLanguage, String>>{']
    for cat, (_g, en, de) in sorted(CATEGORIES.items()):
        lines.append(
            f"  '{cat}': {{AppLanguage.en: '{en}', "
            f"AppLanguage.ru: '{cat}', AppLanguage.de: '{de}'}},")
    lines += ['};', '']
    with open(f'{ROOT}/lib/i18n/categories.dart', 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    # ---- report ---------------------------------------------------------
    from collections import Counter
    print('total items :', len(items))
    print('by kind     :', dict(Counter(i['kind'] for i in items)))
    print('by group    :', dict(Counter(i['categoryGroup'] for i in items)))
    print('with pricing:', sum(1 for i in items if i['pricing'].get('from')))
    print('with shows  :', sum(1 for i in items if i.get('showcases')))
    print('categories  :', len({i['category'] for i in items}))
    print('paid access :', sum(1 for i in items if i['access'] == 'paid'))
    unused = set(SHOWCASES) - {i['id'] for i in items}
    if unused:
        print('!! showcase ids not found:', sorted(unused))

main()
