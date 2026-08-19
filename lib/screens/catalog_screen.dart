import 'package:flutter/material.dart';

import '../app_state.dart';
import '../main.dart' show searchFocusNode;
import '../models/catalog_item.dart';
import '../platform/platform_info.dart';
import '../theme.dart';
import '../widgets/catalog_card.dart';
import '../widgets/item_detail_sheet.dart';
import '../widgets/scroll_to_top.dart';

/// Catalog browser.
///
/// Below [Breakpoints.expanded] this is a grid plus a modal detail sheet.
/// Above it the screen switches to a master–detail layout: the grid keeps its
/// scroll position while the selected entry lives in a pane on the right. That
/// is the desktop shape the rework is heading towards.
class CatalogScreen extends StatelessWidget {
  const CatalogScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);

    return LayoutBuilder(
      builder: (context, constraints) {
        final width = constraints.maxWidth;
        final masterDetail = Breakpoints.isMasterDetail(width);
        final selected = state.selectedItem;

        final list = _CatalogList(
          width: masterDetail ? width * 0.58 : width,
          masterDetail: masterDetail,
        );

        if (!masterDetail) return Scaffold(body: SafeArea(child: list));

        return Scaffold(
          body: SafeArea(
            child: Row(
              children: [
                Expanded(flex: 58, child: list),
                const VerticalDivider(width: 1),
                Expanded(
                  flex: 42,
                  child: selected == null
                      ? const _EmptyPane()
                      : ItemDetailView(
                          key: ValueKey(selected.id),
                          item: selected,
                          onClose: () => state.select(null),
                        ),
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}

class _CatalogList extends StatefulWidget {
  const _CatalogList({required this.width, required this.masterDetail});

  final double width;
  final bool masterDetail;

  @override
  State<_CatalogList> createState() => _CatalogListState();
}

class _CatalogListState extends State<_CatalogList> {
  /// Owned here rather than in [CatalogScreen] so the scroll position survives
  /// the master–detail switch, which rebuilds the surrounding layout.
  final _scroll = ScrollController();

  @override
  void dispose() {
    _scroll.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final s = state.s;
    final items = state.visibleItems;
    final columns = widget.masterDetail ? 2 : Breakpoints.gridColumns(widget.width);
    final showPreview = widget.width / columns >= 240 && state.previewsEnabled;

    return ScrollTopHook(
      controller: _scroll,
      topLabel: s.t('scrollTop'),
      backLabel: s.t('scrollBack'),
      child: CustomScrollView(
        controller: _scroll,
        slivers: [
          SliverPadding(
            padding: const EdgeInsets.fromLTRB(16, 14, 16, 8),
            sliver: SliverToBoxAdapter(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(s.t('catalogKicker').toUpperCase(),
                      style: const TextStyle(
                          fontSize: 10.5,
                          letterSpacing: 1.6,
                          fontWeight: FontWeight.w800,
                          color: AtlasTheme.info)),
                  const SizedBox(height: 8),
                  Text(s.t('catalogTitle'),
                      style: const TextStyle(
                          fontSize: 25, fontWeight: FontWeight.w800, height: 1.1, letterSpacing: -0.5)),
                  const SizedBox(height: 7),
                  Text(s.t('catalogSubtitle', {'count': state.allItems.length}),
                      style: const TextStyle(fontSize: 13, height: 1.45, color: AtlasTheme.textMuted)),
                  const SizedBox(height: 16),
                  const _SearchField(),
                  const SizedBox(height: 12),
                  const _GroupBar(),
                  const SizedBox(height: 10),
                  const _FilterBar(),
                  const SizedBox(height: 14),
                  Row(
                    children: [
                      Text(s.t('found', {'count': items.length}),
                          style: const TextStyle(fontSize: 12.5, color: AtlasTheme.textMuted)),
                      const Spacer(),
                      const _SortMenu(),
                    ],
                  ),
                ],
              ),
            ),
          ),
          if (items.isEmpty)
            SliverFillRemaining(
              hasScrollBody: false,
              child: _Empty(title: s.t('emptyTitle'), hint: s.t('emptyHint')),
            )
          else
            SliverPadding(
              padding: const EdgeInsets.fromLTRB(16, 6, 16, 88),
              sliver: SliverGrid(
                gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                  crossAxisCount: columns,
                  crossAxisSpacing: 12,
                  mainAxisSpacing: 12,
                  mainAxisExtent: showPreview ? 292 : 172,
                ),
                delegate: SliverChildBuilderDelegate(
                  (context, index) {
                    final item = items[index];
                    return CatalogCard(
                      item: item,
                      showPreview: showPreview,
                      selected: widget.masterDetail && state.selectedItemId == item.id,
                      onTap: () {
                        if (widget.masterDetail) {
                          state.select(item.id);
                        } else {
                          showItemDetailSheet(context, item);
                        }
                      },
                    );
                  },
                  childCount: items.length,
                ),
              ),
            ),
        ],
      ),
    );
  }
}

class _SearchField extends StatefulWidget {
  const _SearchField();

  @override
  State<_SearchField> createState() => _SearchFieldState();
}

class _SearchFieldState extends State<_SearchField> {
  final _controller = TextEditingController();

  /// The controller starts empty on purpose: reading [AppScope] from initState
  /// trips an assertion in debug builds, and build() below already copies the
  /// current query in whenever the two differ, including on the first frame.

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    if (_controller.text != state.query) {
      _controller.value = TextEditingValue(
        text: state.query,
        selection: TextSelection.collapsed(offset: state.query.length),
      );
    }
    return TextField(
      controller: _controller,
      focusNode: searchFocusNode,
      onChanged: state.setQuery,
      textInputAction: TextInputAction.search,
      decoration: InputDecoration(
        hintText: state.s.t('searchHint'),
        prefixIcon: const Icon(Icons.search_rounded, size: 20),
        suffixIcon: state.query.isEmpty
            ? null
            : IconButton(
                icon: const Icon(Icons.close_rounded, size: 18),
                onPressed: () {
                  _controller.clear();
                  state.setQuery('');
                },
              ),
      ),
    );
  }
}

/// Top level sections (AI, frontend, backend…) generated from the catalog.
class _GroupBar extends StatelessWidget {
  const _GroupBar();

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final s = state.s;
    final counts = state.countByGroup;
    final entries = counts.entries.toList()
      ..sort((a, b) => b.value.compareTo(a.value));

    return SizedBox(
      height: 36,
      child: ListView(
        scrollDirection: Axis.horizontal,
        children: [
          _Pill(
            label: s.t('allSections'),
            selected: state.groupFilter == 'all',
            onTap: () => state.setGroup('all'),
          ),
          for (final entry in entries)
            _Pill(
              label: '${s.group(entry.key)} · ${entry.value}',
              selected: state.groupFilter == entry.key,
              onTap: () => state.setGroup(entry.key),
            ),
        ],
      ),
    );
  }
}

class _FilterBar extends StatelessWidget {
  const _FilterBar();

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final s = state.s;

    return Wrap(
      spacing: 8,
      runSpacing: 8,
      children: [
        for (final kind in ItemKind.values)
          FilterChip(
            label: Text(s.kind(kind)),
            selected: state.kindFilter == kind,
            onSelected: (value) => state.setKind(value ? kind : null),
          ),
        FilterChip(
          label: Text(s.t('withExamples')),
          selected: state.onlyExamples,
          avatar: const Icon(Icons.photo_library_outlined, size: 16),
          onSelected: state.toggleOnlyExamples,
        ),
        FilterChip(
          label: Text(s.t('freeOnly')),
          selected: state.priceFilter == 'free',
          onSelected: (value) => state.setPrice(value ? 'free' : 'any'),
        ),
        FilterChip(
          label: Text(s.t('favoritesOnly')),
          selected: state.onlyFavorites,
          onSelected: state.toggleOnlyFavorites,
        ),
        FilterChip(
          label: Text(s.t('liveOnly')),
          selected: state.onlyLive,
          onSelected: state.toggleOnlyLive,
        ),
        if (state.activeFilterCount > 0)
          ActionChip(
            avatar: const Icon(Icons.restart_alt_rounded, size: 16),
            label: Text('${s.t('resetFilters')} (${state.activeFilterCount})'),
            onPressed: state.clearFilters,
          ),
      ],
    );
  }
}

class _SortMenu extends StatelessWidget {
  const _SortMenu();

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final s = state.s;
    const modes = ['relevance', 'name', 'new', 'free', 'popular'];
    final labels = {
      'relevance': s.t('sortRelevance'),
      'name': s.t('sortName'),
      'new': s.t('sortNew'),
      'free': s.t('sortFreeFirst'),
      'popular': s.t('sortPopular'),
    };

    return PopupMenuButton<String>(
      initialValue: state.sortMode,
      tooltip: s.t('sortBy'),
      onSelected: state.setSort,
      itemBuilder: (context) => [
        for (final mode in modes)
          PopupMenuItem(value: mode, child: Text(labels[mode] ?? mode)),
      ],
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(Icons.sort_rounded, size: 16, color: AtlasTheme.textMuted),
          const SizedBox(width: 6),
          Text(labels[state.sortMode] ?? '',
              style: const TextStyle(fontSize: 12.5, color: AtlasTheme.textMuted)),
        ],
      ),
    );
  }
}

class _Pill extends StatelessWidget {
  const _Pill({required this.label, required this.selected, required this.onTap});
  final String label;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(right: 8),
      child: InkWell(
        borderRadius: BorderRadius.circular(AtlasTheme.rChip),
        onTap: onTap,
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 13, vertical: 8),
          decoration: BoxDecoration(
            color: selected ? AtlasTheme.accent.withValues(alpha: 0.18) : AtlasTheme.panel,
            borderRadius: BorderRadius.circular(AtlasTheme.rChip),
            border: Border.all(
              color: selected ? AtlasTheme.accent : AtlasTheme.line,
            ),
          ),
          child: Text(
            label,
            style: TextStyle(
              fontSize: 12.5,
              fontWeight: FontWeight.w600,
              color: selected ? AtlasTheme.textPrimary : AtlasTheme.textSecondary,
            ),
          ),
        ),
      ),
    );
  }
}

class _EmptyPane extends StatelessWidget {
  const _EmptyPane();

  @override
  Widget build(BuildContext context) {
    final s = AppScope.of(context).s;
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.dashboard_customize_outlined, size: 40, color: AtlasTheme.textGhost),
            const SizedBox(height: 14),
            Text(s.t('detailPane'),
                textAlign: TextAlign.center,
                style: const TextStyle(fontSize: 13.5, height: 1.5, color: AtlasTheme.textFaint)),
          ],
        ),
      ),
    );
  }
}

class _Empty extends StatelessWidget {
  const _Empty({required this.title, required this.hint});
  final String title;
  final String hint;

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.search_off_rounded, size: 40, color: AtlasTheme.textGhost),
            const SizedBox(height: 14),
            Text(title, style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w700)),
            const SizedBox(height: 6),
            Text(hint,
                textAlign: TextAlign.center,
                style: const TextStyle(fontSize: 13, height: 1.5, color: AtlasTheme.textFaint)),
          ],
        ),
      ),
    );
  }
}
