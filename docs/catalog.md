# Каталог: формат, сборка, публикация

Каталог — это данные, а не код. Он живёт отдельно от приложения, собирается
отдельной командой и обновляется на устройстве без переустановки.

```
data/catalog/*.jsonl   ← правится руками, ревьюится в PR
        │
        │  python3 tool/build_catalog.py
        ▼
assets/catalog.db      ← едет внутри сборки, засевает первый запуск
lib/i18n/categories.dart
lib/app_info.dart
        │
        │  python3 tool/build_catalog.py --release
        ▼
dist/catalog.db.gz + dist/manifest.json   ← ассеты GitHub Release
        │
        │  приложение читает manifest, сверяет sha256, подменяет файл
        ▼
<app support dir>/catalog.db
```

## Исходные данные

Одна запись — одна строка JSON. Вид записи задаёт имя файла, поэтому внутри
записи его нет:

| Файл | Вид |
|------|-----|
| `data/catalog/apis.jsonl` | `api` |
| `data/catalog/libs.jsonl` | `library` |
| `data/catalog/mcp.jsonl` | `mcp` |
| `data/catalog/skills.jsonl` | `skill` |
| `data/catalog/taxonomy.json` | группы и категории с переводами |

```json
{"id":"api-open-meteo","name":"Open-Meteo","category":"Погода и геоданные",
 "access":"noKey","url":"https://open-meteo.com/en/docs",
 "tags":["weather","forecast","no-key"],
 "summary":{"en":"Forecast, archive and geocoding without a key.",
            "ru":"Прогноз, архив и геокодирование без ключа."},
 "tip":{"en":"Request only the hourly fields you need."},
 "verifiedAt":"2026-08-19"}
```

Обязательные поля: `id`, `name`, `category`, `access`, `url`, `tags`,
`summary` (хотя бы один язык), `tip`, `verifiedAt`.

Необязательные: `install`, `compatibility` (по умолчанию `["both"]`),
`source`, `pricing`, `showcases`.

Ничего выводимого в исходниках не хранится. Группа категории берётся из
`taxonomy.json`, модель оплаты для бесплатных записей — из `access`,
нормализованный текст для поиска считается при сборке.

### Значения полей

- `access`: `noKey`, `freeTier`, `openSource`, `free`, `mixed`, `paid`;
- `pricing.model`: `free`, `freemium`, `openSource`, `usage`, `subscription`,
  `hybrid`, `commission`, `credits`, `one-time`, `custom`;
- `showcases[].kind`: `gallery`, `site`, `demo`, `code`.

У записи с `access: paid` обязана быть `pricing.from` и ссылка на прайс.

## Добавить запись

```bash
# 1. дописать строку в нужный .jsonl
# 2. пересобрать и проверить
python3 tool/build_catalog.py
python3 tool/verify_catalog.py
```

`verify_catalog.py` — это тот же контракт, который CI гоняет на каждый PR:
уникальные id и имена, только https, категория из таксономии, id похож на имя,
у платных записей есть цена, собранная база совпадает с исходниками.

Что коммитится, а что нет:

| Файл | В git | Почему |
|------|-------|--------|
| `data/catalog/*.jsonl`, `taxonomy.json` | да | это и есть исходник |
| `lib/i18n/categories.dart`, `lib/app_info.dart` | да | должны компилироваться, текстовый diff |
| `assets/catalog.db` | нет | артефакт сборки, 3.5 МБ бинаря на каждое изменение каталога |
| `dist/catalog.db.gz`, `dist/manifest.json` | нет | ассеты релиза, собираются в CI |

Свежий клон без `assets/catalog.db` не соберётся: сначала
`python3 tool/build_catalog.py`. CI и `tool/bootstrap_platforms.sh` делают это
сами.

## Схема базы

`schemaVersion` = 4. Таблицы: `meta`, `groups`, `categories`, `items`,
`showcases`.

`items` хранит и предвычисленные `norm_name`, `norm_category`, `norm_tags`,
`norm_text` — нормализованный текст для поиска. Нормализация (нижний регистр,
`ё` → `е`, срез пунктуации) в Python и в Dart совпадает символ в символ; база
её несёт, чтобы приложение не гоняло два регулярных выражения по всем записям
на каждое нажатие клавиши.

База для приложения — только на чтение. Ничего пользовательского в ней нет:
избранное, свои записи и настройки лежат в SharedPreferences именно потому,
что этот файл целиком заменяется при обновлении.

## Публикация

Релиз (`.github/workflows/release.yml`, триггер — тег `v*`) кладёт в GitHub
Release сборки под платформы плюс два файла каталога:

- `catalog.db.gz` — сжатая база (~0.8 МБ против 3.5 МБ распакованной);
- `manifest.json` — то, что приложение читает первым.

```json
{
  "schemaVersion": 4,
  "catalogVersion": "2026.08.19",
  "verifiedAt": "2026-08-19",
  "itemCount": 2889,
  "file": "catalog.db.gz",
  "url": "https://github.com/…/releases/download/v0.7.0/catalog.db.gz",
  "size": 828158,
  "sha256": "…",
  "databaseSize": 3633152,
  "databaseSha256": "…",
  "minAppVersion": "0.7.0",
  "signature": ""
}
```

Приложение ходит по постоянному адресу
`releases/latest/download/manifest.json` — это редирект GitHub на последний
релиз, поэтому не нужен ни токен, ни обращение к API с его лимитами.

Порядок проверок при обновлении, каждая до того, как что-то заменено:

1. `schemaVersion` и `minAppVersion` — иначе качать нечего;
2. `catalogVersion` новее установленной — иначе трафик не тратится;
3. размер и `sha256` архива;
4. размер и `databaseSha256` распакованной базы;
5. файл открывается и его `meta.catalogVersion` совпадает с манифестом;
6. только теперь старый файл удаляется и новый встаёт на его место.

Провал любого шага оставляет установленную базу нетронутой.

`minAppVersion` — это нижняя граница совместимости схемы
(`MIN_APP_VERSION` в `tool/catalog/release.py`), а не текущая версия
приложения. Поднимать её нужно только вместе с `CATALOG_SCHEMA_VERSION`,
иначе старые сборки будут получать отказ на каталоге, который они прекрасно
прочитали бы.

Поле `signature` зарезервировано и сейчас пустое. Подлинность обеспечивают
HTTPS до github.com и sha256 из манифеста; см. [SECURITY.md](../SECURITY.md).
