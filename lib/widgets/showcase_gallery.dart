import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:url_launcher/url_launcher.dart';

import '../app_state.dart';
import '../i18n/app_strings.dart';
import '../models/catalog_item.dart';
import '../theme.dart';

/// Horizontal strip of "what this looks like in production" cards.
class ShowcaseGallery extends StatelessWidget {
  const ShowcaseGallery({required this.item, this.height = 190, super.key});

  final CatalogItem item;
  final double height;

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final s = state.s;
    final examples = state.examplesFor(item, s.t('officialPage'));
    if (examples.isEmpty) {
      return Text(s.t('noExamples'), style: const TextStyle(color: AtlasTheme.textFaint));
    }
    return SizedBox(
      height: height,
      child: ListView.separated(
        scrollDirection: Axis.horizontal,
        itemCount: examples.length,
        separatorBuilder: (_, __) => const SizedBox(width: 12),
        itemBuilder: (context, index) => _ShowcaseCard(showcase: examples[index]),
      ),
    );
  }
}

class _ShowcaseCard extends StatelessWidget {
  const _ShowcaseCard({required this.showcase});

  final Showcase showcase;

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final note = showcase.noteFor(state.language.code);
    return SizedBox(
      width: 250,
      child: Material(
        color: AtlasTheme.ink,
        borderRadius: BorderRadius.circular(AtlasTheme.rCard),
        clipBehavior: Clip.antiAlias,
        child: InkWell(
          onTap: () => _open(context),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Expanded(
                child: Stack(
                  fit: StackFit.expand,
                  children: [
                    ShowcaseImage(url: state.previewUrlFor(showcase), label: showcase.title),
                    Positioned(
                      left: 8,
                      top: 8,
                      child: _KindTag(kind: showcase.kind),
                    ),
                  ],
                ),
              ),
              Padding(
                padding: const EdgeInsets.fromLTRB(12, 10, 12, 12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(showcase.title,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: const TextStyle(fontWeight: FontWeight.w700, fontSize: 13)),
                    if (note.isNotEmpty) ...[
                      const SizedBox(height: 3),
                      Text(note,
                          maxLines: 2,
                          overflow: TextOverflow.ellipsis,
                          style: const TextStyle(color: AtlasTheme.textMuted, fontSize: 11, height: 1.3)),
                    ],
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Future<void> _open(BuildContext context) async {
    final uri = Uri.tryParse(showcase.url);
    if (uri == null) return;
    await launchUrl(uri, mode: LaunchMode.externalApplication);
  }
}

class _KindTag extends StatelessWidget {
  const _KindTag({required this.kind});

  final String kind;

  @override
  Widget build(BuildContext context) {
    final color = switch (kind) {
      'gallery' => AtlasTheme.accent,
      'demo' => AtlasTheme.info,
      'code' => AtlasTheme.positive,
      _ => AtlasTheme.textSecondary,
    };
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: BoxDecoration(
        color: Colors.black.withValues(alpha: .55),
        borderRadius: BorderRadius.circular(AtlasTheme.rTag),
        border: Border.all(color: color.withValues(alpha: .5)),
      ),
      child: Text(kind, style: TextStyle(color: color, fontSize: 10, fontWeight: FontWeight.w700)),
    );
  }
}

/// Image that loads through [ImageCacheService] so a preview seen once keeps
/// working with no network. Screenshot services answer with a placeholder on
/// the first hit, hence the single retry.
class ShowcaseImage extends StatefulWidget {
  const ShowcaseImage({required this.url, required this.label, super.key});

  final String url;
  final String label;

  @override
  State<ShowcaseImage> createState() => _ShowcaseImageState();
}

class _ShowcaseImageState extends State<ShowcaseImage> {
  Uint8List? _bytes;
  bool _loading = false;
  bool _failed = false;
  int _attempt = 0;

  @override
  void initState() {
    super.initState();
    _load();
  }

  @override
  void didUpdateWidget(covariant ShowcaseImage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.url != widget.url) {
      _bytes = null;
      _failed = false;
      _attempt = 0;
      _load();
    }
  }

  Future<void> _load() async {
    if (widget.url.isEmpty || _loading) return;
    final state = AppScope.of(context);
    setState(() => _loading = true);
    final bytes = await state.imageCache.load(widget.url);
    if (!mounted) return;
    if (bytes == null) {
      final resolver = state.showcases;
      if (resolver.shouldRetry(state.previewProvider, _attempt)) {
        final delay = resolver.retryDelay(_attempt);
        _attempt++;
        setState(() => _loading = false);
        await Future<void>.delayed(delay);
        if (mounted) await _load();
        return;
      }
      setState(() {
        _loading = false;
        _failed = true;
      });
      return;
    }
    setState(() {
      _bytes = bytes;
      _loading = false;
      _failed = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    final bytes = _bytes;
    if (bytes != null) {
      return Image.memory(bytes, fit: BoxFit.cover, gaplessPlayback: true);
    }
    return _Placeholder(
      label: widget.label,
      loading: _loading,
      failed: _failed || widget.url.isEmpty,
      onRetry: widget.url.isEmpty ? null : _load,
    );
  }
}

class _Placeholder extends StatelessWidget {
  const _Placeholder({
    required this.label,
    required this.loading,
    required this.failed,
    this.onRetry,
  });

  final String label;
  final bool loading;
  final bool failed;
  final VoidCallback? onRetry;

  @override
  Widget build(BuildContext context) {
    final letter = label.isEmpty ? '?' : label.characters.first.toUpperCase();
    return DecoratedBox(
      decoration: const BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [Color(0x267D6BFF), Color(0x1F3ED6C5)],
        ),
      ),
      child: Center(
        child: loading
            ? const SizedBox(width: 22, height: 22, child: CircularProgressIndicator(strokeWidth: 2))
            : failed && onRetry != null
                ? IconButton(
                    onPressed: onRetry,
                    icon: const Icon(Icons.refresh, color: AtlasTheme.textMuted),
                    tooltip: AppScope.of(context).s.t('loadPreview'),
                  )
                : Text(
                    letter,
                    style: const TextStyle(fontSize: 34, fontWeight: FontWeight.w800, color: AtlasTheme.textGhost),
                  ),
      ),
    );
  }
}
