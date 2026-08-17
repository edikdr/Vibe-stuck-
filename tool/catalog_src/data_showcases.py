# -*- coding: utf-8 -*-
# id -> [(title, url, kind, note_en, note_ru)]
# kind: gallery | site | demo | code
SHOWCASES = {
"lib-react": [
 ("React Showcase Sites", "https://react.dev/community/showcase", "gallery", "Official community showcase", "Официальная витрина сообщества"),
 ("react.dev", "https://react.dev/", "site", "The docs site itself is a React app", "Сам сайт документации написан на React"),
],
"lib-nextjs": [
 ("Next.js Showcase", "https://nextjs.org/showcase", "gallery", "Production sites built with Next.js", "Продакшен-сайты, собранные на Next.js"),
 ("vercel.com", "https://vercel.com/", "site", "Vercel's own site runs on Next.js", "Сайт самого Vercel работает на Next.js"),
],
"lib-vue": [
 ("Made with Vue.js", "https://madewithvuejs.com/", "gallery", "Community gallery of Vue projects", "Галерея проектов на Vue от сообщества"),
],
"lib-nuxt": [
 ("Nuxt Showcase", "https://nuxt.com/showcase", "gallery", "Sites and apps built with Nuxt", "Сайты и приложения на Nuxt"),
],
"lib-svelte": [
 ("Svelte Showcase", "https://svelte.dev/", "site", "Svelte homepage and interactive tutorial", "Главная Svelte с интерактивным туториалом"),
 ("Svelte Society", "https://www.sveltesociety.dev/", "gallery", "Community components and templates", "Компоненты и шаблоны сообщества"),
],
"lib-sveltekit": [
 ("SvelteKit demo", "https://kit.svelte.dev/", "site", "Framework site built on SvelteKit", "Сайт фреймворка собран на SvelteKit"),
],
"lib-astro": [
 ("Astro Showcase", "https://astro.build/showcase/", "gallery", "Hundreds of real sites with screenshots", "Сотни реальных сайтов со скриншотами"),
 ("Astro Themes", "https://astro.build/themes/", "gallery", "Ready templates you can copy", "Готовые шаблоны, которые можно скопировать"),
],
"lib-tailwind": [
 ("Tailwind Showcase", "https://tailwindcss.com/showcase", "gallery", "Well known sites styled with Tailwind", "Известные сайты, свёрстанные на Tailwind"),
 ("Tailwind Play", "https://play.tailwindcss.com/", "demo", "Live sandbox for utility classes", "Живая песочница для utility-классов"),
],
"lib-shadcn": [
 ("shadcn/ui Examples", "https://ui.shadcn.com/examples/dashboard", "demo", "Full dashboard example in the browser", "Полноценный пример дашборда в браузере"),
 ("Blocks library", "https://ui.shadcn.com/blocks", "gallery", "Copy-paste page sections", "Готовые секции страниц для копирования"),
],
"lib-mui": [
 ("MUI Templates", "https://mui.com/material-ui/getting-started/templates/", "gallery", "Free dashboard and landing templates", "Бесплатные шаблоны дашбордов и лендингов"),
],
"lib-chakra": [
 ("Chakra UI Showcase", "https://chakra-ui.com/showcase", "gallery", "Products built with Chakra", "Продукты, собранные на Chakra"),
],
"lib-ant-design": [
 ("Ant Design Pro", "https://pro.ant.design/", "demo", "Complete admin system preview", "Живое превью полноценной админки"),
],
"lib-radix": [
 ("Radix Primitives demos", "https://www.radix-ui.com/primitives", "demo", "Every primitive with a live example", "Каждый примитив с живым примером"),
],
"lib-motion": [
 ("Motion Examples", "https://examples.motion.dev/", "gallery", "Runnable animation recipes", "Готовые анимации, которые можно запустить"),
],
"lib-threejs": [
 ("Three.js Examples", "https://threejs.org/examples/", "gallery", "Hundreds of runnable 3D scenes", "Сотни запускаемых 3D-сцен"),
 ("Bruno Simon portfolio", "https://bruno-simon.com/", "site", "Famous WebGL portfolio built on Three.js", "Знаменитое WebGL-портфолио на Three.js"),
],
"lib-d3": [
 ("D3 Gallery", "https://observablehq.com/@d3/gallery", "gallery", "Official chart gallery with source", "Официальная галерея графиков с исходниками"),
],
"lib-echarts": [
 ("ECharts Examples", "https://echarts.apache.org/examples/en/index.html", "gallery", "Editable chart examples", "Редактируемые примеры графиков"),
],
"lib-chartjs": [
 ("Chart.js Samples", "https://www.chartjs.org/docs/latest/samples/information.html", "gallery", "Official sample charts", "Официальные примеры графиков"),
],
"lib-flutter": [
 ("Flutter Showcase", "https://flutter.dev/showcase", "gallery", "Shipped apps built with Flutter", "Выпущенные приложения на Flutter"),
 ("Flutter Gallery", "https://flutter.github.io/samples/web/material_3_demo/", "demo", "Material 3 demo running in the browser", "Демо Material 3 прямо в браузере"),
],
"lib-tauri": [
 ("Tauri Showcase", "https://v2.tauri.app/showcase/", "gallery", "Desktop apps built with Tauri", "Desktop-приложения, собранные на Tauri"),
],
"lib-electron": [
 ("Electron Apps", "https://www.electronjs.org/apps", "gallery", "Directory of shipped Electron apps", "Каталог выпущенных Electron-приложений"),
],
"lib-django": [
 ("Django Sites", "https://www.djangoproject.com/", "site", "Framework home with case studies", "Сайт фреймворка с кейсами"),
 ("Django Packages", "https://djangopackages.org/", "site", "Community site itself built on Django", "Сайт сообщества, сам написанный на Django"),
],
"lib-fastapi": [
 ("FastAPI interactive docs", "https://fastapi.tiangolo.com/tutorial/first-steps/", "demo", "Auto-generated Swagger UI per project", "Автогенерируемый Swagger UI в каждом проекте"),
],
"lib-storybook": [
 ("Storybook Showcase", "https://storybook.js.org/showcase", "gallery", "Public design systems in Storybook", "Публичные дизайн-системы в Storybook"),
],
"lib-playwright": [
 ("Trace Viewer demo", "https://trace.playwright.dev/", "demo", "Open a real test trace in the browser", "Открывает реальный trace теста в браузере"),
],
"lib-htmx": [
 ("htmx Examples", "https://htmx.org/examples/", "gallery", "Common UI patterns without a framework", "Типовые UI-паттерны без фреймворка"),
],
"lib-alpine": [
 ("Alpine components", "https://alpinejs.dev/components", "gallery", "Ready interactive components", "Готовые интерактивные компоненты"),
],
"lib-lit": [
 ("Lit Playground", "https://lit.dev/playground/", "demo", "Editable web component sandbox", "Редактируемая песочница веб-компонентов"),
],
"lib-solid": [
 ("Solid Playground", "https://playground.solidjs.com/", "demo", "Live reactive playground", "Живая реактивная песочница"),
],
"lib-eleventy": [
 ("Eleventy Built With", "https://www.11ty.dev/docs/sites/", "gallery", "Sites generated with Eleventy", "Сайты, собранные на Eleventy"),
],
"lib-hugo": [
 ("Hugo Showcase", "https://gohugo.io/showcase/", "gallery", "Real sites with technical write-ups", "Реальные сайты с техническими разборами"),
 ("Hugo Themes", "https://themes.gohugo.io/", "gallery", "Theme gallery with live previews", "Галерея тем с живыми превью"),
],
"lib-docusaurus": [
 ("Docusaurus Showcase", "https://docusaurus.io/showcase", "gallery", "Documentation sites in production", "Продакшен-сайты документации"),
],
"lib-vitepress": [
 ("VitePress Showcase", "https://vitepress.dev/", "site", "Vue and Vite docs run on VitePress", "Документация Vue и Vite работает на VitePress"),
],
"lib-gsap": [
 ("GSAP Showcase", "https://gsap.com/showcase/", "gallery", "Award-winning animated sites", "Отмеченные наградами анимированные сайты"),
 ("GSAP Demos", "https://gsap.com/demos/", "demo", "Editable CodePen collections", "Редактируемые коллекции на CodePen"),
],
"lib-animejs": [
 ("Anime.js examples", "https://animejs.com/documentation/", "demo", "Inline runnable examples in the docs", "Запускаемые примеры прямо в документации"),
],
"lib-lenis": [
 ("Lenis showcase", "https://lenis.darkroom.engineering/", "site", "The library site is its own demo", "Сайт библиотеки — сам себе демо"),
],
"lib-swiper": [
 ("Swiper Demos", "https://swiperjs.com/demos", "gallery", "Every slider mode as a live demo", "Все режимы слайдера в живом демо"),
],
"lib-leaflet": [
 ("Leaflet Quick Start map", "https://leafletjs.com/examples/quick-start/", "demo", "Working map in twenty lines", "Рабочая карта в двадцать строк"),
],
"lib-maplibre": [
 ("MapLibre examples", "https://maplibre.org/maplibre-gl-js/docs/examples/", "gallery", "Vector map recipes with code", "Рецепты векторных карт с кодом"),
],
"lib-apexcharts": [
 ("ApexCharts demos", "https://apexcharts.com/javascript-chart-demos/", "gallery", "Dashboard-ready chart demos", "Демо графиков, готовых для дашбордов"),
],
"lib-plotly": [
 ("Plotly.js gallery", "https://plotly.com/javascript/", "gallery", "Scientific chart gallery", "Галерея научных графиков"),
],
"lib-p5": [
 ("p5.js Examples", "https://p5js.org/examples/", "gallery", "Creative coding sketches", "Скетчи креативного кодинга"),
 ("OpenProcessing", "https://openprocessing.org/", "gallery", "Community sketches you can fork", "Скетчи сообщества, которые можно форкнуть"),
],
"lib-pixi": [
 ("PixiJS Examples", "https://pixijs.com/8.x/examples", "gallery", "Runnable rendering examples", "Запускаемые примеры рендеринга"),
],
"lib-phaser": [
 ("Phaser Examples", "https://phaser.io/examples", "gallery", "Hundreds of playable game snippets", "Сотни играбельных игровых примеров"),
],
"lib-tone": [
 ("Tone.js Demos", "https://tonejs.github.io/demos", "demo", "Playable synth and effect demos", "Играбельные демо синтезаторов и эффектов"),
],
"lib-wavesurfer": [
 ("WaveSurfer examples", "https://wavesurfer.xyz/examples/", "gallery", "Waveform players with code", "Плееры с волновой формой и кодом"),
],
"lib-plyr": [
 ("Plyr demo", "https://plyr.io/", "demo", "Player demo on the home page", "Демо плеера прямо на главной"),
],
"lib-fusejs": [
 ("Fuse.js live demo", "https://www.fusejs.io/demo.html", "demo", "Tune fuzzy options against sample data", "Настройка нечёткого поиска на демо-данных"),
],
"lib-dexie": [
 ("Dexie samples", "https://dexie.org/docs/Samples", "code", "Offline app samples with source", "Примеры офлайн-приложений с исходниками"),
],
"lib-streamlit": [
 ("Streamlit Gallery", "https://streamlit.io/gallery", "gallery", "Live data apps you can open", "Живые дата-приложения, которые можно открыть"),
],
"lib-gradio": [
 ("Hugging Face Spaces", "https://huggingface.co/spaces", "gallery", "Thousands of running Gradio demos", "Тысячи работающих Gradio-демо"),
],
"lib-expo": [
 ("Expo Showcase", "https://expo.dev/showcase", "gallery", "Store apps built with Expo", "Приложения из сторов, собранные на Expo"),
],
"lib-compose": [
 ("Now in Android app", "https://github.com/android/nowinandroid", "code", "Google's full sample app in Compose", "Полноценное эталонное приложение Google на Compose"),
],
"lib-kmp": [
 ("KMP case studies", "https://kotlinlang.org/lp/multiplatform/case-studies/", "gallery", "Shipped apps sharing Kotlin code", "Выпущенные приложения с общим Kotlin-кодом"),
],
"lib-riverpod": [
 ("Riverpod examples", "https://github.com/rrousselGit/riverpod/tree/master/examples", "code", "Official example apps", "Официальные примеры приложений"),
],
"lib-capacitor": [
 ("Capacitor showcase", "https://capacitorjs.com/", "site", "Web apps shipped to the app stores", "Веб-приложения, выпущенные в сторы"),
],
"api-open-meteo": [
 ("Open-Meteo web demo", "https://open-meteo.com/en/docs", "demo", "Interactive request builder with a live chart", "Интерактивный конструктор запросов с живым графиком"),
],
"api-brightsky": [
 ("Bright Sky demo", "https://brightsky.dev/demo/", "demo", "Live German weather from DWD data", "Живая погода Германии по данным DWD"),
],
"api-nasa": [
 ("Astronomy Picture of the Day", "https://apod.nasa.gov/apod/astropix.html", "site", "The classic site behind the APOD endpoint", "Классический сайт за эндпоинтом APOD"),
 ("NASA Image Library", "https://images.nasa.gov/", "site", "Public gallery served by the same APIs", "Публичная галерея на тех же API"),
],
"api-pokeapi": [
 ("Pokédex demo", "https://pokeapi.co/", "site", "Search widget on the API home page", "Виджет поиска на главной странице API"),
],
"api-tmdb": [
 ("TMDB site", "https://www.themoviedb.org/", "site", "The movie database the API serves", "Кинобаза, которую отдаёт этот API"),
],
"api-unsplash": [
 ("Unsplash", "https://unsplash.com/", "site", "The photo site behind the API", "Фотосайт, стоящий за этим API"),
],
"api-itunes-search": [
 ("Apple Music preview", "https://music.apple.com/", "site", "Same catalog the search API returns", "Тот же каталог, что отдаёт search API"),
],
"api-lrclib": [
 ("LRCLIB search", "https://lrclib.net/", "site", "Lyrics search running on the public API", "Поиск текстов, работающий на публичном API"),
],
"api-theaudiodb": [
 ("TheAudioDB", "https://www.theaudiodb.com/", "site", "Artist pages built from the same data", "Страницы артистов на тех же данных"),
],
"api-musicbrainz": [
 ("MusicBrainz", "https://musicbrainz.org/", "site", "Open music encyclopedia behind the API", "Открытая музыкальная энциклопедия за API"),
],
"api-openverse": [
 ("Openverse search", "https://openverse.org/", "site", "Reference client for the same API", "Эталонный клиент того же API"),
],
"api-picsum": [
 ("Lorem Picsum", "https://picsum.photos/", "demo", "Images generated straight from the URL", "Изображения генерируются прямо из URL"),
],
"api-wikimedia": [
 ("Wikipedia", "https://www.wikipedia.org/", "site", "The largest consumer of these APIs", "Крупнейший потребитель этих API"),
],
"api-rest-countries": [
 ("REST Countries", "https://restcountries.com/", "site", "Docs with live example responses", "Документация с живыми примерами ответов"),
],
"api-db-transport": [
 ("DB REST playground", "https://v6.db.transport.rest/", "demo", "Live departures in your browser", "Живые отправления прямо в браузере"),
],
"api-lichess": [
 ("Lichess", "https://lichess.org/", "site", "Full open source chess platform", "Полностью открытая шахматная платформа"),
],
"api-rickandmorty": [
 ("API home with live queries", "https://rickandmortyapi.com/", "site", "Try REST and GraphQL in place", "Можно попробовать REST и GraphQL на месте"),
],
"api-dummyjson": [
 ("DummyJSON products", "https://dummyjson.com/products", "demo", "Raw JSON you can fetch immediately", "Сырой JSON, который можно запросить сразу"),
],
"api-httpbin": [
 ("httpbin", "https://httpbin.org/", "demo", "Interactive endpoint explorer", "Интерактивный обозреватель эндпоинтов"),
],
"api-shields": [
 ("Shields.io badge builder", "https://shields.io/badges", "demo", "Compose a badge and copy the URL", "Собрать бейдж и скопировать URL"),
],
"api-qrserver": [
 ("goQR API demo", "https://goqr.me/api/", "demo", "Generate a QR code from a URL", "Сгенерировать QR-код прямо из URL"),
],
"api-stripe": [
 ("Stripe Checkout demo", "https://checkout.stripe.dev/", "demo", "Official test checkout you can complete", "Официальный тестовый checkout, который можно пройти"),
],
"api-supabase": [
 ("Supabase examples", "https://supabase.com/docs/guides/getting-started", "gallery", "Starter apps for several frameworks", "Стартовые приложения для нескольких фреймворков"),
],
"mcp-registry": [
 ("MCP Registry API", "https://registry.modelcontextprotocol.io/docs", "demo", "Browse published servers over HTTP", "Просмотр опубликованных серверов по HTTP"),
],
"mcp-inspector": [
 ("Inspector screenshots", "https://github.com/modelcontextprotocol/inspector", "code", "Interface screenshots in the README", "Скриншоты интерфейса в README"),
],
"mcp-playwright": [
 ("Playwright MCP repo", "https://github.com/microsoft/playwright-mcp", "code", "Usage examples and tool list", "Примеры использования и список инструментов"),
],
"mcp-context7": [
 ("Context7", "https://context7.com/", "site", "Web interface over the same index", "Веб-интерфейс над тем же индексом"),
],
"skill-anthropic-catalog": [
 ("Anthropic skills repo", "https://github.com/anthropics/skills", "code", "Reference SKILL.md files to copy from", "Эталонные SKILL.md, с которых можно копировать"),
],
"skill-agents-md": [
 ("agents.md", "https://agents.md/", "site", "The convention plus example files", "Соглашение и примеры файлов"),
],
"skill-awesome-claude-code": [
 ("Awesome Claude Code", "https://github.com/hesreallyhim/awesome-claude-code", "code", "Curated commands and workflows", "Подобранные команды и workflow"),
],
"lib-vite": [
 ("Vite templates", "https://vite.new/", "demo", "Spin up a starter in the browser", "Создать стартер прямо в браузере"),
],
"lib-tanstack-query": [
 ("TanStack Query examples", "https://tanstack.com/query/latest/docs/framework/react/examples/simple", "demo", "Runnable data-fetching examples", "Запускаемые примеры загрузки данных"),
],
"lib-zod": [
 ("Zod playground", "https://zod.dev/", "demo", "Schema examples inline in the docs", "Примеры схем прямо в документации"),
],
"lib-ollama": [
 ("Ollama model library", "https://ollama.com/library", "gallery", "Local models you can pull today", "Локальные модели, которые можно скачать сейчас"),
],
"lib-openwebui": [
 ("Open WebUI demo", "https://docs.openwebui.com/", "site", "Interface screenshots and setup", "Скриншоты интерфейса и установка"),
],
"lib-docker": [
 ("Docker Hub", "https://hub.docker.com/", "gallery", "Public images ready to run", "Публичные образы, готовые к запуску"),
],

# ==================== Stage 4 additions ====================

"lib-qwik": [
 ("Qwik Showcase", "https://qwik.dev/showcase/", "gallery", "Sites built with Qwik", "Сайты, собранные на Qwik"),
 ("Qwik Playground", "https://qwik.dev/playground/", "demo", "Edit and run in the browser", "Правка и запуск в браузере"),
],
"lib-mantine": [
 ("Mantine UI Components", "https://ui.mantine.dev/", "gallery", "Ready-made component compositions", "Готовые композиции компонентов"),
],
"lib-observable-plot": [
 ("Plot Gallery", "https://observablehq.com/@observablehq/plot-gallery", "gallery", "Dozens of annotated chart examples", "Десятки разобранных примеров графиков"),
],
"lib-visx": [
 ("visx Gallery", "https://airbnb.io/visx/gallery", "gallery", "Every primitive with live code", "Все примитивы с живым кодом"),
],
"lib-deck-gl": [
 ("deck.gl Examples", "https://deck.gl/examples", "gallery", "Large geospatial datasets rendered live", "Большие геоданные в живом рендере"),
],
"lib-react-three-fiber": [
 ("R3F Examples", "https://r3f.docs.pmnd.rs/getting-started/examples", "gallery", "Interactive scenes with source", "Интерактивные сцены с исходниками"),
],
"lib-babylon": [
 ("Babylon.js Playground", "https://playground.babylonjs.com/", "demo", "Live scene editor in the browser", "Живой редактор сцены в браузере"),
 ("Babylon.js Demos", "https://www.babylonjs.com/community/", "gallery", "Community projects and games", "Проекты и игры сообщества"),
],
"lib-rive": [
 ("Rive Community", "https://rive.app/community/", "gallery", "Remixable animation files", "Анимации, доступные для ремикса"),
],
"lib-lottie": [
 ("LottieFiles", "https://lottiefiles.com/", "gallery", "Free animation library with previews", "Бесплатная библиотека анимаций с превью"),
],
"lib-lucide": [
 ("Lucide Icon Search", "https://lucide.dev/icons/", "demo", "Searchable icon browser", "Иконки с поиском"),
],
"lib-phosphor": [
 ("Phosphor Icon Browser", "https://phosphoricons.com/", "demo", "All six weights side by side", "Все шесть весов рядом"),
],
"lib-iconify": [
 ("Icon Sets Browser", "https://icon-sets.iconify.design/", "gallery", "Search across two hundred sets", "Поиск по двумстам наборам"),
],
"lib-open-props": [
 ("Open Props Demos", "https://open-props.style/", "site", "Every token rendered on the page", "Все токены показаны на странице"),
],
"lib-uplot": [
 ("uPlot Demos", "https://leeoniya.github.io/uPlot/demos/index.html", "gallery", "Benchmarks with millions of points", "Бенчмарки с миллионами точек"),
],
"lib-codemirror": [
 ("CodeMirror Examples", "https://codemirror.net/examples/", "gallery", "Configured editors you can type in", "Настроенные редакторы, в которых можно печатать"),
],
"lib-lexical": [
 ("Lexical Playground", "https://playground.lexical.dev/", "demo", "Full-featured editor demo", "Демо редактора со всеми возможностями"),
],
"lib-transformers-js": [
 ("Transformers.js Spaces", "https://huggingface.co/spaces?search=transformers.js", "gallery", "Models running fully in the browser", "Модели, работающие целиком в браузере"),
],
"lib-matplotlib": [
 ("Matplotlib Gallery", "https://matplotlib.org/stable/gallery/index.html", "gallery", "Every plot type with source", "Все типы графиков с исходниками"),
],
"lib-altair": [
 ("Altair Example Gallery", "https://altair-viz.github.io/gallery/index.html", "gallery", "Declarative charts with code", "Декларативные графики с кодом"),
],
"lib-unocss": [
 ("UnoCSS Interactive Docs", "https://unocss.dev/interactive/", "demo", "Type a class and see the rule", "Введи класс и увидишь правило"),
],
"api-dicebear": [
 ("DiceBear Styles", "https://www.dicebear.com/styles/", "gallery", "Every avatar style with live seeds", "Все стили аватаров с живыми сидами"),
],
"api-turnstile": [
 ("Turnstile Demo", "https://demo.turnstile.workers.dev/", "demo", "The widget in its real states", "Виджет в реальных состояниях"),
],
"api-gotenberg": [
 ("Gotenberg Live Demo", "https://demo.gotenberg.dev/", "demo", "Convert a file without installing anything", "Конвертация файла без установки"),
],
"api-langfuse": [
 ("Langfuse Public Demo", "https://langfuse.com/docs/demo", "demo", "A real project with live traces", "Реальный проект с живыми трейсами"),
],
"api-spacex": [
 ("SpaceX API Docs", "https://github.com/r-spacex/SpaceX-API/tree/master/docs", "code", "Endpoint reference and examples", "Справочник эндпоинтов и примеры"),
],
"api-deck-of-cards": [
 ("Deck of Cards API", "https://deckofcardsapi.com/", "site", "Interactive endpoint walkthrough", "Интерактивный разбор эндпоинтов"),
],
"lib-fzf": [
 ("fzf Examples", "https://github.com/junegunn/fzf/blob/master/ADVANCED.md", "code", "Advanced pickers built from pipes", "Продвинутые пикеры из пайпов"),
],
"lib-gh-cli": [
 ("gh Manual", "https://cli.github.com/manual/", "site", "Every command with examples", "Все команды с примерами"),
],
}
