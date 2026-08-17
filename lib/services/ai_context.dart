import '../models/catalog_item.dart';

/// Builds a ready-to-paste context block for a coding agent (Claude Code,
/// Codex, Cursor …). The goal is that a user can copy one card and the agent
/// immediately knows what the tool is, where its documentation lives, how to
/// install it and what to be careful about — without a web search.
class AiContextBuilder {
  const AiContextBuilder();

  /// Context for a single entry.
  String buildItem(CatalogItem item, {String languageCode = 'en'}) {
    final buffer = StringBuffer()
      ..writeln('## ${item.name} (${_kindLabel(item.kind)})')
      ..writeln();

    final summary = item.localizedSummary[languageCode]?.trim();
    buffer.writeln(summary?.isNotEmpty == true ? summary : item.summary);
    buffer.writeln();

    buffer.writeln('- Type: ${item.kind.name}');
    buffer.writeln('- Category: ${item.category} / ${item.categoryGroup}');
    buffer.writeln('- Docs: ${item.url}');
    if (item.install.isNotEmpty) buffer.writeln('- Install: `${item.install}`');
    buffer.writeln('- Access: ${item.access.name}');
    if (item.pricing.hasPrice) {
      buffer.writeln('- Pricing: ${item.pricing.from} (${item.pricing.model})');
      if (item.pricing.freeQuota.isNotEmpty) {
        buffer.writeln('- Free quota: ${item.pricing.freeQuota}');
      }
      if (item.pricing.url.isNotEmpty) {
        buffer.writeln('- Pricing page: ${item.pricing.url}');
      }
      if (item.pricing.checkedAt.isNotEmpty) {
        buffer.writeln('- Pricing checked: ${item.pricing.checkedAt} (verify before relying on it)');
      }
    }
    if (item.compatibility.isNotEmpty) {
      buffer.writeln('- Works with: ${item.compatibility.contains('both') ? 'Codex + Claude Code' : item.compatibility.join(', ')}');
    }
    if (item.tags.isNotEmpty) buffer.writeln('- Tags: ${item.tags.take(10).join(', ')}');
    if (item.verifiedAt.isNotEmpty) buffer.writeln('- Verified: ${item.verifiedAt}');

    final tip = item.localizedTip[languageCode]?.trim();
    final effectiveTip = tip?.isNotEmpty == true ? tip! : item.tip;
    if (effectiveTip.isNotEmpty) {
      buffer
        ..writeln()
        ..writeln('Practical note: $effectiveTip');
    }

    final config = mcpConfigSnippet(item);
    if (config.isNotEmpty) {
      buffer
        ..writeln()
        ..writeln('MCP client config:')
        ..writeln('```json')
        ..writeln(config)
        ..writeln('```');
    }

    if (item.showcases.isNotEmpty) {
      buffer
        ..writeln()
        ..writeln('Reference implementations:');
      for (final showcase in item.showcases) {
        final note = showcase.noteFor(languageCode);
        buffer.writeln('- ${showcase.title} — ${showcase.url}${note.isEmpty ? '' : ' ($note)'}');
      }
    }

    buffer
      ..writeln()
      ..writeln('When you use this, read the official documentation above first and '
          'pin the version you verified. Do not invent endpoints or option names.');
    return buffer.toString().trimRight();
  }

  /// Context for a whole result set, capped so a paste stays usable.
  String buildList(List<CatalogItem> items, {String languageCode = 'en', int limit = 40}) {
    final selection = items.take(limit).toList(growable: false);
    final buffer = StringBuffer()
      ..writeln('# VibeStack Atlas selection (${selection.length} of ${items.length})')
      ..writeln();
    for (final item in selection) {
      final summary = item.localizedSummary[languageCode]?.trim();
      final line = summary?.isNotEmpty == true ? summary! : item.summary;
      buffer.write('- **${item.name}** (${item.kind.name}, ${item.category}) — $line ${item.url}');
      if (item.install.isNotEmpty) buffer.write(' · `${item.install}`');
      if (item.pricing.hasPrice) buffer.write(' · ${item.pricing.from}');
      buffer.writeln();
    }
    if (items.length > selection.length) {
      buffer
        ..writeln()
        ..writeln('(${items.length - selection.length} more entries were omitted.)');
    }
    return buffer.toString().trimRight();
  }

  /// Turns an `npx …` / `uvx …` install line into an mcpServers config block.
  String mcpConfigSnippet(CatalogItem item) {
    if (item.kind != ItemKind.mcp || item.install.isEmpty) return '';
    final parts = item.install.split(RegExp(r'\s+')).where((part) => part.isNotEmpty).toList();
    if (parts.isEmpty) return '';
    final command = parts.first;
    if (command != 'npx' && command != 'uvx' && command != 'docker') return '';
    final args = parts.skip(1).map((part) => '"$part"').join(', ');
    final key = _slug(item.name);
    return '{\n'
        '  "mcpServers": {\n'
        '    "$key": {\n'
        '      "command": "$command",\n'
        '      "args": [$args]\n'
        '    }\n'
        '  }\n'
        '}';
  }

  String _kindLabel(ItemKind kind) => switch (kind) {
        ItemKind.api => 'API',
        ItemKind.library => 'library',
        ItemKind.mcp => 'MCP server',
        ItemKind.skill => 'agent skill',
      };

  String _slug(String value) => value
      .toLowerCase()
      .replaceAll(RegExp(r'[^a-z0-9]+'), '-')
      .replaceAll(RegExp(r'^-+|-+$'), '');
}
