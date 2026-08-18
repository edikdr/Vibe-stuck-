# -*- coding: utf-8 -*-
"""Stage 5 API pack H: more communications, calendars, forms, misc consumer/dev services."""

APIS_5H = [
    # ---------- Коммуникации (deepen further) ----------
    ("api-plivo", "Plivo", "Коммуникации", "https://www.plivo.com/docs/", "mixed", "sms,voice,verification,messaging", "",
     "SMS, voice and verification API, often priced competitively against Twilio for high volume.",
     "API SMS, голоса и верификации, часто выгоднее Twilio при больших объёмах.",
     "Compare per-country SMS pricing directly against Twilio and Vonage — the cheapest varies by corridor."),
    ("api-whatsapp-business", "WhatsApp Business Platform", "Коммуникации", "https://developers.facebook.com/docs/whatsapp", "mixed", "whatsapp,messaging,meta,business", "",
     "Official WhatsApp API for business messaging, notifications and customer support.",
     "Официальный API WhatsApp для бизнес-сообщений, уведомлений и поддержки клиентов.",
     "Message templates need Meta approval before first use — submit them early, review takes real time."),
    ("api-telegram-mtproto", "Telegram MTProto API", "Коммуникации", "https://core.telegram.org/api", "free", "telegram,mtproto,client,advanced", "",
     "The full client protocol behind Telegram, for building alternative clients or advanced automation.",
     "Полный клиентский протокол Telegram для создания альтернативных клиентов или продвинутой автоматизации.",
     "Use the simpler Bot API instead unless you specifically need user-account-level access this provides."),
    ("api-postmark-2", "Postmark (templates)", "Коммуникации", "https://postmarkapp.com/developer/api/templates-api", "paid", "email,templates,transactional,api", "",
     "Manage and send templated transactional email through Postmark's template API.",
     "Управление и отправка шаблонных транзакционных писем через API шаблонов Postmark.",
     "Design templates in their editor, then send by template alias — no HTML string-building in application code."),

    # ---------- Юридические и налоговые API (deepen) ----------
    ("api-checkr", "Checkr", "Идентификация и KYC", "https://docs.checkr.com/", "mixed", "background-check,hr,compliance,screening", "",
     "Background check API for employment screening, integrated into hiring workflows.",
     "API проверки биографии для скрининга при найме, интегрируется в процессы найма.",
     "Turnaround time varies a lot by check type and jurisdiction — set realistic candidate expectations up front."),
    ("api-middesk", "Middesk", "Идентификация и KYC", "https://docs.middesk.com/", "paid", "business-verification,kyb,compliance,fintech", "",
     "Business verification (KYB) API: confirms a company is real and in good standing.",
     "API проверки бизнеса (KYB): подтверждает, что компания реальна и в хорошем состоянии.",
     "This is KYB, not KYC — it verifies businesses, and is often paired with a separate KYC tool for individuals."),

    # ---------- Формы и опросы (deepen) ----------
    ("api-tally-so", "Tally", "Формы и опросы", "https://tally.so/help/tally-api", "freeTier", "forms,simple,notion-like,api", "",
     "Notion-like block-based form builder with a genuinely free unlimited tier.",
     "Конструктор форм на блоках в духе Notion с по-настоящему бесплатным неограниченным тарифом.",
     "The free plan has no response cap at all — check it before assuming you need Typeform's paid tier."),
    ("api-formspree", "Formspree", "Формы и опросы", "https://formspree.io/docs/", "freeTier", "forms,backend,static-sites,email", "",
     "Form backend for static sites: point a plain HTML form at it, get email notifications.",
     "Бэкенд для форм статических сайтов: указываешь обычную HTML-форму, получаешь письма-уведомления.",
     "No JavaScript is required at all — a plain HTML form action attribute is the entire integration."),

    # ---------- Видеоконференции (deepen) ----------
    ("api-whereby-embedded", "Whereby Embedded", "Видеоконференции", "https://docs.whereby.com/", "freeTier", "video,embed,webrtc,simple", "",
     "Embeddable video rooms with minimal setup, no custom SDK integration required.",
     "Встраиваемые видеокомнаты с минимальной настройкой, без интеграции кастомного SDK.",
     "An iframe embed is the entire integration for the common case — reach for the API only for custom logic."),
    ("api-jitsi-meet", "Jitsi Meet API", "Видеоконференции", "https://jitsi.github.io/handbook/docs/dev-guide/dev-guide-iframe/", "openSource", "video,open-source,self-host,webrtc", "",
     "Open-source video conferencing, embeddable via iframe or self-hostable entirely.",
     "Открытая видеоконференцсвязь, встраивается через iframe или размещается полностью самостоятельно.",
     "Self-hosting removes per-minute costs entirely — meet.jit.si is the free hosted option to start with."),

    # ---------- Мониторинг и аналитика (deepen further) ----------
    ("api-checkly", "Checkly", "Мониторинг и аналитика", "https://www.checklyhq.com/docs/", "freeTier", "monitoring,synthetic,playwright,api", "",
     "Synthetic monitoring using real Playwright scripts to check critical user flows.",
     "Синтетический мониторинг на реальных Playwright-скриптах для проверки критичных пользовательских сценариев.",
     "Reuse your existing Playwright e2e tests as monitoring checks instead of writing separate synthetic scripts."),
    ("api-better-stack-2", "Better Stack Logs", "Мониторинг и аналитика", "https://betterstack.com/docs/logs/", "freeTier", "logs,monitoring,search,retention", "",
     "Log management with fast search, alerting and a generous free ingestion tier.",
     "Управление логами с быстрым поиском, алертами и щедрым бесплатным тарифом на приём данных.",
     "Structure logs as JSON from the start — the search and filtering value depends entirely on that."),

    # ---------- CDP и маркетинг-данные (deepen) ----------
    ("api-june-so", "June", "CDP и маркетинг-данные", "https://help.june.so/en/", "freeTier", "product-analytics,b2b,cdp,simple", "",
     "Product analytics purpose-built for B2B SaaS, simpler than general-purpose CDPs.",
     "Продуктовая аналитика, заточенная под B2B SaaS, проще, чем универсальные CDP.",
     "It auto-generates the standard SaaS dashboards from your existing Segment events — no dashboard-building needed."),

    # ---------- Дизайн-системы (deepen) ----------
    ("api-supernova-io", "Supernova", "Дизайн-системы", "https://docs.supernova.io/", "freeTier", "design-tokens,documentation,sync,figma", "",
     "Design system documentation and token pipeline that syncs from Figma to code.",
     "Документация дизайн-системы и пайплайн токенов, синхронизируется из Figma в код.",
     "The published docs site updates automatically on each sync, so design and documentation never drift apart."),

    # ---------- Rust экосистема (deepen) ----------
    ("api-shuttle-rs", "Shuttle", "Rust экосистема", "https://docs.shuttle.dev/", "freeTier", "rust,deployment,serverless,infrastructure", "",
     "Deploy Rust backends with infrastructure declared directly in code, no separate config files.",
     "Деплой Rust-бэкендов с инфраструктурой, объявленной прямо в коде, без отдельных конфигов.",
     "Annotate a resource in code (like a database) and it provisions automatically on deploy — no dashboard clicking."),
]
