# -*- coding: utf-8 -*-
"""Stage 5 MCP + skills pack G: official vendor servers, data platforms, workspace tools."""

MCPS_5G = [
    # ---------- Официальные серверы поставщиков ----------
    ("mcp-shopify", "Shopify MCP", "Платежи и commerce", "https://shopify.dev/docs/apps/build/devmcp", "free", "shopify,commerce,official,storefront", "",
     "Shopify's official server exposing store data and development documentation.",
     "Официальный сервер Shopify с данными магазина и документацией для разработки.",
     "Its docs-search tools keep an agent on current Shopify APIs rather than deprecated ones."),

    # ---------- Данные и аналитика ----------
    ("mcp-snowflake", "Snowflake MCP", "Data engineering", "https://github.com/Snowflake-Labs/mcp", "openSource", "snowflake,warehouse,sql,official", "",
     "Snowflake Labs' server for warehouse queries and Cortex AI features.",
     "Сервер Snowflake Labs для запросов к хранилищу и функций Cortex AI.",
     "Set a query timeout and warehouse size limit — an agent can start a very expensive query easily."),
    ("mcp-bigquery", "BigQuery MCP", "Data engineering", "https://github.com/googleapis/genai-toolbox", "openSource", "bigquery,google,sql,toolbox", "",
     "BigQuery access through Google's MCP Toolbox for databases.",
     "Доступ к BigQuery через MCP Toolbox for Databases от Google.",
     "BigQuery bills by bytes scanned, so cap the maximum bytes billed before letting an agent query."),
    ("mcp-databricks", "Databricks MCP", "Data engineering", "https://docs.databricks.com/aws/en/generative-ai/mcp/", "free", "databricks,lakehouse,official,unity-catalog", "",
     "Databricks' managed servers for Unity Catalog data and functions.",
     "Управляемые серверы Databricks для данных и функций Unity Catalog.",
     "Unity Catalog permissions still apply, so the agent inherits exactly the access its identity has."),

    # ---------- Веб-данные и поиск ----------
    ("mcp-puppeteer", "Puppeteer MCP", "Браузер и автоматизация", "https://github.com/modelcontextprotocol/servers-archived/tree/main/src/puppeteer", "openSource", "puppeteer,browser,screenshots,reference", "npx -y @modelcontextprotocol/server-puppeteer",
     "Reference server for browser automation and screenshots via Puppeteer.",
     "Эталонный сервер автоматизации браузера и снимков экрана через Puppeteer.",
     "Now archived — the Playwright server is the maintained option for the same job."),

    # ---------- Инфраструктура и DevOps ----------
    ("mcp-gitlab", "GitLab MCP", "Git и совместная работа", "https://docs.gitlab.com/user/gitlab_duo/model_context_protocol/mcp_server/", "free", "gitlab,official,merge-requests,pipelines", "",
     "GitLab's official server for issues, merge requests and pipelines.",
     "Официальный сервер GitLab для задач, merge request и пайплайнов.",
     "Token scopes decide what the agent can do — a read_api token covers most review workflows."),

    # ---------- Рабочее пространство и CRM ----------
    ("mcp-monday", "Monday.com MCP", "Планирование и календари", "https://developer.monday.com/apps/docs/mondaycom-mcp-integration", "free", "monday,boards,official,work-management", "",
     "Monday.com's official server for boards, items and updates.",
     "Официальный сервер Monday.com для досок, элементов и обновлений.",
     "Column types are strongly typed — have the agent read the board schema before writing values."),
    ("mcp-airtable", "Airtable MCP", "Таблицы и no-code", "https://github.com/domdomegg/airtable-mcp-server", "openSource", "airtable,records,bases,community", "npx -y airtable-mcp-server",
     "Community server for reading and writing Airtable bases and records.",
     "Сервер сообщества для чтения и записи баз и записей Airtable.",
     "Use a scoped personal access token — Airtable tokens can otherwise reach every base you own."),
    ("mcp-salesforce", "Salesforce MCP", "CRM и продажи", "https://developer.salesforce.com/docs/einstein/genai/guide/mcp.html", "free", "salesforce,crm,official,records", "",
     "Salesforce's official server for CRM records and Apex metadata.",
     "Официальный сервер Salesforce для записей CRM и метаданных Apex.",
     "Field-level security applies to the agent as to any user, so permissions carry over unchanged."),
    ("mcp-hubspot", "HubSpot MCP", "CRM и продажи", "https://developers.hubspot.com/mcp", "free", "hubspot,crm,official,contacts", "npx -y @hubspot/mcp-server",
     "HubSpot's official server for contacts, deals and CRM objects.",
     "Официальный сервер HubSpot для контактов, сделок и объектов CRM.",
     "Its association tools are what make multi-object CRM questions answerable in one pass."),
    ("mcp-intercom", "Intercom MCP", "CRM и продажи", "https://developers.intercom.com/docs/guides/mcp", "free", "intercom,support,official,conversations", "",
     "Intercom's official server for conversations, contacts and help articles.",
     "Официальный сервер Intercom для диалогов, контактов и статей справки.",
     "Useful for summarizing support themes across conversations rather than reading them one by one."),
    ("mcp-twilio", "Twilio MCP", "Коммуникации", "https://github.com/twilio-labs/mcp", "openSource", "twilio,messaging,official,api", "npx -y @twilio-alpha/mcp",
     "Twilio Labs' server exposing the Twilio API surface to agents.",
     "Сервер Twilio Labs, открывающий агентам поверхность API Twilio.",
     "An agent with send permissions can spend real money per message — restrict the API key scope."),
    ("mcp-sendgrid", "SendGrid MCP", "Почта и уведомления", "https://github.com/Garoth/sendgrid-mcp", "openSource", "sendgrid,email,community,campaigns", "",
     "Community server for SendGrid contacts, templates and email sending.",
     "Сервер сообщества для контактов, шаблонов и отправки писем SendGrid.",
     "Keep sending permissions off unless you intend the agent to actually mail your contact list."),
    ("mcp-bitbucket", "Bitbucket MCP", "Git и совместная работа", "https://github.com/MatanYemini/bitbucket-mcp", "openSource", "bitbucket,pull-requests,community,repos", "npx -y bitbucket-mcp",
     "Community server for Bitbucket repositories and pull requests.",
     "Сервер сообщества для репозиториев и pull request Bitbucket.",
     "App passwords are scoped per permission — grant only pull request read for review workflows."),
]

SKILLS_5G = [
    ("skill-mcp-build-server", "Build an MCP Server", "Создание skills", "https://modelcontextprotocol.io/docs/develop/build-server", "free", "mcp,tutorial,server,sdk", "",
     "Official walkthrough for writing your first MCP server.",
     "Официальное руководство по написанию первого MCP-сервера.",
     "Start with stdio transport — remote servers add auth concerns you do not need while learning."),
    ("skill-mcp-security", "MCP Security Best Practices", "AI-оценка и safety", "https://modelcontextprotocol.io/specification/draft/basic/security_best_practices", "free", "mcp,security,tokens,confused-deputy", "",
     "Security guidance for MCP servers, covering token handling and confused-deputy risks.",
     "Руководство по безопасности MCP-серверов: работа с токенами и риски confused deputy.",
     "Read this before exposing any server that holds credentials on behalf of multiple users."),
    ("skill-tool-use-guide", "Tool Use with Claude", "Гайды и практики", "https://docs.claude.com/en/docs/agents-and-tools/tool-use/overview", "free", "tools,function-calling,schemas,claude", "",
     "How tool definitions, invocation and results work with the Claude API.",
     "Как работают определения инструментов, их вызов и результаты в Claude API.",
     "Tool descriptions are prompt text — a vague description is the usual cause of wrong tool selection."),
]
