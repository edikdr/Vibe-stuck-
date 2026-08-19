# VibeStack Atlas — этап 5

Кроссплатформенный offline-first каталог инструментов для vibe coding в Codex и Claude Code. Один Flutter-проект для Android, Windows, Linux и macOS.

## Что нового в 0.6.0

- **2889 записей** вместо 562: 1006 API, 1586 библиотек, 152 MCP-сервера, 145 skills, 107 категорий, 7 разделов верхнего уровня;
- **275 записей с ценами** — модель оплаты, стартовая цена и бесплатный лимит указаны явно;
- **тренды по звёздам GitHub**: сортировка «самые популярные», бейдж в карточке; счётчики обновляются суточной ротацией, чтобы не упираться в лимит GitHub API;
- **десктопная сборка**: Windows, Linux и macOS собираются в CI и упаковываются в `.zip`, `.tar.gz` и `.dmg`;
- **инструменты каталога**: `check_names.py` ловит коллизии id и имён до сборки, `verify_catalog.py` дополнительно проверяет, что id соответствует названию.

## Что было в 0.5.0

- 562 записи вместо 393, 73 записи с ценами;
- тема на ролевых токенах: глубже фон, тоньше рамки, новый акцент — и ни одного hex-литерала в виджетах;
- крюк быстрого скролла наверх с кольцом прогресса и возвратом на прежнее место;
- исправлены баги этапа 3, из-за которых проект не проходил `flutter analyze` и рисовал каталог блоком ошибки в debug-сборке.

## Быстрый скролл наверх

Каталог на 2889 записей прокручивается далеко, поэтому в него добавлен `ScrollTopHook` — обёртка над любым скроллом:

```dart
ScrollTopHook(
  controller: _scroll,
  topLabel: s.t('scrollTop'),
  backLabel: s.t('scrollBack'),
  child: CustomScrollView(controller: _scroll, slivers: [...]),
)
```

- кольцо вокруг кнопки — это позиция в списке, поэтому отдельный индикатор прогресса не нужен;
- после прыжка наверх появляется «вернуться» на прежнее место; предложение снимается, если пользователь сам прокрутил вниз;
- анимация пропускается при `MediaQuery.disableAnimations` и на коротких прыжках, длительность зависит от расстояния;
- на экране один слушатель скролла, перерисовка только при смене целого процента;
- `Home` и `Ctrl/Cmd+Home` делают то же самое с клавиатуры: оболочка находит активный крюк через `ScrollTopHook.requestTop()`.

## Тема

Весь цвет, радиусы и тени живут в `lib/theme.dart`. Виджеты просят роль, а не оттенок:

| Роль | Токены |
|------|--------|
| фон | `ink`, `panel`, `panelRaised`, `panelSunken` |
| разделение | `line`, `lineStrong` |
| текст | `textPrimary`, `textSecondary`, `textMuted`, `textFaint`, `textGhost` |
| акцент и сигналы | `accent`, `info`, `positive`, `warning` |
| форма | `rCard`, `rControl`, `rChip`, `rTag`, `rSheet` |

Поэтому «сделать темнее» — правка одного файла, а не тридцати. Светлая тема, если понадобится, тоже сводится к переопределению этого класса.

## Примеры с изображениями

У записи в `catalog.json` есть массив `showcases` — реальные сайты, продакшен-галереи, живые демо и репозитории с кодом:

```json
"showcases": [
  {"title": "Astro Showcase", "url": "https://astro.build/showcase/", "kind": "gallery",
   "note": "Hundreds of real sites with screenshots",
   "localizedNote": {"ru": "Сотни реальных сайтов со скриншотами"}}
]
```

Поле `image` опционально. Если своей картинки нет — приложение берёт **живой скриншот сайта из интернета** через выбранный провайдер:

| Провайдер | Где берётся картинка | Ключ |
|-----------|----------------------|------|
| `mshots` (по умолчанию) | `s.wordpress.com/mshots` — рендерит страницу по URL | не нужен |
| `thumio` | `image.thum.io` | не нужен |
| `off` | внешние запросы полностью отключены | — |

mShots на первый запрос отдаёт заглушку, пока делает снимок, поэтому `ShowcaseResolver.shouldRetry()` разрешает один повтор с задержкой. Все загруженные картинки кладутся в собственный дисковый кэш (`ImageCacheService`, `path_provider`), поэтому во второй раз галерея открывается офлайн. Кэш виден и очищается в «Обновлениях».

## Ежедневное автообновление

«Автообновление» в большинстве приложений работает только пока приложение открыто. Здесь три слоя, и приложение честно показывает, какой из них активен:

| Слой | Платформа | Работает при закрытом приложении |
|------|-----------|----------------------------------|
| WorkManager periodic task | Android | да |
| Таймер внутри приложения + догоняющий запуск при старте и `resume` | все | нет |
| Системная задача → `--headless-sync` | Windows, Linux, macOS | да |

- время запуска выбирается в настройках (по умолчанию 09:00), показывается «следующее обновление»;
- `SyncRunner` — общий код для всех трёх слоёв, не трогает виджеты, поэтому безопасен в фоновом изолейте;
- если плагин недоступен или его API изменился, `BackgroundWorker` ловит ошибку и приложение деградирует до слоя 2 вместо падения при старте;
- защита от лишнего трафика: повтор не чаще чем раз в 20 часов, `--force` игнорирует ограничение.

Шаблоны системных задач — в `tool/desktop/` (systemd timer с `Persistent=true`, Task Scheduler с `-StartWhenAvailable`, оба догоняют пропущенный запуск).

## Подготовка к десктопу

- `PlatformInfo` и `Breakpoints` — единственное место, где приложение спрашивает «где я работаю» и «какая ширина»;
- на ширине ≥ 1180 px каталог переключается в master–detail: сетка слева, карточка записи в правой панели вместо модального листа;
- горячие клавиши: `Ctrl+F` поиск, `Ctrl+R` обновление, `Ctrl+1…4` разделы, `Home` наверх, `Esc` закрыть панель;
- `--headless-sync` запускает обновление без окна и завершает процесс (`0` — обновлено, `2` — пропущено);
- `tool/post_create.dart` ставит размер окна 1280×820 и человеческий заголовок для Windows и Linux.

Полный план перехода — в [DESKTOP.md](DESKTOP.md).

## Источники автообновления

- `registry.modelcontextprotocol.io` — инкрементальная синхронизация официального MCP Registry;
- GitHub Trees API — поиск `SKILL.md` в `anthropics/skills`, `anthropics/claude-plugins-official`, `vercel-labs/agent-skills`, `expo/skills`, `huggingface/skills`, `remotion-dev/skills`;
- публичные metadata API npm и PyPI — свежие версии популярных библиотек;
- циклическая проверка доступности официальных ссылок API;
- собственная HTTPS JSON-лента, если она указана в настройках.

## Ключи и цены

В проект не встроены общие секретные ключи: каталог ведёт на официальную регистрацию и различает `noKey`, `freeTier`, `openSource`, `free`, `mixed` и `paid`. Цены в `pricing` — ориентир на дату `checkedAt`, а не оффер: тарифы меняются, поэтому перед оплатой нужно открыть официальную страницу прайса кнопкой в карточке.

## Сборка APK

Быстрый путь без локального тулчейна: вкладка **Actions → APK → Run workflow**, потом скачать артефакт `vibestack-atlas-apk`. Сборка идёт с нуля: `flutter create` генерирует `android/`, дальше `post_create.dart`, `analyze`, тесты и `flutter build apk`.

Локально:

```bash
bash tool/bootstrap_platforms.sh     # flutter create + патчи + analyze + test
flutter build apk --release
```

Без keystore Flutter подписывает release-сборку debug-ключом — на телефон ставится, для Play Store нужен свой keystore.

Если `flutter pub get` не резолвит `workmanager`:

```bash
cp tool/fallback/background_worker_no_plugin.dart lib/services/sync/background_worker.dart
# и убрать строку workmanager: из pubspec.yaml
```

Приложение соберётся без плагина: пропадёт только пробуждение при закрытом приложении на Android, слои «таймер в приложении» и «системная задача на десктопе» продолжат работать, а экран «Обновления» честно покажет режим `in-app`.

## Сборка

```bash
bash tool/bootstrap_platforms.sh     # flutter create + патчи + analyze + test
flutter build apk --release
flutter build windows --release
flutter build linux --release
```

## Проверки

```bash
flutter analyze     # должно быть «No issues found!»
flutter test        # 27 тестов
```

- `catalog_integrity_test.dart` — контракт каталога: уникальные id и имена, только https, переводимая категория у каждой записи, корректные showcases, у платных записей есть цена;
- `daily_sync_test.dart` — расписание, догоняющий запуск, деградация до таймера и без планировщика, и когда планировщик отказал;
- `showcase_resolver_test.dart` — построение URL скриншота, режим `off`, повтор для mShots;
- `search_engine_test.dart`, `localization_test.dart` — поиск и три языка.

Каталог можно проверить и без Flutter SDK — то же самое на чистом Python:

```bash
python3 tool/build_catalog.py     # пересобрать assets/catalog.json и categories.dart
python3 tool/verify_catalog.py    # тот же контракт, что в catalog_integrity_test.dart
```

## Как расширять каталог

Одна запись — один литерал в одном файле, всё остальное выводится из него:

| Что добавить | Куда |
|--------------|------|
| API | `tool/catalog_src/data_apis.py` |
| библиотеку или CLI | `tool/catalog_src/data_libs.py` |
| MCP-сервер или skill | `tool/catalog_src/data_mcp_skills.py` |
| цену к существующей записи | `tool/catalog_src/data_paid.py` + строка в `MERGE` |
| примеры-витрины | `tool/catalog_src/data_showcases.py` |
| новую категорию | `CATEGORIES` в `tool/build_catalog.py` |

Дальше `python3 tool/build_catalog.py` пересобирает `assets/catalog.json` и генерирует `lib/i18n/categories.dart` с переводами. Генератор сам валится с ошибкой на дублирующемся id или имени, не-https ссылке и категории без группы, так что сломанная запись не доедет до сборки.

Учти: `tool/catalog_src/items_api.py` не подключён к генератору и использует другой формат кортежа — это остаток раннего черновика, его содержимое в основном уже есть в `data_apis.py`.

## Структура

```
lib/
  models/         catalog_item.dart, showcase.dart (Showcase + PricingInfo)
  platform/       platform_info.dart (PlatformInfo, Breakpoints)
  services/
    sync/         sync_runner.dart, daily_sync_scheduler.dart, background_worker.dart
    showcase_resolver.dart, image_cache_service.dart, live_update_service.dart
    catalog_repository.dart, search_engine.dart, ai_context.dart
  widgets/        showcase_gallery.dart, catalog_card.dart, item_detail_sheet.dart,
                  scroll_to_top.dart (ScrollTopHook)
  screens/        catalog, favorites, add_item, settings
  theme.dart      все цвета, радиусы и темы компонентов
tool/catalog_src/ пакеты записей каталога + build_catalog.py и verify_catalog.py
tool/desktop/     шаблоны системных задач для ежедневного обновления
```
