import 'dart:convert';

import 'package:http/http.dart' as http;

import '../models/catalog_item.dart';

class LiveSyncResult {
  const LiveSyncResult({
    required this.items,
    required this.addedCount,
    required this.versions,
    required this.linkHealth,
    required this.stars,
    required this.events,
    required this.errors,
  });

  final List<CatalogItem> items;
  final int addedCount;
  final Map<String, String> versions;
  final Map<String, bool> linkHealth;
  final Map<String, int> stars;
  final List<UpdateEvent> events;
  final List<String> errors;
}

class LiveUpdateService {
  LiveUpdateService({http.Client? client}) : _client = client ?? http.Client();

  final http.Client _client;

  static const skillRepositories = [
    'openai/plugins',
    'anthropics/skills',
    'anthropics/claude-plugins-official',
    'vercel-labs/agent-skills',
    'expo/skills',
    'huggingface/skills',
    'remotion-dev/skills',
  ];

  static const packageRegistry = <String, ({String registry, String package})>{
    'lib-langchain': (registry: 'pypi', package: 'langchain'),
    'lib-langgraph': (registry: 'pypi', package: 'langgraph'),
    'lib-llamaindex': (registry: 'pypi', package: 'llama-index'),
    'lib-haystack': (registry: 'pypi', package: 'haystack-ai'),
    'lib-pydantic-ai': (registry: 'pypi', package: 'pydantic-ai'),
    'lib-autogen': (registry: 'pypi', package: 'autogen-agentchat'),
    'lib-crewai': (registry: 'pypi', package: 'crewai'),
    'lib-smolagents': (registry: 'pypi', package: 'smolagents'),
    'lib-openai-agents': (registry: 'pypi', package: 'openai-agents'),
    'lib-litellm': (registry: 'pypi', package: 'litellm'),
    'lib-instructor': (registry: 'pypi', package: 'instructor'),
    'lib-dspy': (registry: 'pypi', package: 'dspy'),
    'lib-transformers': (registry: 'pypi', package: 'transformers'),
    'lib-sentence-transformers': (registry: 'pypi', package: 'sentence-transformers'),
    'lib-fastembed': (registry: 'pypi', package: 'fastembed'),
    'lib-chroma': (registry: 'pypi', package: 'chromadb'),
    'lib-fastapi': (registry: 'pypi', package: 'fastapi'),
    'lib-django': (registry: 'pypi', package: 'Django'),
    'lib-flask': (registry: 'pypi', package: 'Flask'),
    'lib-sqlalchemy': (registry: 'pypi', package: 'SQLAlchemy'),
    'lib-pytest': (registry: 'pypi', package: 'pytest'),
    'lib-ruff': (registry: 'pypi', package: 'ruff'),
    'lib-vercel-ai-sdk': (registry: 'npm', package: 'ai'),
    'lib-react': (registry: 'npm', package: 'react'),
    'lib-nextjs': (registry: 'npm', package: 'next'),
    'lib-vue': (registry: 'npm', package: 'vue'),
    'lib-nuxt': (registry: 'npm', package: 'nuxt'),
    'lib-svelte': (registry: 'npm', package: 'svelte'),
    'lib-astro': (registry: 'npm', package: 'astro'),
    'lib-vite': (registry: 'npm', package: 'vite'),
    'lib-tailwind': (registry: 'npm', package: 'tailwindcss'),
    'lib-threejs': (registry: 'npm', package: 'three'),
    'lib-echarts': (registry: 'npm', package: 'echarts'),
    'lib-chartjs': (registry: 'npm', package: 'chart.js'),
    'lib-express': (registry: 'npm', package: 'express'),
    'lib-nestjs': (registry: 'npm', package: '@nestjs/core'),
    'lib-hono': (registry: 'npm', package: 'hono'),
    'lib-trpc': (registry: 'npm', package: '@trpc/server'),
    'lib-prisma': (registry: 'npm', package: 'prisma'),
    'lib-drizzle': (registry: 'npm', package: 'drizzle-orm'),
    'lib-playwright': (registry: 'npm', package: 'playwright'),
    'lib-cypress': (registry: 'npm', package: 'cypress'),
    'lib-vitest': (registry: 'npm', package: 'vitest'),
    'lib-jest': (registry: 'npm', package: 'jest'),
    'lib-biome': (registry: 'npm', package: '@biomejs/biome'),
    'lib-eslint': (registry: 'npm', package: 'eslint'),
    'lib-prettier': (registry: 'npm', package: 'prettier'),
    'lib-storybook': (registry: 'npm', package: 'storybook'),
    'lib-msw': (registry: 'npm', package: 'msw'),
  };

  Future<LiveSyncResult> sync({
    required List<CatalogItem> existing,
    required Map<String, String> currentVersions,
    Map<String, int> currentStars = const {},
    required DateTime? lastSync,
    bool syncMcp = true,
    bool syncSkills = true,
    bool syncVersions = true,
    bool syncLinks = true,
    bool syncStars = true,
  }) async {
    final items = <CatalogItem>[];
    final versions = <String, String>{};
    final health = <String, bool>{};
    final stars = <String, int>{};
    final events = <UpdateEvent>[];
    final errors = <String>[];
    final existingIds = existing.map((item) => item.id).toSet();
    // Live cards are allowed through again so an upstream change can replace
    // the locally cached card with the same stable id.
    final knownNames = existing.where((item) => !item.isLive).map((item) => item.name.toLowerCase()).toSet();

    if (syncMcp) {
      try {
        items.addAll(await _fetchMcpRegistry(lastSync: lastSync, knownNames: knownNames));
      } catch (error) {
        errors.add('MCP Registry: ${error.runtimeType}');
      }
    }

    if (syncSkills) {
      try {
        items.addAll(await _fetchSkillRepositories(knownNames: knownNames));
      } catch (error) {
        errors.add('Skill repositories: ${error.runtimeType}');
      }
    }

    if (syncVersions) {
      try {
        versions.addAll(await _fetchPackageVersions());
        for (final entry in versions.entries) {
          final before = currentVersions[entry.key];
          if (before != null && before != entry.value) {
            events.add(_event('version', '${_nameFor(existing, entry.key)}: $before → ${entry.value}'));
          }
        }
      } catch (error) {
        errors.add('Package registries: ${error.runtimeType}');
      }
    }

    if (syncLinks) {
      try {
        health.addAll(await _checkApiLinks(existing));
      } catch (error) {
        errors.add('API links: ${error.runtimeType}');
      }
    }

    if (syncStars) {
      try {
        stars.addAll(await _fetchGithubStars(existing, currentStars));
      } catch (error) {
        errors.add('GitHub stars: ${error.runtimeType}');
      }
    }

    final addedCount = items.where((item) => !existingIds.contains(item.id)).length;
    for (final item in items.where((item) => !existingIds.contains(item.id))) {
      events.add(_event('added', '${item.kind.name.toUpperCase()}: ${item.name}'));
    }
    return LiveSyncResult(
      items: items,
      addedCount: addedCount,
      versions: versions,
      linkHealth: health,
      stars: stars,
      events: events,
      errors: errors,
    );
  }

  Future<List<CatalogItem>> _fetchMcpRegistry({
    required DateTime? lastSync,
    required Set<String> knownNames,
  }) async {
    final found = <CatalogItem>[];
    String? cursor;
    // A first install walks the whole practical registry window. Later runs use
    // updated_since and normally finish after a single page.
    final maxPages = lastSync == null ? 50 : 10;
    for (var page = 0; page < maxPages; page++) {
      final parameters = <String, String>{'version': 'latest', 'limit': '100'};
      if (lastSync != null) parameters['updated_since'] = lastSync.toUtc().toIso8601String();
      if (cursor != null) parameters['cursor'] = cursor;
      final uri = Uri.https('registry.modelcontextprotocol.io', '/v0.1/servers', parameters);
      final response = await _client.get(uri, headers: const {'Accept': 'application/json'}).timeout(const Duration(seconds: 15));
      if (response.statusCode != 200) throw StateError('HTTP ${response.statusCode}');
      final decoded = jsonDecode(utf8.decode(response.bodyBytes)) as Map<String, dynamic>;
      final servers = decoded['servers'] as List? ?? const [];
      for (final raw in servers.whereType<Map>()) {
        final wrapper = Map<String, dynamic>.from(raw);
        final server = Map<String, dynamic>.from((wrapper['server'] as Map?) ?? wrapper);
        final name = server['name']?.toString().trim() ?? '';
        if (name.isEmpty || knownNames.contains(name.toLowerCase())) continue;
        final meta = wrapper['_meta'] as Map?;
        final official = meta?['io.modelcontextprotocol.registry/official'] as Map?;
        if (official?['status'] == 'deleted') continue;
        final description = server['description']?.toString().trim();
        final version = server['version']?.toString() ?? '';
        final url = _serverUrl(server, name, version);
        found.add(CatalogItem(
          id: 'live-mcp-${_slug(name)}',
          name: name,
          kind: ItemKind.mcp,
          category: 'Документация и контекст',
          summary: description?.isNotEmpty == true ? description! : 'MCP server published in the official MCP Registry.',
          url: url,
          access: AccessType.openSource,
          compatibility: const ['both'],
          tags: const ['mcp', 'registry', 'live', 'discovered'],
          tip: '',
          verifiedAt: DateTime.now().toIso8601String().split('T').first,
          source: 'Official MCP Registry',
          sourceLanguage: 'en',
          localizedSummary: description?.isNotEmpty == true ? {'en': description!} : const {},
          isLive: true,
        ));
        knownNames.add(name.toLowerCase());
      }
      final metadata = decoded['metadata'] as Map?;
      cursor = metadata?['nextCursor']?.toString();
      if (cursor == null || cursor.isEmpty) break;
    }
    return found;
  }

  String _serverUrl(Map<String, dynamic> server, String name, String version) {
    final repository = server['repository'];
    if (repository is Map && repository['url']?.toString().startsWith('https://') == true) {
      return repository['url'].toString();
    }
    final website = server['websiteUrl']?.toString();
    if (website?.startsWith('https://') == true) return website!;
    return 'https://registry.modelcontextprotocol.io/v0.1/servers/${Uri.encodeComponent(name)}/versions/${Uri.encodeComponent(version.isEmpty ? 'latest' : version)}';
  }

  Future<List<CatalogItem>> _fetchSkillRepositories({required Set<String> knownNames}) async {
    final found = <CatalogItem>[];
    for (final repository in skillRepositories) {
      final uri = Uri.parse('https://api.github.com/repos/$repository/git/trees/main?recursive=1');
      final response = await _client.get(uri, headers: const {
        'Accept': 'application/vnd.github+json',
        'X-GitHub-Api-Version': '2026-03-10',
      }).timeout(const Duration(seconds: 15));
      if (response.statusCode != 200) continue;
      final decoded = jsonDecode(utf8.decode(response.bodyBytes)) as Map<String, dynamic>;
      final tree = decoded['tree'] as List? ?? const [];
      var accepted = 0;
      for (final raw in tree.whereType<Map>()) {
        if (accepted >= 350) break;
        final path = raw['path']?.toString() ?? '';
        if (!(path == 'SKILL.md' || path.endsWith('/SKILL.md'))) continue;
        if (path.contains('/node_modules/') || path.contains('/test/fixtures/')) continue;
        final segments = path.split('/');
        final folder = segments.length > 1 ? segments[segments.length - 2] : repository.split('/').last;
        final name = _title(folder);
        if (knownNames.contains(name.toLowerCase())) continue;
        final htmlUrl = 'https://github.com/$repository/blob/main/$path';
        found.add(CatalogItem(
          id: 'live-skill-${_slug('$repository-$path')}',
          name: name,
          kind: ItemKind.skill,
          category: 'Каталоги skills',
          summary: 'Agent Skill discovered in $repository at $path.',
          url: htmlUrl,
          access: AccessType.openSource,
          compatibility: const ['both'],
          tags: ['skill', 'live', 'github', ...repository.split('/')],
          tip: '',
          verifiedAt: DateTime.now().toIso8601String().split('T').first,
          source: repository,
          sourceLanguage: 'en',
          localizedSummary: {'en': 'Agent Skill discovered in $repository at $path.'},
          isLive: true,
        ));
        knownNames.add(name.toLowerCase());
        accepted++;
      }
    }
    return found;
  }

  Future<Map<String, String>> _fetchPackageVersions() async {
    final result = <String, String>{};
    final entries = packageRegistry.entries.toList();
    for (var start = 0; start < entries.length; start += 6) {
      final batch = entries.skip(start).take(6);
      await Future.wait(batch.map((entry) async {
        try {
          final spec = entry.value;
          final uri = spec.registry == 'npm'
              ? Uri.parse('https://registry.npmjs.org/${Uri.encodeComponent(spec.package)}/latest')
              : Uri.parse('https://pypi.org/pypi/${Uri.encodeComponent(spec.package)}/json');
          final response = await _client.get(uri, headers: const {'Accept': 'application/json'}).timeout(const Duration(seconds: 8));
          if (response.statusCode != 200) return;
          final decoded = jsonDecode(utf8.decode(response.bodyBytes)) as Map<String, dynamic>;
          final version = spec.registry == 'npm' ? decoded['version']?.toString() : (decoded['info'] as Map?)?['version']?.toString();
          if (version != null && version.isNotEmpty) result[entry.key] = version;
        } catch (_) {
          // A single registry failure must not block the remaining sources.
        }
      }));
    }
    return result;
  }

  Future<Map<String, bool>> _checkApiLinks(List<CatalogItem> existing) async {
    final apis = existing.where((item) => item.kind == ItemKind.api && !item.isCustom).toList();
    if (apis.isEmpty) return {};
    final offset = DateTime.now().day % apis.length;
    final selection = <CatalogItem>[];
    for (var index = 0; index < 12 && index < apis.length; index++) {
      selection.add(apis[(offset + index) % apis.length]);
    }
    final result = <String, bool>{};
    for (final item in selection) {
      try {
        var response = await _client.head(Uri.parse(item.url)).timeout(const Duration(seconds: 6));
        if (response.statusCode == 405) {
          response = await _client.get(Uri.parse(item.url), headers: const {'Range': 'bytes=0-0'}).timeout(const Duration(seconds: 6));
        }
        result[item.id] = response.statusCode >= 200 && response.statusCode < 500 && response.statusCode != 404;
      } catch (_) {
        result[item.id] = false;
      }
    }
    return result;
  }

  /// How many repositories one daily run refreshes.
  ///
  /// Unauthenticated GitHub allows 60 requests an hour, so a full catalogue
  /// sweep in one run is impossible. A daily slice keeps every entry refreshed
  /// on a rolling basis instead, the same approach `_checkApiLinks` uses.
  static const starsPerRun = 40;

  /// Star counts for entries that point at a GitHub repository.
  ///
  /// Entries whose star count is already known are refreshed last, so a first
  /// run fills in unranked entries rather than re-fetching what it already has.
  Future<Map<String, int>> _fetchGithubStars(
    List<CatalogItem> existing,
    Map<String, int> currentStars,
  ) async {
    final candidates = <(String, String)>[];
    for (final item in existing) {
      if (item.isCustom) continue;
      final repository = githubRepositoryOf(item.url);
      if (repository != null) candidates.add((item.id, repository));
    }
    if (candidates.isEmpty) return {};

    final unranked = candidates.where((entry) => !currentStars.containsKey(entry.$1)).toList();
    final ranked = candidates.where((entry) => currentStars.containsKey(entry.$1)).toList();
    // Rotate the already-ranked ones by day so refreshes spread over the month.
    if (ranked.isNotEmpty) {
      final offset = DateTime.now().day % ranked.length;
      ranked
        ..addAll(ranked.sublist(0, offset))
        ..removeRange(0, offset);
    }
    final selection = [...unranked, ...ranked].take(starsPerRun);

    final result = <String, int>{};
    for (final (id, repository) in selection) {
      try {
        final uri = Uri.parse('https://api.github.com/repos/$repository');
        final response = await _client.get(uri, headers: const {
          'Accept': 'application/vnd.github+json',
          'X-GitHub-Api-Version': '2026-03-10',
        }).timeout(const Duration(seconds: 8));
        // 403 means the hourly limit is spent; stop rather than burn the rest.
        if (response.statusCode == 403 || response.statusCode == 429) break;
        if (response.statusCode != 200) continue;
        final decoded = jsonDecode(utf8.decode(response.bodyBytes)) as Map<String, dynamic>;
        final count = decoded['stargazers_count'];
        if (count is int) result[id] = count;
      } catch (_) {
        // One unreachable repository must not end the sweep.
      }
    }
    return result;
  }

  /// `owner/repo` for a GitHub URL, or null when the URL points elsewhere.
  ///
  /// Deliberately rejects the reserved paths that look like repositories but
  /// are not, so `github.com/features/actions` is never treated as one.
  static String? githubRepositoryOf(String url) {
    final uri = Uri.tryParse(url);
    if (uri == null || uri.host != 'github.com') return null;
    final segments = uri.pathSegments.where((part) => part.isNotEmpty).toList();
    if (segments.length < 2) return null;
    const reserved = {
      'features', 'about', 'pricing', 'marketplace', 'sponsors', 'topics',
      'collections', 'trending', 'explore', 'settings', 'orgs', 'apps', 'login',
    };
    if (reserved.contains(segments[0])) return null;
    final name = segments[1].endsWith('.git')
        ? segments[1].substring(0, segments[1].length - 4)
        : segments[1];
    if (name.isEmpty) return null;
    return '${segments[0]}/$name';
  }

  UpdateEvent _event(String kind, String message) {
    final now = DateTime.now();
    return UpdateEvent(id: '${now.microsecondsSinceEpoch}-$kind', kind: kind, message: message, createdAt: now);
  }

  String _nameFor(List<CatalogItem> existing, String id) {
    for (final item in existing) {
      if (item.id == id) return item.name;
    }
    return id;
  }

  String _slug(String value) => value.toLowerCase().replaceAll(RegExp(r'[^a-z0-9]+'), '-').replaceAll(RegExp(r'^-|-$'), '');

  String _title(String value) => value
      .split(RegExp(r'[-_]'))
      .where((part) => part.isNotEmpty)
      .map((part) => '${part[0].toUpperCase()}${part.substring(1)}')
      .join(' ');
}
