import 'package:flutter/material.dart';

import '../app_state.dart';
import '../platform/platform_info.dart';
import '../theme.dart';
import '../widgets/catalog_card.dart';
import '../widgets/item_detail_sheet.dart';
import '../widgets/scroll_to_top.dart';

/// Saved entries. Stateful only because the grid owns a [ScrollController] for
/// [ScrollTopHook]; a long favorites list scrolls just as far as the catalog.
class FavoritesScreen extends StatefulWidget {
  const FavoritesScreen({super.key});

  @override
  State<FavoritesScreen> createState() => _FavoritesScreenState();
}

class _FavoritesScreenState extends State<FavoritesScreen> {
  final _scroll = ScrollController();

  @override
  void dispose() {
    _scroll.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final items = state.favoriteItems;
    final s = state.s;
    return SafeArea(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          Text(s.t('favoriteTitle'), style: Theme.of(context).textTheme.displaySmall),
          const SizedBox(height: 8),
          Text(s.t('favoriteSubtitle', {'count': items.length}),
              style: const TextStyle(color: AtlasTheme.textSecondary)),
          const SizedBox(height: 22),
          Expanded(
            child: items.isEmpty
                ? Center(child: Text(s.t('favoriteEmpty'), style: const TextStyle(color: AtlasTheme.textMuted)))
                : LayoutBuilder(builder: (context, constraints) {
                    final columns = Breakpoints.gridColumns(constraints.maxWidth);
                    final showPreview = constraints.maxWidth / columns >= 240 && state.previewsEnabled;
                    return ScrollTopHook(
                      controller: _scroll,
                      topLabel: s.t('scrollTop'),
                      backLabel: s.t('scrollBack'),
                      child: GridView.builder(
                        controller: _scroll,
                        itemCount: items.length,
                        gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                          crossAxisCount: columns,
                          crossAxisSpacing: 14,
                          mainAxisSpacing: 14,
                          mainAxisExtent: showPreview ? 292 : 172,
                        ),
                        itemBuilder: (context, index) => CatalogCard(
                          item: items[index],
                          showPreview: showPreview,
                          onTap: () => showItemDetailSheet(context, items[index]),
                        ),
                      ),
                    );
                  }),
          ),
        ]),
      ),
    );
  }
}
