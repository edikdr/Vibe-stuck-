# -*- coding: utf-8 -*-
"""Stage 5 library pack U: CLI tooling, terminal UI, shells, developer ergonomics."""

LIBS_5U = [
    # ---------- CLI и терминал ----------
    ("lib-clack", "Clack", "Разработка и DevOps", "https://www.clack.cc/", "openSource", "node,prompts,cli,interactive", "npm i @clack/prompts",
     "Polished interactive CLI prompts with a consistent visual style out of the box.",
     "Отточенные интерактивные CLI-промпты с единым визуальным стилем из коробки.",
     "Handles Ctrl+C cancellation properly by default, which hand-rolled prompt loops usually get wrong."),
    ("lib-inquirer", "Inquirer.js", "Разработка и DevOps", "https://github.com/SBoudrias/Inquirer.js", "openSource", "node,prompts,cli,interactive", "npm i @inquirer/prompts",
     "The long-standing interactive prompt collection for Node CLIs.",
     "Давно устоявшийся набор интерактивных промптов для Node-CLI.",
     "The newer @inquirer/prompts packages are far lighter than the legacy monolithic import."),

    # ---------- Оболочки и dev-окружение ----------
    ("lib-fish-shell", "fish shell", "Разработка и DevOps", "https://fishshell.com/docs/current/", "openSource", "shell,autosuggestions,interactive,ux", "",
     "Interactive shell with autosuggestions and syntax highlighting configured by default.",
     "Интерактивная оболочка с автоподсказками и подсветкой синтаксиса из коробки.",
     "Not POSIX-compatible — keep your scripts in bash and use fish only as the interactive shell."),
    ("lib-nushell", "Nushell", "Разработка и DevOps", "https://www.nushell.sh/book/", "openSource", "shell,structured-data,pipelines,tables", "",
     "Shell where pipelines carry structured data instead of plain text.",
     "Оболочка, где по пайплайнам передаются структурированные данные, а не просто текст.",
     "Removes the awk/cut/sed parsing layer entirely — columns are real fields, not character offsets."),
    ("lib-starship", "Starship", "Разработка и DevOps", "https://starship.rs/config/", "openSource", "prompt,shell,cross-shell,rust", "curl -sS https://starship.rs/install.sh | sh",
     "Fast, configurable shell prompt that works identically across bash, zsh, fish and others.",
     "Быстрый настраиваемый промпт оболочки, одинаково работающий в bash, zsh, fish и других.",
     "One TOML config gives an identical prompt on every machine and every shell you use."),
    ("lib-asdf", "asdf", "Разработка и DevOps", "https://asdf-vm.com/guide/getting-started.html", "openSource", "version-manager,plugins,runtimes,tool-versions", "",
     "Plugin-based manager for multiple runtime versions from one .tool-versions file.",
     "Менеджер версий рантаймов на плагинах, работающий из одного файла .tool-versions.",
     "Its plugin coverage is the broadest, though mise is considerably faster for the same job."),

    # ---------- Файлы и поиск в терминале ----------
    ("lib-yq", "yq", "Разработка и DevOps", "https://mikefarah.gitbook.io/yq/", "openSource", "yaml,cli,query,transform", "brew install yq",
     "jq-style processor for YAML, XML and TOML documents.",
     "Обработчик в стиле jq для документов YAML, XML и TOML.",
     "Essential for scripted edits to Kubernetes manifests and CI configs without a YAML parser in code."),
    ("lib-hyperfine", "hyperfine", "Тестирование и качество", "https://github.com/sharkdp/hyperfine", "openSource", "benchmark,cli,statistics,rust", "cargo install hyperfine",
     "Command-line benchmarking tool with warmup runs and statistical output.",
     "Инструмент бенчмаркинга командной строки с прогревочными запусками и статистикой.",
     "Warmup runs matter more than most people expect — first-run I/O caching skews naive time comparisons."),
]
