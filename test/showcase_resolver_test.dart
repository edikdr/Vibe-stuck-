import 'package:flutter_test/flutter_test.dart';
import 'package:vibestack_atlas/models/catalog_item.dart';
import 'package:vibestack_atlas/services/showcase_resolver.dart';

void main() {
  const resolver = ShowcaseResolver();

  const bare = CatalogItem(
    id: 'bare',
    name: 'Bare Tool',
    kind: ItemKind.library,
    category: 'UI и frontend',
    summary: 'No curated examples yet',
    url: 'https://example.com/docs',
    access: AccessType.openSource,
    compatibility: ['both'],
    tags: ['ui'],
    tip: 'Start here',
    verifiedAt: '2026-08-16',
  );

  const curated = CatalogItem(
    id: 'curated',
    name: 'Curated Tool',
    kind: ItemKind.library,
    category: 'UI и frontend',
    summary: 'Has a gallery',
    url: 'https://example.com',
    access: AccessType.openSource,
    compatibility: ['both'],
    tags: ['ui'],
    tip: 'Start here',
    verifiedAt: '2026-08-16',
    showcases: [
      Showcase(title: 'Gallery', url: 'https://example.com/showcase', kind: 'gallery'),
    ],
  );

  test('entries without curated examples still get an official card', () {
    final examples = resolver.examplesFor(bare, officialLabel: 'Official page');
    expect(examples, hasLength(1));
    expect(examples.first.url, bare.url);
    expect(examples.first.title, 'Official page');
  });

  test('curated examples win over the generated one', () {
    final examples = resolver.examplesFor(curated, officialLabel: 'Official page');
    expect(examples, hasLength(1));
    expect(examples.first.kind, 'gallery');
  });

  test('mShots url encodes the target and carries the preview size', () {
    final url = resolver.imageUrl(curated.showcases.first, PreviewProvider.mshots);
    expect(url, startsWith('https://s.wordpress.com/mshots/v1/'));
    expect(url, contains(Uri.encodeComponent('https://example.com/showcase')));
    expect(url, contains('w=${ShowcaseResolver.previewWidth}'));
  });

  test('turning previews off produces no image request at all', () {
    expect(resolver.imageUrl(curated.showcases.first, PreviewProvider.off), isEmpty);
  });

  test('a curated image is used as is, whatever the provider', () {
    const withImage = Showcase(
      title: 'Shot',
      url: 'https://example.com/site',
      image: 'https://cdn.example.com/shot.png',
    );
    expect(resolver.imageUrl(withImage, PreviewProvider.mshots), 'https://cdn.example.com/shot.png');
    expect(resolver.imageUrl(withImage, PreviewProvider.off), isEmpty);
  });

  test('only mShots is retried, and only once', () {
    expect(resolver.shouldRetry(PreviewProvider.mshots, 0), isTrue);
    expect(resolver.shouldRetry(PreviewProvider.mshots, 2), isFalse);
    expect(resolver.shouldRetry(PreviewProvider.thumio, 0), isFalse);
  });

  test('non-https targets are never sent to a screenshot service', () {
    const insecure = Showcase(title: 'Old', url: 'http://example.com');
    expect(resolver.imageUrl(insecure, PreviewProvider.mshots), isEmpty);
  });
}
