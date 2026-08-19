import '../models/catalog_item.dart';

/// Where preview images come from when a showcase has no curated image.
enum PreviewProvider { mshots, thumio, off }

extension PreviewProviderInfo on PreviewProvider {
  String get code => name;

  String get label => switch (this) {
        PreviewProvider.mshots => 'WordPress mShots',
        PreviewProvider.thumio => 'thum.io',
        PreviewProvider.off => 'off',
      };

  static PreviewProvider fromCode(String code) {
    for (final value in PreviewProvider.values) {
      if (value.name == code) return value;
    }
    return PreviewProvider.mshots;
  }
}

/// Resolves the list of examples shown for an entry and the image URL used for
/// each one.
///
/// Curated examples live in the catalog database. Entries without curated
/// examples still get one card pointing at their official page, so every item
/// in the catalog is visual.
class ShowcaseResolver {
  const ShowcaseResolver();

  static const previewWidth = 1000;
  static const previewHeight = 625;

  List<Showcase> examplesFor(CatalogItem item, {required String officialLabel}) {
    if (item.showcases.isNotEmpty) return item.showcases;
    if (item.url.isEmpty) return const [];
    return [
      Showcase(
        title: officialLabel,
        url: item.url,
        kind: 'site',
        note: '',
      ),
    ];
  }

  /// Empty string means "do not show an image".
  ///
  /// The `off` check comes first on purpose: a curated image is still a request
  /// to a third-party host, so "off means no external requests" has to hold for
  /// curated images too, not only for generated screenshots.
  String imageUrl(Showcase showcase, PreviewProvider provider) {
    if (provider == PreviewProvider.off) return '';
    if (showcase.image.isNotEmpty) return showcase.image;
    final target = showcase.url;
    if (!target.startsWith('https://')) return '';
    return switch (provider) {
      PreviewProvider.mshots =>
        'https://s.wordpress.com/mshots/v1/${Uri.encodeComponent(target)}?w=$previewWidth&h=$previewHeight',
      PreviewProvider.thumio =>
        'https://image.thum.io/get/width/$previewWidth/crop/$previewHeight/$target',
      PreviewProvider.off => '',
    };
  }

  /// Screenshot services render asynchronously and answer with a placeholder
  /// on the first request, so a single retry is worth it.
  bool shouldRetry(PreviewProvider provider, int attempt) =>
      provider == PreviewProvider.mshots && attempt < 2;

  Duration retryDelay(int attempt) => Duration(seconds: 3 * (attempt + 1));
}
