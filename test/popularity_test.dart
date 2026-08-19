import 'package:flutter_test/flutter_test.dart';
import 'package:vibestack_atlas/models/catalog_item.dart';
import 'package:vibestack_atlas/services/live_update_service.dart';
import 'package:vibestack_atlas/services/search_engine.dart';

CatalogItem _item(String id, String name, {String url = 'https://example.com'}) => CatalogItem(
      id: id,
      name: name,
      kind: ItemKind.library,
      category: 'UI и frontend',
      summary: 'summary',
      url: url,
      access: AccessType.openSource,
      compatibility: const ['both'],
      tags: const ['tag'],
      tip: 'tip',
      verifiedAt: '2026-08-12',
    );

void main() {
  group('githubRepositoryOf', () {
    test('extracts owner and repo from a repository url', () {
      expect(
        LiveUpdateService.githubRepositoryOf('https://github.com/facebook/react'),
        'facebook/react',
      );
    });

    test('keeps only the first two path segments', () {
      expect(
        LiveUpdateService.githubRepositoryOf('https://github.com/withastro/astro/tree/main/docs'),
        'withastro/astro',
      );
    });

    test('strips a trailing .git suffix', () {
      expect(
        LiveUpdateService.githubRepositoryOf('https://github.com/rust-lang/cargo.git'),
        'rust-lang/cargo',
      );
    });

    test('rejects reserved paths that are not repositories', () {
      expect(LiveUpdateService.githubRepositoryOf('https://github.com/features/actions'), isNull);
      expect(LiveUpdateService.githubRepositoryOf('https://github.com/topics/rust'), isNull);
    });

    test('rejects non-GitHub and incomplete urls', () {
      expect(LiveUpdateService.githubRepositoryOf('https://gitlab.com/owner/repo'), isNull);
      expect(LiveUpdateService.githubRepositoryOf('https://github.com/onlyowner'), isNull);
      expect(LiveUpdateService.githubRepositoryOf('not a url at all'), isNull);
    });
  });

  group('popular sort', () {
    final engine = SearchEngine();
    final items = [
      _item('small', 'Small'),
      _item('big', 'Big'),
      _item('unranked', 'Unranked'),
    ];

    test('orders by star count, highest first', () {
      final result = engine.search(
        items,
        '',
        sort: 'popular',
        stars: const {'small': 10, 'big': 900},
      );
      expect(result.map((item) => item.id).take(2), ['big', 'small']);
    });

    test('places entries with no star data last', () {
      final result = engine.search(
        items,
        '',
        sort: 'popular',
        stars: const {'small': 10, 'big': 900},
      );
      expect(result.last.id, 'unranked');
    });

    test('falls back to name order when nothing is ranked', () {
      final result = engine.search(items, '', sort: 'popular');
      expect(result.map((item) => item.name), ['Big', 'Small', 'Unranked']);
    });
  });
}
