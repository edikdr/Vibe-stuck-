# VibeStack Atlas

[![CI](https://github.com/edikdr/Vibe-stuck-/actions/workflows/ci.yml/badge.svg)](https://github.com/edikdr/Vibe-stuck-/actions/workflows/ci.yml)
[![Release](https://github.com/edikdr/Vibe-stuck-/actions/workflows/release.yml/badge.svg)](https://github.com/edikdr/Vibe-stuck-/actions/workflows/release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Offline-first каталог инструментов для vibe coding в Codex и Claude Code:
**2889 записей** — 1006 API, 1586 библиотек, 152 MCP-сервера, 145 skills,
107 категорий. Один Flutter-проект под Android, Windows, Linux и macOS.

Каталог работает без сети целиком. Сеть нужна только чтобы обновиться.

## Установить

Готовые сборки — на [странице релизов](https://github.com/edikdr/Vibe-stuck-/releases/latest):

| Платформа | Файл |
|-----------|------|
| Android | `VibeStack-Atlas-<версия>-android.apk` |
| Windows | `VibeStack-Atlas-<версия>-windows-x64.zip` |
| Linux | `VibeStack-Atlas-<версия>-linux-x64.tar.gz` |
| macOS | `VibeStack-Atlas-<версия>-macos.dmg` |

APK подписан отладочным ключом — на телефон ставится, для Play Store нужен
свой keystore. Нужна свежая сборка без релиза: **Actions → APK → Run
workflow**.

## Каталог — это база на устройстве

Весь каталог — один файл SQLite. Приложение носит его в сборке, при первом
запуске кладёт себе в рабочую директорию и дальше обновляет **сам файл**, не
приложение.

```
data/catalog/*.jsonl ──build──▶ assets/catalog.db ──установка──▶ catalog.db на устройстве
                                       │                                ▲
                                       └──release──▶ GitHub Release ────┘
                                          catalog.db.gz + manifest.json
```

Обновление идёт со страницы релизов: приложение читает `manifest.json`,
сравнивает версию каталога со своей, качает `catalog.db.gz` (~0.8 МБ), сверяет
sha256 архива и распакованной базы, открывает её на проверку — и только потом
подменяет старый файл. Провал любого шага оставляет установленную базу
нетронутой.

Источников манифеста может быть несколько: если хост недоступен, клиент
пробует следующее зеркало, а архив всегда качает с того же хоста, который
ответил. Релиз можно подписывать Ed25519 — если в сборку вшит публичный ключ,
каталог без действительной подписи не устанавливается вовсе. Как это включить
— в [SECURITY.md](SECURITY.md).

Избранное, свои записи и настройки лежат отдельно, в SharedPreferences, —
именно потому, что база каталога заменяется целиком.

Кнопка ручной проверки и адрес источника — в «Обновлениях». Формат данных,
схема базы и порядок проверок описаны в [docs/catalog.md](docs/catalog.md).

## Добавить запись в каталог

Одна запись — одна строка JSON в `data/catalog/<вид>.jsonl`:

```json
{"id":"api-open-meteo","name":"Open-Meteo","category":"Погода и геоданные",
 "access":"noKey","url":"https://open-meteo.com/en/docs",
 "tags":["weather","forecast","no-key"],
 "summary":{"en":"Forecast and geocoding without a key.",
            "ru":"Прогноз и геокодирование без ключа."},
 "tip":{"en":"Request only the hourly fields you need."},
 "verifiedAt":"2026-08-19"}
```

```bash
python3 tool/build_catalog.py     # пересобрать базу и генерируемый Dart
python3 tool/verify_catalog.py    # контракт каталога, тот же что в CI
```

Flutter SDK для этого не нужен — только Python. Подробности и правила приёма —
в [CONTRIBUTING.md](CONTRIBUTING.md).

## Ежедневное автообновление

«Автообновление» в большинстве приложений работает только пока приложение
открыто. Здесь три слоя, и приложение честно показывает, какой из них активен:

| Слой | Платформа | Работает при закрытом приложении |
|------|-----------|----------------------------------|
| WorkManager periodic task | Android | да |
| Таймер внутри приложения + догоняющий запуск при старте и `resume` | все | нет |
| Системная задача → `--headless-sync` | Windows, Linux, macOS | да |

- время запуска выбирается в настройках (по умолчанию 09:00), показывается
  «следующее обновление»;
- `SyncRunner` — общий код для всех трёх слоёв, не трогает виджеты, поэтому
  безопасен в фоновом изолейте;
- если плагин недоступен или его API изменился, `BackgroundWorker` ловит ошибку
  и приложение деградирует до слоя 2 вместо падения при старте;
- защита от лишнего трафика: повтор не чаще чем раз в 20 часов, `--force`
  игнорирует ограничение.

Шаблоны системных задач — в `tool/desktop/` (systemd timer с
`Persistent=true`, Task Scheduler с `-StartWhenAvailable`, оба догоняют
пропущенный запуск).

## Источники живых обновлений

Помимо самой базы каталога приложение подтягивает то, что устаревает быстрее
релизов. Каждый источник отключается отдельно:

- `registry.modelcontextprotocol.io` — инкрементальная синхронизация
  официального MCP Registry;
- GitHub Trees API — поиск `SKILL.md` в `anthropics/skills`,
  `anthropics/claude-plugins-official`, `vercel-labs/agent-skills`,
  `expo/skills`, `huggingface/skills`, `remotion-dev/skills`;
- публичные metadata API npm и PyPI — свежие версии популярных библиотек;
- циклическая проверка доступности официальных ссылок API;
- звёзды GitHub суточной ротацией — для сортировки «самые популярные»;
- собственная HTTPS JSON-лента, если она указана в настройках.

## Поиск

Поиск идёт в памяти по всем 2889 записям и обязан быть мгновенным, поэтому
нормализация текста (нижний регистр, `ё` → `е`, срез пунктуации) считается
один раз при сборке базы и лежит в ней готовой. Приложение на каждое нажатие
клавиши только сравнивает уже нормализованные строки. Записи, которых в базе
нет — свои и пришедшие живым обновлением, — нормализуются один раз при первом
поиске.

Запрос понимает намерение на трёх языках: «бесплатная база», «сделать UI»,
«free database», «Datenbank» разворачиваются в теги, а не ищутся буквально.

## Тема

Весь цвет, радиусы и тени живут в `lib/theme.dart`. Виджеты просят роль, а не
оттенок:

| Роль | Токены |
|------|--------|
| фон | `ink`, `panel`, `panelRaised`, `panelSunken` |
| разделение | `line`, `lineStrong` |
| текст | `textPrimary`, `textSecondary`, `textMuted`, `textFaint`, `textGhost` |
| акцент и сигналы | `accent`, `info`, `positive`, `warning` |
| форма | `rCard`, `rControl`, `rChip`, `rTag`, `rSheet` |

Поэтому «сделать темнее» — правка одного файла, а не тридцати.

## Примеры с изображениями

У записи может быть массив `showcases` — реальные сайты, продакшен-галереи,
живые демо и репозитории с кодом. Поле `image` необязательно: если своей
картинки нет, приложение берёт живой скриншот сайта через выбранный провайдер.

| Провайдер | Где берётся картинка | Ключ |
|-----------|----------------------|------|
| `mshots` (по умолчанию) | `s.wordpress.com/mshots` | не нужен |
| `thumio` | `image.thum.io` | не нужен |
| `off` | внешние запросы полностью отключены | — |

mShots на первый запрос отдаёт заглушку, пока делает снимок, поэтому
`ShowcaseResolver.shouldRetry()` разрешает один повтор с задержкой. Всё
загруженное кладётся в дисковый кэш, поэтому во второй раз галерея открывается
офлайн. Кэш виден и очищается в «Обновлениях».

## Десктоп

- `PlatformInfo` и `Breakpoints` — единственное место, где приложение
  спрашивает «где я работаю» и «какая ширина»;
- на ширине ≥ 1180 px каталог переключается в master–detail: сетка слева,
  карточка записи в правой панели вместо модального листа;
- горячие клавиши: `Ctrl+F` поиск, `Ctrl+R` обновление, `Ctrl+1…4` разделы,
  `Home` наверх, `Esc` закрыть панель;
- `--headless-sync` запускает обновление без окна и завершает процесс
  (`0` — обновлено, `2` — пропущено);
- `tool/post_create.dart` ставит размер окна 1280×820 и человеческий заголовок.

Полный план перехода — в [DESKTOP.md](DESKTOP.md).

## Ключи и цены

В проект не встроены общие секретные ключи: каталог ведёт на официальную
регистрацию и различает `noKey`, `freeTier`, `openSource`, `free`, `mixed` и
`paid`. Цены в `pricing` — ориентир на дату `checkedAt`, а не оффер: тарифы
меняются, поэтому перед оплатой нужно открыть официальную страницу прайса
кнопкой в карточке. Что именно приложение делает с сетью — в
[SECURITY.md](SECURITY.md).

## Собрать самому

```bash
bash tool/bootstrap_platforms.sh     # flutter create + патчи + analyze + test
flutter build apk --release
flutter build windows --release
flutter build linux --release
```

Платформенные директории (`android/`, `windows/`, `linux/`, `macos/`) не
коммитятся: они генерируются `flutter create` из той версии Flutter, которая
стоит, а не замораживаются в репозитории.

Если `flutter pub get` не резолвит `workmanager`:

```bash
cp tool/fallback/background_worker_no_plugin.dart lib/services/sync/background_worker.dart
# и убрать строку workmanager: из pubspec.yaml
```

Приложение соберётся без плагина: пропадёт только пробуждение при закрытом
приложении на Android, остальные два слоя продолжат работать, а экран
«Обновления» честно покажет режим `in-app`.

## Проверки

```bash
python3 tool/build_catalog.py     # собрать каталог: без него тесты не запустятся
python3 tool/verify_catalog.py    # контракт каталога, без Flutter SDK
flutter analyze                   # должно быть «No issues found!»
flutter test
```

| Тест | Что держит |
|------|-----------|
| `catalog_database_test.dart` | база читается Dart-кодом, контракт записей, предвычисленный поиск |
| `catalog_update_test.dart` | обновление: битая сумма, обрезанный файл, чужая схема, старое приложение |
| `daily_sync_test.dart` | расписание, догоняющий запуск, деградация слоёв |
| `search_engine_test.dart`, `localization_test.dart` | поиск и три языка |
| `showcase_resolver_test.dart`, `popularity_test.dart` | скриншоты витрин и сортировка по звёздам |

## Структура

```
data/catalog/     исходные данные каталога (JSONL) + таксономия
tool/             build_catalog.py, verify_catalog.py, release_key.py, пакет catalog/
tool/desktop/     шаблоны системных задач для ежедневного обновления
assets/           catalog.db — собранная база (артефакт сборки, не в git)
lib/
  models/         catalog_item.dart, showcase.dart, search_index.dart
  platform/       platform_info.dart (PlatformInfo, Breakpoints)
  services/
    catalog_database.dart        база на устройстве
    catalog_update_service.dart  обновление из релизов
    catalog_repository.dart      настройки, избранное, свои записи
    sync/                        sync_runner, daily_sync_scheduler, background_worker
    search_engine.dart, live_update_service.dart, ai_context.dart
    showcase_resolver.dart, image_cache_service.dart
  widgets/        showcase_gallery, catalog_card, item_detail_sheet, scroll_to_top
  screens/        catalog, favorites, add_item, settings
  theme.dart      все цвета, радиусы и темы компонентов
  app_info.dart   генерируется сборкой: версии приложения и каталога
```

## Лицензия

[MIT](LICENSE) — и код, и данные каталога. Каталог ссылается на сторонние
сервисы: их имена и торговые марки принадлежат их владельцам, и правила
пользования у них свои.

История изменений — в [CHANGELOG.md](CHANGELOG.md).
