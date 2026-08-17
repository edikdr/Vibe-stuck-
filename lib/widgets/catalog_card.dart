import 'package:flutter/material.dart';

import '../app_state.dart';
import '../models/catalog_item.dart';
import '../theme.dart';
import 'showcase_gallery.dart';

/// One entry in the catalog grid.
///
/// [showPreview] draws the first example image on top of the card. It is on by
/// default on wide layouts and off on phones, where a dense text list scans
/// faster and saves both traffic and cache space.
class CatalogCard extends StatelessWidget {
  const CatalogCard({
    required this.item,
    required this.onTap,
    this.showPreview = false,
    this.selected = false,
    super.key,
  });

  final CatalogItem item;
  final VoidCallback onTap;
  final bool showPreview;
  final bool selected;

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final s = state.s;
    final favorite = state.isFavorite(item.id);
    final version = state.versionFor(item.id);

    final examples = state.examplesFor(item, s.t('officialPage'));
    final previewUrl = examples.isEmpty ? '' : state.previewUrlFor(examples.first);

    return Material(
      color: AtlasTheme.panel,
      borderRadius: BorderRadius.circular(AtlasTheme.rCard),
      child: InkWell(
        borderRadius: BorderRadius.circular(AtlasTheme.rCard),
        onTap: onTap,
        child: Container(
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(AtlasTheme.rCard),
            border: Border.all(
              color: selected ? AtlasTheme.accent : AtlasTheme.line,
              width: selected ? 1.6 : 1,
            ),
          ),
          clipBehavior: Clip.antiAlias,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              if (showPreview && previewUrl.isNotEmpty)
                SizedBox(
                  height: 116,
                  width: double.infinity,
                  child: ShowcaseImage(url: previewUrl, label: item.name),
                ),
              Padding(
                padding: const EdgeInsets.fromLTRB(14, 13, 14, 14),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Expanded(
                          child: Text(
                            item.name,
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: const TextStyle(fontSize: 15.5, fontWeight: FontWeight.w700),
                          ),
                        ),
                        if (favorite)
                          const Icon(Icons.bookmark, size: 16, color: AtlasTheme.positive),
                      ],
                    ),
                    const SizedBox(height: 6),
                    Text(
                      s.summary(item),
                      maxLines: 2,
                      overflow: TextOverflow.ellipsis,
                      style: const TextStyle(fontSize: 12.5, height: 1.42, color: AtlasTheme.textSecondary),
                    ),
                    const SizedBox(height: 10),
                    Wrap(
                      spacing: 6,
                      runSpacing: 6,
                      children: [
                        _Tag(text: s.kind(item.kind), color: AtlasTheme.accent),
                        _Tag(text: s.access(item.access), color: AtlasTheme.info),
                        if (item.hasExamples)
                          _Tag(text: '${item.showcases.length} ${s.t('examples').toLowerCase()}', color: AtlasTheme.positive),
                        if (item.hasPricing) _Tag(text: item.pricing.from, color: AtlasTheme.warning),
                        if (version != null) _Tag(text: 'v$version'),
                      ],
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _Tag extends StatelessWidget {
  const _Tag({required this.text, this.color});
  final String text;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    final tone = color ?? AtlasTheme.textFaint;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: BoxDecoration(
        color: tone.withValues(alpha: 0.11),
        borderRadius: BorderRadius.circular(AtlasTheme.rTag),
      ),
      child: Text(
        text,
        maxLines: 1,
        overflow: TextOverflow.ellipsis,
        style: TextStyle(fontSize: 11, fontWeight: FontWeight.w600, color: tone),
      ),
    );
  }
}
