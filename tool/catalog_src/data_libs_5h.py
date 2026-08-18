# -*- coding: utf-8 -*-
"""Stage 5 library pack H: more testing/data-science, image/design tooling depth."""

LIBS_5H = [
    # ---------- Данные и ML (deepen further) ----------
    ("lib-xgboost", "XGBoost", "Данные и ML", "https://xgboost.readthedocs.io/", "openSource", "gradient-boosting,tabular,ml,performance", "pip install xgboost",
     "The gradient-boosting library that wins most tabular-data ML competitions.",
     "Библиотека градиентного бустинга, выигрывающая большинство соревнований по табличным данным.",
     "Try this before a neural network on tabular data — it usually wins on both accuracy and training time."),
    ("lib-lightgbm", "LightGBM", "Данные и ML", "https://lightgbm.readthedocs.io/", "openSource", "gradient-boosting,fast,large-data,microsoft", "pip install lightgbm",
     "Microsoft's gradient-boosting framework, faster than XGBoost on large datasets.",
     "Фреймворк градиентного бустинга от Microsoft, быстрее XGBoost на больших датасетах.",
     "Its leaf-wise tree growth can overfit small datasets — prefer XGBoost below roughly ten thousand rows."),
    ("lib-catboost", "CatBoost", "Данные и ML", "https://catboost.ai/docs/", "openSource", "gradient-boosting,categorical,yandex,ml", "pip install catboost",
     "Gradient boosting with native categorical feature handling, no manual encoding required.",
     "Градиентный бустинг с нативной обработкой категориальных признаков без ручного кодирования.",
     "Pass categorical columns directly instead of one-hot encoding them — it handles the encoding internally."),
    ("lib-polars-2", "Polars (lazy API)", "Данные и ML", "https://docs.pola.rs/user-guide/lazy/", "openSource", "dataframes,lazy,rust,performance", "pip install polars",
     "The lazy evaluation mode of Polars, which builds a query plan before executing.",
     "Ленивый режим выполнения Polars, строит план запроса перед выполнением.",
     "Prefer scan_csv/lazy over read_csv/eager for anything beyond a quick interactive check — it enables real optimization."),
    ("lib-statsmodels", "statsmodels", "Данные и ML", "https://www.statsmodels.org/stable/index.html", "openSource", "statistics,regression,python,inference", "pip install statsmodels",
     "Statistical modeling in Python with proper inference: p-values, confidence intervals, diagnostics.",
     "Статистическое моделирование в Python с настоящим выводом: p-values, доверительные интервалы, диагностика.",
     "Reach for this over scikit-learn specifically when you need statistical significance, not just a prediction."),

    # ---------- Тестирование и качество (further) ----------
    ("lib-locust-2", "Locust (distributed mode)", "Тестирование и качество", "https://docs.locust.io/en/stable/running-distributed.html", "openSource", "load-testing,distributed,python,scale", "",
     "Distributed mode of Locust for generating load beyond a single machine's capacity.",
     "Распределённый режим Locust для генерации нагрузки сверх возможностей одной машины.",
     "Start with a single worker to validate the test script before scaling out to a real distributed run."),
    ("lib-artillery", "Artillery", "Тестирование и качество", "https://www.artillery.io/docs", "openSource", "load-testing,javascript,yaml,ci", "npm i -g artillery",
     "Load and performance testing configured in YAML, with JavaScript hooks for custom logic.",
     "Нагрузочное и performance-тестирование через YAML-конфиг с JS-хуками для кастомной логики.",
     "Its YAML scenarios are easy to generate from a recorded HAR file for realistic traffic patterns."),
    ("lib-madge", "Madge", "Тестирование и качество", "https://github.com/pahen/madge", "openSource", "dependencies,circular,visualization,javascript", "npm i -g madge",
     "Visualizes module dependency graphs and detects circular dependencies in JavaScript projects.",
     "Визуализирует граф зависимостей модулей и находит циклические зависимости в JS-проектах.",
     "Run --circular in CI — circular dependencies are a common, hard-to-spot source of bundler and test flakiness."),

    # ---------- Мониторинг и аналитика (client libs) ----------
    ("lib-sentry-sdk", "Sentry SDK", "Мониторинг и аналитика", "https://docs.sentry.io/platforms/", "freeTier", "errors,sdk,client,tracing", "npm i @sentry/node",
     "Client SDKs for Sentry error and performance tracking across every major language and framework.",
     "Клиентские SDK для отслеживания ошибок и производительности Sentry во всех основных языках и фреймворках.",
     "Enable release tracking and source maps together — without both, stack traces on minified frontend code are useless."),
    ("lib-opentelemetry-js", "OpenTelemetry JS", "Мониторинг и аналитика", "https://opentelemetry.io/docs/languages/js/", "openSource", "tracing,otel,javascript,instrumentation", "npm i @opentelemetry/api",
     "OpenTelemetry SDK and auto-instrumentation for Node.js and browser JavaScript.",
     "SDK OpenTelemetry и авто-инструментация для Node.js и браузерного JavaScript.",
     "Auto-instrumentation packages cover Express, HTTP and common DB clients — add manual spans only where they add insight."),

    # ---------- Изображения и дизайн (further) ----------
    ("lib-satori", "Satori", "Изображения и дизайн", "https://github.com/vercel/satori", "openSource", "svg,html-to-image,vercel,og-images", "npm i satori",
     "Converts HTML/CSS to SVG, the engine behind Vercel's dynamic Open Graph image generation.",
     "Конвертирует HTML/CSS в SVG — движок динамической генерации Open Graph изображений Vercel.",
     "Pair it with resvg or @resvg/resvg-js to rasterize the SVG output to PNG for actual image delivery."),
    ("lib-svgo", "SVGO", "Изображения и дизайн", "https://github.com/svg/svgo", "openSource", "svg,optimization,cli,build", "npm i -D svgo",
     "Optimizes SVG files by removing redundant markup, often shrinking exported files dramatically.",
     "Оптимизирует SVG-файлы, убирая избыточную разметку, часто резко уменьшает размер экспортированных файлов.",
     "Run it as a build step on exported design-tool SVGs — hand-authored SVGs rarely need it."),
]
