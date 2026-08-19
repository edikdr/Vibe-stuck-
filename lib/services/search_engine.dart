import '../models/catalog_item.dart';

/// Normalized text of entries that did not come from the database — custom
/// entries and entries discovered by the live update. Keyed by the entry
/// itself, so it is dropped as soon as the entry is.
final Expando<SearchIndex> _computedIndex = Expando<SearchIndex>('atlas-search-index');

class SearchEngine {
  static const _intentMap = <String, List<String>>{
    'сайт': ['frontend', 'ui', 'css', 'component', 'web'],
    'интерфейс': ['frontend', 'ui', 'design', 'component'],
    'база': ['database', 'postgres', 'sql', 'storage'],
    'данные': ['database', 'api', 'storage', 'dataset'],
    'авторизация': ['auth', 'authentication', 'oauth', 'login'],
    'вход': ['auth', 'authentication', 'oauth', 'login'],
    'тест': ['testing', 'e2e', 'unit', 'quality'],
    'проверка': ['testing', 'lint', 'quality', 'security'],
    'ошибки': ['monitoring', 'logging', 'debug'],
    'нейросеть': ['ai', 'llm', 'agent', 'inference'],
    'ии': ['ai', 'llm', 'agent', 'inference'],
    'документация': ['docs', 'documentation', 'context'],
    'картинки': ['image', 'media', 'photo', 'vision'],
    'музыка': ['music', 'audio', 'lyrics'],
    'карта': ['maps', 'geo', 'geocoding', 'routing'],
    'погода': ['weather', 'forecast'],
    'дешево': ['free', 'no-key', 'free-tier'],
    'бесплатно': ['free', 'no-key', 'free-tier', 'open-source'],
    'без ключа': ['no-key', 'public-api'],
    'деплой': ['deploy', 'hosting', 'cloud', 'ci'],
    'сервер': ['backend', 'server', 'api'],
    'мобильное': ['mobile', 'flutter', 'android'],
    'website': ['frontend', 'ui', 'css', 'component', 'web'],
    'interface': ['frontend', 'ui', 'design', 'component'],
    'database': ['database', 'postgres', 'sql', 'storage'],
    'authentication': ['auth', 'oauth', 'login'],
    'testing': ['testing', 'e2e', 'unit', 'quality'],
    'errors': ['monitoring', 'logging', 'debug'],
    'documentation': ['docs', 'documentation', 'context'],
    'images': ['image', 'media', 'photo', 'vision'],
    'weather': ['weather', 'forecast'],
    'free': ['free', 'no-key', 'free-tier', 'open-source'],
    'deployment': ['deploy', 'hosting', 'cloud', 'ci'],
    'webseite': ['frontend', 'ui', 'css', 'component', 'web'],
    'oberfläche': ['frontend', 'ui', 'design', 'component'],
    'schnittstelle': ['frontend', 'ui', 'design', 'component'],
    'datenbank': ['database', 'postgres', 'sql', 'storage'],
    'daten': ['database', 'api', 'storage', 'dataset'],
    'anmeldung': ['auth', 'authentication', 'oauth', 'login'],
    'authentifizierung': ['auth', 'authentication', 'oauth', 'login'],
    'tests': ['testing', 'e2e', 'unit', 'quality'],
    'fehler': ['monitoring', 'logging', 'debug'],
    'dokumentation': ['docs', 'documentation', 'context'],
    'bilder': ['image', 'media', 'photo', 'vision'],
    'wetter': ['weather', 'forecast'],
    'kostenlos': ['free', 'no-key', 'free-tier', 'open-source'],
    'ohne schlüssel': ['no-key', 'public-api'],
    'bereitstellung': ['deploy', 'hosting', 'cloud', 'ci'],
    'mobil': ['mobile', 'flutter', 'android'],
  };

  List<CatalogItem> search(
    Iterable<CatalogItem> source,
    String rawQuery, {
    ItemKind? kind,
    AccessType? access,
    String compatibility = 'all',
    String category = 'all',
    String group = 'all',
    String price = 'any',
    bool onlyExamples = false,
    bool onlyLive = false,
    bool onlyFavorites = false,
    Set<String> favorites = const {},
    String sort = 'relevance',
    Map<String, int> stars = const {},
  }) {
    final query = _normalize(rawQuery);
    final queryTokens = _expandedTokens(query);
    final scored = <({CatalogItem item, int score})>[];

    for (final item in source) {
      if (kind != null && item.kind != kind) continue;
      if (access != null && item.access != access) continue;
      if (category != 'all' && item.category != category) continue;
      if (group != 'all' && item.categoryGroup != group) continue;
      if (onlyExamples && !item.hasExamples) continue;
      if (onlyLive && !item.isLive) continue;
      if (onlyFavorites && !favorites.contains(item.id)) continue;
      if (!_matchesPrice(item, price)) continue;
      if (compatibility != 'all' &&
          !item.compatibility.contains('both') &&
          !item.compatibility.contains(compatibility)) {
        continue;
      }

      final score = query.isEmpty ? 1 : _score(item, query, queryTokens);
      if (score > 0) scored.add((item: item, score: score));
    }

    scored.sort((a, b) => switch (sort) {
          'name' => _byName(a.item, b.item),
          'new' => _byVerified(a.item, b.item),
          'free' => _byFreeFirst(a.item, b.item),
          'popular' => _byStars(a.item, b.item, stars),
          _ => _byScore(a, b),
        });
    return scored.map((entry) => entry.item).toList(growable: false);
  }

  /// price: any | free | freemium | paid | withPrice
  bool _matchesPrice(CatalogItem item, String price) {
    return switch (price) {
      'free' => item.access == AccessType.noKey ||
          item.access == AccessType.free ||
          item.access == AccessType.openSource,
      'freemium' => item.access == AccessType.freeTier ||
          item.access == AccessType.mixed ||
          item.pricing.freeQuota.isNotEmpty,
      'paid' => item.access == AccessType.paid || item.pricing.isPaidOnly,
      'withPrice' => item.pricing.hasPrice,
      _ => true,
    };
  }

  int _byScore(({CatalogItem item, int score}) a, ({CatalogItem item, int score}) b) {
    final order = b.score.compareTo(a.score);
    return order != 0 ? order : _byName(a.item, b.item);
  }

  /// Entries with no known star count sort last rather than as zero, so an
  /// unranked entry never outranks a genuinely small project.
  int _byStars(CatalogItem a, CatalogItem b, Map<String, int> stars) {
    final left = stars[a.id];
    final right = stars[b.id];
    if (left == null && right == null) return _byName(a, b);
    if (left == null) return 1;
    if (right == null) return -1;
    final order = right.compareTo(left);
    return order != 0 ? order : _byName(a, b);
  }

  int _byName(CatalogItem a, CatalogItem b) =>
      a.name.toLowerCase().compareTo(b.name.toLowerCase());

  int _byVerified(CatalogItem a, CatalogItem b) {
    final order = b.verifiedAt.compareTo(a.verifiedAt);
    return order != 0 ? order : _byName(a, b);
  }

  int _byFreeFirst(CatalogItem a, CatalogItem b) {
    int rank(CatalogItem item) => switch (item.access) {
          AccessType.noKey => 0,
          AccessType.openSource => 1,
          AccessType.free => 2,
          AccessType.freeTier => 3,
          AccessType.mixed => 4,
          AccessType.paid => 5,
        };
    final order = rank(a).compareTo(rank(b));
    return order != 0 ? order : _byName(a, b);
  }

  Set<String> _expandedTokens(String query) {
    final result = query.split(' ').where((value) => value.isNotEmpty).toSet();
    for (final entry in _intentMap.entries) {
      if (query.contains(entry.key)) result.addAll(entry.value);
    }
    return result;
  }

  /// Normalized text for [item], from the database when it has it.
  ///
  /// Normalizing runs two regular expressions over the entry's whole text.
  /// Doing that per entry per keystroke is what made a large catalog feel
  /// slow, so it happens once — at build time for the shipped catalog, on
  /// first use for everything else.
  SearchIndex indexOf(CatalogItem item) {
    final shipped = item.searchIndex;
    if (shipped != null) return shipped;
    final cached = _computedIndex[item];
    if (cached != null) return cached;
    final built = SearchIndex(
      name: _normalize(item.name),
      category: _normalize(item.category),
      tags: item.tags.map(_normalize).toList(growable: false),
      text: _normalize(item.searchableText),
    );
    _computedIndex[item] = built;
    return built;
  }

  int _score(CatalogItem item, String query, Set<String> tokens) {
    final index = indexOf(item);
    final name = index.name;
    final category = index.category;
    final tags = index.tags;
    final body = index.text;
    var score = 0;

    if (name == query) score += 240;
    if (name.startsWith(query)) score += 150;
    if (name.contains(query)) score += 110;
    if (category.contains(query)) score += 55;
    if (body.contains(query)) score += 45;

    final words = name.split(' ');
    for (final token in tokens) {
      if (token.length < 2) continue;
      if (name == token) score += 90;
      if (name.contains(token)) score += 55;
      if (tags.any((tag) => tag == token)) score += 42;
      if (tags.any((tag) => tag.contains(token))) score += 28;
      if (category.contains(token)) score += 20;
      if (body.contains(token)) score += 10;
      if (words.any((word) => _distance(word, token) <= 1)) score += 18;
    }
    return score;
  }

  String _normalize(String value) => value
      .toLowerCase()
      .replaceAll('ё', 'е')
      .replaceAll(RegExp(r'[^a-zäöüßа-я0-9+#.\- ]'), ' ')
      .replaceAll(RegExp(r'\s+'), ' ')
      .trim();

  int _distance(String a, String b) {
    if ((a.length - b.length).abs() > 1) return 99;
    final previous = List<int>.generate(b.length + 1, (index) => index);
    for (var i = 0; i < a.length; i++) {
      var diagonal = previous[0];
      previous[0] = i + 1;
      for (var j = 0; j < b.length; j++) {
        final old = previous[j + 1];
        previous[j + 1] = [
          previous[j + 1] + 1,
          previous[j] + 1,
          diagonal + (a[i] == b[j] ? 0 : 1),
        ].reduce((x, y) => x < y ? x : y);
        diagonal = old;
      }
    }
    return previous.last;
  }
}
