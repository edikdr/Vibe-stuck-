# -*- coding: utf-8 -*-
"""Stage 5 library pack G: state/data depth, more Go/Rust/JVM, embeddings/vector libs."""

LIBS_5G = [
    # ---------- Векторные базы (deepen) ----------
    ("lib-usearch", "USearch", "Векторные базы", "https://github.com/unum-cloud/usearch", "openSource", "vector-search,embedded,fast,multi-language", "pip install usearch",
     "Compact, fast approximate nearest-neighbor search library with bindings in many languages.",
     "Компактная быстрая библиотека приближённого поиска ближайших соседей с биндингами на многих языках.",
     "Its memory footprint is smaller than FAISS for comparable recall — worth benchmarking on your own data."),
    ("lib-annoy", "Annoy", "Векторные базы", "https://github.com/spotify/annoy", "openSource", "vector-search,spotify,memory-mapped,static", "pip install annoy",
     "Spotify's approximate nearest-neighbor library, optimized for memory-mapped read-only indexes.",
     "Библиотека приближённого поиска соседей от Spotify, оптимизирована под memory-mapped индексы только для чтения.",
     "Build the index once and memory-map it across processes — this is where Annoy specifically shines over FAISS."),
    ("lib-scann", "ScaNN", "Векторные базы", "https://github.com/google-research/google-research/tree/master/scann", "openSource", "vector-search,google,quantization,research", "pip install scann",
     "Google Research's vector search library, strong at large-scale quantized search.",
     "Библиотека векторного поиска Google Research, сильна в квантованном поиске на большом масштабе.",
     "Consider it specifically at the scale where FAISS's memory footprint becomes the bottleneck."),

    # ---------- Планирование и reasoning (deepen) ----------
    ("lib-guidance-ai", "Guidance", "Планирование и reasoning", "https://github.com/guidance-ai/guidance", "openSource", "constrained-generation,templates,structured", "pip install guidance",
     "Constrains and templates LLM generation at the token level for structured, controllable output.",
     "Ограничивает и шаблонизирует генерацию LLM на уровне токенов для структурированного вывода.",
     "It interleaves generation with fixed template text, which removes retry loops for format-following entirely."),
    ("lib-lmql", "LMQL", "Планирование и reasoning", "https://lmql.ai/docs/", "openSource", "constrained-generation,query-language,structured", "pip install lmql",
     "Query language for LLMs combining prompting, constraints and control flow in one syntax.",
     "Язык запросов к LLM, объединяющий промптинг, ограничения и управление потоком в одном синтаксисе.",
     "Constraints are declared inline in the query itself, so validity is enforced during generation, not after."),

    # ---------- Go экосистема (deepen) ----------
    ("lib-fiber-go", "Fiber", "Go экосистема", "https://docs.gofiber.io/", "openSource", "go,web,fasthttp,express-like", "go get github.com/gofiber/fiber/v2",
     "Express-inspired Go web framework built on the fasthttp engine.",
     "Go веб-фреймворк в духе Express, построен на движке fasthttp.",
     "fasthttp is not net/http-compatible — check middleware compatibility before switching an existing project."),
    ("lib-chi-go", "chi", "Go экосистема", "https://go-chi.io/", "openSource", "go,router,stdlib,lightweight", "go get github.com/go-chi/chi/v5",
     "Lightweight, idiomatic router built entirely on Go's standard net/http.",
     "Лёгкий идиоматичный роутер, построен целиком на стандартном net/http Go.",
     "Full net/http compatibility means every existing standard-library middleware works here unmodified."),
    ("lib-air-go", "Air", "Go экосистема", "https://github.com/air-verse/air", "openSource", "go,live-reload,development,cli", "go install github.com/air-verse/air@latest",
     "Live reload for Go apps during development, rebuilding and restarting on file changes.",
     "Live reload для Go-приложений в разработке, пересборка и перезапуск при изменении файлов.",
     "The dev-loop speedup here is exactly why compiled languages can still feel as fast to iterate on as scripting ones."),

    # ---------- Rust экосистема (deepen) ----------
    ("lib-rocket-rs", "Rocket", "Rust экосистема", "https://rocket.rs/guide/", "openSource", "rust,web,type-safe,ergonomic", "cargo add rocket",
     "Ergonomic Rust web framework emphasizing type safety and minimal boilerplate.",
     "Эргономичный веб-фреймворк Rust с акцентом на типобезопасность и минимум шаблонного кода.",
     "Request guards catch invalid requests at the type level before your handler code even runs."),
    ("lib-leptos", "Leptos", "Rust экосистема", "https://leptos.dev/", "openSource", "rust,wasm,reactive,fullstack", "cargo install cargo-leptos",
     "Full-stack Rust web framework compiling to WebAssembly, with fine-grained reactivity.",
     "Full-stack веб-фреймворк Rust с компиляцией в WebAssembly и тонкозернистой реактивностью.",
     "Isomorphic server functions let one function definition run seamlessly on both server and client."),

    # ---------- JVM экосистема (deepen) ----------
    ("lib-vertx", "Eclipse Vert.x", "JVM экосистема", "https://vertx.io/docs/", "openSource", "java,reactive,event-driven,polyglot", "",
     "Event-driven, non-blocking toolkit for the JVM, usable from Java, Kotlin and Scala.",
     "Событийный неблокирующий тулкит для JVM, доступен из Java, Kotlin и Scala.",
     "The event bus lets independently deployed verticles communicate without direct coupling."),
    ("lib-jooq", "jOOQ", "JVM экосистема", "https://www.jooq.org/doc/latest/manual/", "mixed", "java,sql,type-safe,code-generation", "",
     "Type-safe SQL query building for Java, generated directly from your database schema.",
     "Типобезопасное построение SQL-запросов для Java, генерируется прямо из схемы базы данных.",
     "Schema changes surface as compile errors in queries that reference the changed column — no silent runtime breaks."),

    # ---------- Состояние и данные (deepen further) ----------
    ("lib-tanstack-store", "TanStack Store", "Состояние и данные", "https://tanstack.com/store/latest", "openSource", "state,framework-agnostic,tanstack,reactive", "npm i @tanstack/store",
     "Framework-agnostic reactive state primitive from the TanStack team.",
     "Фреймворк-независимый реактивный примитив состояния от команды TanStack.",
     "Useful as the shared state layer under a TanStack Query + Router stack in non-React frameworks too."),
    ("lib-effect-ts", "Effect", "Состояние и данные", "https://effect.website/docs/introduction", "openSource", "typescript,functional,error-handling,effects", "npm i effect",
     "Functional effect system for TypeScript: typed errors, dependency injection, structured concurrency.",
     "Функциональная система эффектов для TypeScript: типизированные ошибки, DI, структурированная конкурентность.",
     "Typed errors mean the compiler forces you to handle every failure mode a function can actually produce."),
]
