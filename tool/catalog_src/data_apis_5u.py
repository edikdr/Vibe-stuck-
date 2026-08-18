# -*- coding: utf-8 -*-
"""Stage 5 API pack U: workflow/BPM, docs signing, translation memory, misc business."""

APIS_5U = [
    # ---------- Очереди и события: workflow engines ----------
    ("api-camunda", "Camunda", "Очереди и события", "https://docs.camunda.io/", "freeTier", "bpmn,workflow,orchestration,process", "",
     "BPMN-based process orchestration for business workflows with a visual modeler.",
     "Оркестрация бизнес-процессов на BPMN с визуальным моделировщиком.",
     "BPMN diagrams are the executable artifact, which lets non-developers review the actual process logic."),
    ("api-activepieces", "Activepieces", "Разработка и DevOps", "https://www.activepieces.com/docs", "openSource", "automation,no-code,self-host,typescript", "",
     "Open-source no-code automation platform, an alternative to Zapier and Make.",
     "Открытая no-code платформа автоматизации, альтернатива Zapier и Make.",
     "Its pieces are TypeScript packages, so writing a custom integration is a normal npm workflow."),
    ("api-kestra", "Kestra", "Data engineering", "https://kestra.io/docs", "openSource", "orchestration,yaml,declarative,data", "",
     "Declarative YAML-based orchestration for data and infrastructure workflows.",
     "Декларативная оркестрация workflow данных и инфраструктуры на YAML.",
     "YAML definitions make workflows reviewable in a PR without reading Python DAG code."),
    ("api-restate", "Restate", "Очереди и события", "https://docs.restate.dev/", "openSource", "durable-execution,workflows,typescript,resilient", "",
     "Durable execution engine for resilient microservice workflows.",
     "Движок durable execution для устойчивых микросервисных workflow.",
     "Lighter-weight than Temporal for teams that want durability without running a separate cluster."),

    # ---------- Юридические и налоговые API (deepen) ----------
    ("api-dropbox-sign", "Dropbox Sign", "Юридические и налоговые API", "https://developers.hellosign.com/", "freeTier", "esignature,contracts,embedded,legal", "",
     "E-signature API with embedded signing, formerly HelloSign.",
     "API электронной подписи со встраиваемым подписанием, ранее HelloSign.",
     "Its test mode allows unlimited signature requests, so the whole flow can be built before paying."),
    ("api-pandadoc", "PandaDoc", "Юридические и налоговые API", "https://developers.pandadoc.com/", "mixed", "documents,esignature,proposals,templates", "",
     "Document automation with templates, e-signature and payment collection.",
     "Автоматизация документов с шаблонами, электронной подписью и сбором платежей.",
     "Templates with variable tokens turn contract generation into a data-merge rather than document editing."),
    ("api-signwell", "SignWell", "Юридические и налоговые API", "https://www.signwell.com/api/", "freeTier", "esignature,documents,simple,legal", "",
     "Straightforward e-signature API with a usable free tier for low volume.",
     "Простой API электронной подписи с рабочим бесплатным тарифом для небольших объёмов.",
     "Cheaper than DocuSign for basic signature workflows without enterprise compliance requirements."),

    # ---------- HR и рекрутинг (deepen) ----------
    ("api-deel", "Deel API", "HR и рекрутинг", "https://developer.deel.com/", "paid", "payroll,global,contractors,eor", "",
     "Global payroll, contractor and employer-of-record APIs for international hiring.",
     "API глобальных зарплат, подрядчиков и employer-of-record для международного найма.",
     "EOR handles legal employment in countries where you have no entity, which is the core value."),
    ("api-remote-com", "Remote API", "HR и рекрутинг", "https://developer.remote.com/", "paid", "eor,payroll,global,compliance", "",
     "Employer-of-record and global payroll platform API.",
     "API платформы employer-of-record и глобальных зарплат.",
     "Compare EOR per-employee fees against Deel directly — they differ meaningfully by country."),

    # ---------- Переводы и языки (deepen) ----------
    ("api-weglot", "Weglot", "Локализация и i18n", "https://developers.weglot.com/", "paid", "translation,website,proxy,seo", "",
     "Website translation layer that proxies and translates pages without code changes.",
     "Слой перевода сайта, который проксирует и переводит страницы без изменений в коде.",
     "SEO-friendly because translated pages get their own URLs, unlike client-side translation widgets."),
    ("api-localazy", "Localazy", "Локализация и i18n", "https://localazy.com/docs", "freeTier", "localization,translations,mobile,cli", "",
     "Localization management with a strong CLI and mobile app focus.",
     "Управление локализацией с сильным CLI и фокусом на мобильные приложения.",
     "Its shared community translations cover common UI strings for free across projects."),
    ("api-tolgee", "Tolgee", "Локализация и i18n", "https://tolgee.io/platform", "openSource", "localization,in-context,open-source,self-host", "",
     "Open-source localization platform with in-context translation editing in your running app.",
     "Открытая платформа локализации с редактированием переводов прямо в работающем приложении.",
     "In-context editing means translators see the real UI, which catches length and context errors early."),

    # ---------- Образование (deepen) ----------
    ("api-moodle", "Moodle Web Services", "Образование", "https://docs.moodle.org/dev/Web_service_API_functions", "openSource", "lms,education,self-host,courses", "",
     "Web service API for the widely deployed open-source Moodle LMS.",
     "API веб-сервисов для широко распространённой открытой LMS Moodle.",
     "Each web service function must be explicitly enabled by an admin before it becomes callable."),
    ("api-openedx", "Open edX APIs", "Образование", "https://docs.openedx.org/en/latest/developers/references/", "openSource", "lms,mooc,education,self-host", "",
     "REST APIs for the Open edX learning platform used by many universities.",
     "REST API платформы обучения Open edX, используемой многими университетами.",
     "Heavier to self-host than Moodle — evaluate the operational cost before committing to it."),

    # ---------- Недвижимость / логистика (deepen) ----------
    ("api-mapbox-directions", "Mapbox Directions API", "Карты и навигация", "https://docs.mapbox.com/api/navigation/directions/", "freeTier", "routing,navigation,traffic,maps", "",
     "Turn-by-turn routing with live traffic across driving, walking and cycling profiles.",
     "Пошаговая маршрутизация с учётом пробок для авто, пешеходов и велосипедистов.",
     "The matrix endpoint computes many-to-many travel times far more cheaply than looping directions calls."),
    ("api-graphhopper", "GraphHopper", "Карты и навигация", "https://docs.graphhopper.com/", "freeTier", "routing,osm,optimization,self-host", "",
     "Routing and route optimization on OpenStreetMap data, self-hostable.",
     "Маршрутизация и оптимизация маршрутов на данных OpenStreetMap, можно разместить самостоятельно.",
     "Its route optimization solves vehicle-routing problems, which plain directions APIs do not address."),
    ("api-valhalla", "Valhalla", "Карты и навигация", "https://valhalla.github.io/valhalla/", "openSource", "routing,osm,self-host,isochrones", "",
     "Open-source routing engine for OpenStreetMap with isochrones and map matching.",
     "Открытый движок маршрутизации для OpenStreetMap с изохронами и map matching.",
     "Self-hosting eliminates per-request routing costs entirely, at the cost of managing large tile data."),
]
