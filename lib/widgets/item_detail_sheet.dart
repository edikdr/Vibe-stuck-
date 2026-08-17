import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:url_launcher/url_launcher.dart';

import '../app_state.dart';
import '../models/catalog_item.dart';
import '../theme.dart';
import 'showcase_gallery.dart';

/// Opens the entry as a modal sheet. Used on phones and on narrow windows;
/// wide desktop windows embed [ItemDetailView] in a side pane instead.
Future<void> showItemDetailSheet(BuildContext context, CatalogItem item) {
  final state = AppScope.of(context);
  state.select(item.id);
  return showModalBottomSheet<void>(
    context: context,
    isScrollControlled: true,
    backgroundColor: AtlasTheme.panel,
    shape: const RoundedRectangleBorder(
      borderRadius: BorderRadius.vertical(top: Radius.circular(AtlasTheme.rSheet)),
      side: BorderSide(color: AtlasTheme.lineStrong),
    ),
    builder: (_) => DraggableScrollableSheet(
      expand: false,
      initialChildSize: 0.9,
      minChildSize: 0.5,
      maxChildSize: 0.96,
      builder: (context, controller) => ItemDetailView(
        item: item,
        scrollController: controller,
        showHandle: true,
      ),
    ),
  ).whenComplete(() => state.select(null));
}

/// The full record of one catalog entry: examples, pricing, install command,
/// AI context export. Layout agnostic on purpose so the same widget serves the
/// mobile sheet and the desktop detail pane.
class ItemDetailView extends StatelessWidget {
  const ItemDetailView({
    required this.item,
    this.scrollController,
    this.showHandle = false,
    this.onClose,
    super.key,
  });

  final CatalogItem item;
  final ScrollController? scrollController;
  final bool showHandle;
  final VoidCallback? onClose;

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final s = state.s;
    final version = state.versionFor(item.id);
    final health = state.healthFor(item.id);
    final favorite = state.isFavorite(item.id);

    return ListView(
      controller: scrollController,
      padding: const EdgeInsets.fromLTRB(20, 12, 20, 32),
      children: [
        if (showHandle)
          Center(
            child: Container(
              width: 44,
              height: 4,
              margin: const EdgeInsets.only(bottom: 18),
              decoration: BoxDecoration(
                color: AtlasTheme.lineStrong,
                borderRadius: BorderRadius.circular(4),
              ),
            ),
          ),
        Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Expanded(
              child: Text(
                item.name,
                style: const TextStyle(fontSize: 23, fontWeight: FontWeight.w800, height: 1.2),
              ),
            ),
            IconButton(
              tooltip: favorite ? s.t('removeFavorite') : s.t('addFavorite'),
              onPressed: () => state.toggleFavorite(item.id),
              icon: Icon(favorite ? Icons.bookmark : Icons.bookmark_border,
                  color: favorite ? AtlasTheme.positive : null),
            ),
            if (onClose != null)
              IconButton(
                tooltip: 'Esc',
                onPressed: onClose,
                icon: const Icon(Icons.close_rounded),
              ),
          ],
        ),
        const SizedBox(height: 10),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            _Chip(label: s.kind(item.kind), color: AtlasTheme.accent),
            _Chip(label: s.access(item.access), color: AtlasTheme.info),
            _Chip(label: s.category(item.category)),
            if (item.isLive) _Chip(label: s.t('liveSource'), color: AtlasTheme.positive),
            if (version != null) _Chip(label: '${s.t('latestVersion')}: $version'),
            if (health != null)
              _Chip(
                label: health ? s.t('reachable') : s.t('unreachable'),
                color: health ? AtlasTheme.positive : AtlasTheme.warning,
              ),
          ],
        ),
        const SizedBox(height: 16),
        Text(s.summary(item), style: const TextStyle(fontSize: 15, height: 1.55)),
        const SizedBox(height: 20),

        // ---------- examples ----------
        _SectionTitle(title: s.t('examples'), hint: s.t('examplesHint')),
        const SizedBox(height: 10),
        ShowcaseGallery(item: item),
        const SizedBox(height: 20),

        if (item.hasPricing) ...[
          _PricingBlock(item: item),
          const SizedBox(height: 20),
        ],

        _SectionTitle(title: s.t('miniTip')),
        const SizedBox(height: 8),
        Container(
          padding: const EdgeInsets.all(14),
          decoration: BoxDecoration(
            color: AtlasTheme.panelRaised,
            borderRadius: BorderRadius.circular(AtlasTheme.rCard),
            border: Border.all(color: AtlasTheme.line),
          ),
          child: Text(s.tip(item), style: const TextStyle(fontSize: 14, height: 1.5)),
        ),
        const SizedBox(height: 20),

        if (item.install.isNotEmpty) ...[
          _SectionTitle(title: s.t('installCommand')),
          const SizedBox(height: 8),
          _CopyBox(text: item.install, toast: s.t('copied')),
          const SizedBox(height: 20),
        ],

        if (item.tags.isNotEmpty) ...[
          _SectionTitle(title: s.t('tagsLabel')),
          const SizedBox(height: 8),
          Wrap(spacing: 8, runSpacing: 8, children: item.tags.map((tag) => _Chip(label: '#$tag')).toList()),
          const SizedBox(height: 20),
        ],

        Row(
          children: [
            Expanded(
              child: FilledButton.icon(
                onPressed: () => _open(item.url),
                icon: const Icon(Icons.open_in_new_rounded, size: 18),
                label: Text(s.t('openOfficial')),
              ),
            ),
            const SizedBox(width: 10),
            Expanded(
              child: OutlinedButton.icon(
                onPressed: () async {
                  await Clipboard.setData(ClipboardData(text: state.contextForItem(item)));
                  if (context.mounted) _toast(context, s.t('copiedForAi'));
                },
                icon: const Icon(Icons.smart_toy_outlined, size: 18),
                label: Text(s.t('copyForAi')),
              ),
            ),
          ],
        ),
        const SizedBox(height: 12),
        Text(
          '${s.t('worksWith')}: ${item.compatibility.join(' · ')}   ·   ${s.t('verified')}: ${item.verifiedAt}',
          style: const TextStyle(fontSize: 12, color: AtlasTheme.textFaint),
        ),
        if (item.isCustom) ...[
          const SizedBox(height: 16),
          TextButton.icon(
            onPressed: () {
              state.removeCustom(item.id);
              Navigator.of(context).maybePop();
            },
            icon: const Icon(Icons.delete_outline, size: 18),
            label: Text(s.t('removeCustom')),
          ),
        ],
      ],
    );
  }

  Future<void> _open(String url) async {
    final uri = Uri.tryParse(url);
    if (uri == null) return;
    await launchUrl(uri, mode: LaunchMode.externalApplication);
  }
}

class _PricingBlock extends StatelessWidget {
  const _PricingBlock({required this.item});
  final CatalogItem item;

  @override
  Widget build(BuildContext context) {
    final s = AppScope.of(context).s;
    final pricing = item.pricing;
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: AtlasTheme.panelRaised,
        borderRadius: BorderRadius.circular(AtlasTheme.rCard),
        border: Border.all(color: AtlasTheme.line),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.payments_outlined, size: 18, color: AtlasTheme.info),
              const SizedBox(width: 8),
              Text(s.t('pricingTitle'), style: const TextStyle(fontWeight: FontWeight.w700)),
            ],
          ),
          const SizedBox(height: 12),
          _Row(label: s.t('pricingModel'), value: pricing.model),
          if (pricing.from.isNotEmpty) _Row(label: s.t('priceFrom'), value: pricing.from),
          if (pricing.freeQuota.isNotEmpty) _Row(label: s.t('freeQuota'), value: pricing.freeQuota),
          if (pricing.checkedAt.isNotEmpty) _Row(label: s.t('priceChecked'), value: pricing.checkedAt),
          if (pricing.url.isNotEmpty) ...[
            const SizedBox(height: 8),
            TextButton.icon(
              onPressed: () async {
                final uri = Uri.tryParse(pricing.url);
                if (uri != null) await launchUrl(uri, mode: LaunchMode.externalApplication);
              },
              icon: const Icon(Icons.north_east_rounded, size: 16),
              label: Text(s.t('openPricing')),
            ),
          ],
          const SizedBox(height: 4),
          Text(s.t('pricingDisclaimer'),
              style: const TextStyle(fontSize: 11.5, height: 1.45, color: AtlasTheme.textFaint)),
        ],
      ),
    );
  }
}

class _Row extends StatelessWidget {
  const _Row({required this.label, required this.value});
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 118,
            child: Text(label, style: const TextStyle(fontSize: 12.5, color: AtlasTheme.textFaint)),
          ),
          Expanded(child: Text(value, style: const TextStyle(fontSize: 13, height: 1.4))),
        ],
      ),
    );
  }
}

class _SectionTitle extends StatelessWidget {
  const _SectionTitle({required this.title, this.hint});
  final String title;
  final String? hint;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(title.toUpperCase(),
            style: const TextStyle(fontSize: 11.5, fontWeight: FontWeight.w800, letterSpacing: 1.1, color: AtlasTheme.textMuted)),
        if (hint != null && hint!.isNotEmpty) ...[
          const SizedBox(height: 4),
          Text(hint!, style: const TextStyle(fontSize: 12, color: AtlasTheme.textFaint, height: 1.4)),
        ],
      ],
    );
  }
}

class _CopyBox extends StatelessWidget {
  const _CopyBox({required this.text, required this.toast});
  final String text;
  final String toast;

  @override
  Widget build(BuildContext context) {
    return InkWell(
      borderRadius: BorderRadius.circular(AtlasTheme.rControl),
      onTap: () async {
        await Clipboard.setData(ClipboardData(text: text));
        if (context.mounted) _toast(context, toast);
      },
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 13),
        decoration: BoxDecoration(
          color: AtlasTheme.panelSunken,
          borderRadius: BorderRadius.circular(AtlasTheme.rControl),
          border: Border.all(color: AtlasTheme.line),
        ),
        child: Row(
          children: [
            Expanded(
              child: Text(text,
                  style: const TextStyle(fontFamily: 'monospace', fontSize: 13, height: 1.4)),
            ),
            const SizedBox(width: 10),
            const Icon(Icons.copy_rounded, size: 16, color: AtlasTheme.textFaint),
          ],
        ),
      ),
    );
  }
}

class _Chip extends StatelessWidget {
  const _Chip({required this.label, this.color});
  final String label;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    final tone = color ?? AtlasTheme.textMuted;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: tone.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(AtlasTheme.rTag),
        border: Border.all(color: tone.withValues(alpha: 0.35)),
      ),
      child: Text(label, style: TextStyle(fontSize: 12, fontWeight: FontWeight.w600, color: tone)),
    );
  }
}

void _toast(BuildContext context, String message) {
  ScaffoldMessenger.of(context)
    ..hideCurrentSnackBar()
    ..showSnackBar(SnackBar(content: Text(message), duration: const Duration(seconds: 2)));
}
