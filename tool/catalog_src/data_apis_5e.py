# -*- coding: utf-8 -*-
"""Stage 5 API pack E: streaming/social platforms, more open data, developer services."""

APIS_5E = [
    # ---------- Музыка и медиа (deepen) ----------
    ("api-soundcloud-api", "SoundCloud API", "Музыка и медиа", "https://developers.soundcloud.com/docs/api/guide", "mixed", "music,streaming,upload,social", "",
     "Track upload, playback and social features from the SoundCloud platform.",
     "Загрузка треков, воспроизведение и социальные функции платформы SoundCloud.",
     "New API keys are approved manually — apply early since it is not instant self-serve."),
    ("api-genius-api", "Genius API", "Музыка и медиа", "https://docs.genius.com/", "freeTier", "lyrics,annotations,music,metadata", "",
     "Song metadata, lyrics annotations and artist data from Genius.",
     "Метаданные песен, аннотации к текстам и данные артистов с Genius.",
     "Full lyrics require web scraping under their terms — the API itself returns metadata and annotations, not raw lyrics."),
    ("api-bandcamp-api", "Bandcamp API", "Музыка и медиа", "https://bandcamp.com/developer", "free", "music,indie,artists,sales", "",
     "Artist and album data plus sales reporting for Bandcamp accounts.",
     "Данные об артистах, альбомах и отчёты о продажах для аккаунтов Bandcamp.",
     "Access requires being a registered label/artist account — it is not a general public catalog API."),

    # ---------- Видео и медиа (deepen) ----------
    ("api-dailymotion-api", "Dailymotion API", "Видео и медиа", "https://developers.dailymotion.com/", "free", "video,streaming,player,embed", "",
     "Video hosting, search and embeddable player from Dailymotion.",
     "Хостинг видео, поиск и встраиваемый плеер от Dailymotion.",
     "The embed player works without any API key at all for simple playback embedding."),

    # ---------- Игры и развлечения (deepen further) ----------
    ("api-giant-bomb", "Giant Bomb API", "Игры и развлечения", "https://www.giantbomb.com/api/", "free", "games,database,reviews,metadata", "",
     "Game database with reviews, characters and franchise data.",
     "База данных игр с обзорами, персонажами и данными франшиз.",
     "Rate limits are strict on the free key — cache aggressively rather than querying per page view."),
    ("api-speedrun-com", "speedrun.com API", "Игры и развлечения", "https://github.com/speedruncomorg/api", "noKey", "speedrun,games,leaderboards,no-key", "",
     "Speedrun leaderboards, categories and world records, no key required.",
     "Таблицы лидеров спидранов, категории и мировые рекорды, ключ не нужен.",
     "Game and category IDs are opaque strings — resolve them once via the search endpoint and cache the mapping."),
    ("api-open-mtg", "Scryfall", "Игры и развлечения", "https://scryfall.com/docs/api", "noKey", "magic-the-gathering,cards,tcg,no-key", "",
     "Comprehensive Magic: The Gathering card database with prices and images, no key required.",
     "Полная база карт Magic: The Gathering с ценами и изображениями, ключ не нужен.",
     "Bulk data downloads are far more efficient than paging the API for anything covering the whole card set."),

    # ---------- Открытые данные (deepen) ----------
    ("api-data-gov", "Data.gov (CKAN)", "Открытые данные", "https://api.data.gov/", "freeTier", "government,open-data,us,catalog", "",
     "Catalog and access to thousands of official US government open datasets.",
     "Каталог и доступ к тысячам официальных открытых датасетов правительства США.",
     "One free api.data.gov key works across many participating federal agency APIs, not just data.gov itself."),
    ("api-eu-open-data", "EU Open Data Portal", "Открытые данные", "https://data.europa.eu/en/developers", "free", "government,open-data,europe,sparql", "",
     "Access to open datasets published by EU institutions and member states.",
     "Доступ к открытым датасетам, опубликованным институтами ЕС и странами-членами.",
     "Metadata is queryable via SPARQL for anyone comfortable with linked-data queries."),
    ("api-un-data", "UN Data API", "Открытые данные", "https://data.un.org/Host.aspx?Content=API", "free", "statistics,un,global,indicators", "",
     "United Nations statistical indicators across countries and topics.",
     "Статистические показатели ООН по странам и темам.",
     "Indicator codes are UN-specific, not ISO — look them up in the UNdata catalog before querying."),

    # ---------- Наука и исследования (deepen further) ----------
    ("api-orcid", "ORCID API", "Наука и исследования", "https://info.orcid.org/documentation/", "free", "researchers,identifiers,academic,no-key", "",
     "Persistent researcher identifiers linking people to their publications and affiliations.",
     "Постоянные идентификаторы исследователей, связывающие людей с публикациями и аффилиациями.",
     "Public API access needs no auth for reading a researcher's public record."),
    ("api-figshare", "Figshare API", "Наука и исследования", "https://docs.figshare.com/", "free", "research-data,datasets,publishing,doi", "",
     "Upload, publish and cite research data and figures with a DOI.",
     "Загрузка, публикация и цитирование исследовательских данных и рисунков с DOI.",
     "Every published item gets a citable DOI automatically — useful for data that needs to be formally cited."),

    # ---------- Локализация и i18n (deepen) ----------
    ("api-lokalise", "Lokalise API", "Локализация и i18n", "https://developers.lokalise.com/reference/", "freeTier", "localization,translations,i18n,tms", "",
     "Translation management platform API: push source strings, pull translations.",
     "API платформы управления переводами: отправка исходных строк, получение переводов.",
     "CI integration (push on deploy, pull before build) keeps translations from drifting out of sync with code."),
    ("api-crowdin", "Crowdin API", "Локализация и i18n", "https://developer.crowdin.com/api/v2/", "freeTier", "localization,translations,i18n,community", "",
     "Localization platform with strong support for community and crowdsourced translation.",
     "Платформа локализации с сильной поддержкой сообщества и краудсорсинговых переводов.",
     "Open-source projects get a free plan — check eligibility before assuming you need the paid tier."),
    ("api-phrase", "Phrase (Localization)", "Локализация и i18n", "https://developers.phrase.com/api/", "freeTier", "localization,translations,i18n,tms", "",
     "Translation management with in-context editing and strong CLI/CI tooling.",
     "Управление переводами с редактированием в контексте и сильной поддержкой CLI/CI.",
     "In-context editing lets translators see the real UI while translating, which cuts review cycles significantly."),

    # ---------- CMS и контент (deepen) ----------
    ("api-sanity-io", "Sanity", "CMS и контент", "https://www.sanity.io/docs", "freeTier", "cms,headless,structured-content,groq", "npm create sanity@latest",
     "Headless CMS with a real-time collaborative editor and its own GROQ query language.",
     "Headless CMS с совместным редактором в реальном времени и собственным языком запросов GROQ.",
     "Model content as structured documents from day one — retrofitting structure onto free-text later is painful."),
    ("api-contentful", "Contentful", "CMS и контент", "https://www.contentful.com/developers/docs/", "freeTier", "cms,headless,api-first,enterprise", "",
     "API-first headless CMS widely used at enterprise scale.",
     "API-first headless CMS, широко используется на корпоративном масштабе.",
     "The Content Delivery API is read-only and CDN-cached — use the Management API only for authoring flows."),
    ("api-payload-cms-2", "Payload", "CMS и контент", "https://payloadcms.com/docs", "openSource", "cms,headless,typescript,self-host", "npx create-payload-app@latest",
     "TypeScript-native headless CMS that is also a full backend framework.",
     "TypeScript-нативная headless CMS, одновременно полноценный backend-фреймворк.",
     "It generates a working admin UI from your TypeScript config — no separate admin app to build."),

    # ---------- Погода (deepen further) ----------
    ("api-ambee", "Ambee", "Погода и геоданные", "https://api-docs.ambeedata.com/", "freeTier", "weather,air-quality,pollen,environmental", "",
     "Environmental data: air quality, pollen, fire risk and weather in one API.",
     "Экологические данные: качество воздуха, пыльца, риск пожаров и погода в одном API.",
     "Air quality and pollen are separate paid add-ons from basic weather — check the plan tiers carefully."),
    ("api-airvisual", "IQAir AirVisual", "Погода и геоданные", "https://www.iqair.com/us/commercial-air-quality-monitors/airvisual-platform/api", "freeTier", "air-quality,pollution,weather,environmental", "",
     "Real-time and forecast air quality data for cities worldwide.",
     "Данные о качестве воздуха в реальном времени и прогноз для городов по всему миру.",
     "Nearest-city lookup by coordinates is more reliable than assuming an exact city name match."),

    # ---------- Транспорт (deepen further) ----------
    ("api-uber-api", "Uber API", "Транспорт и город", "https://developer.uber.com/docs", "mixed", "rideshare,transport,pricing,eta", "",
     "Ride price estimates, ETAs and trip requests through Uber's developer platform.",
     "Оценка стоимости поездки, время прибытия и запрос поездок через платформу разработчиков Uber.",
     "Price estimate endpoints need no user auth; requesting an actual ride requires full OAuth."),
    ("api-lyft-api", "Lyft API", "Транспорт и город", "https://developer.lyft.com/docs/overview", "mixed", "rideshare,transport,pricing,eta", "",
     "Ride estimates and booking through Lyft's public developer API.",
     "Оценка поездок и бронирование через публичный API разработчиков Lyft.",
     "Coverage is US/Canada-only — check market availability before assuming global reach."),
]
