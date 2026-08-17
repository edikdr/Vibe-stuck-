/// A real project, site or gallery that demonstrates what a catalog entry
/// looks like in production. [image] is optional: when it is empty the app
/// renders a live screenshot of [url] through the configured preview provider.
class Showcase {
  const Showcase({
    required this.title,
    required this.url,
    this.kind = 'site',
    this.note = '',
    this.image = '',
    this.localizedNote = const {},
  });

  final String title;
  final String url;

  /// gallery | site | demo | code
  final String kind;
  final String note;
  final String image;
  final Map<String, String> localizedNote;

  String noteFor(String languageCode) {
    final explicit = localizedNote[languageCode];
    if (explicit != null && explicit.isNotEmpty) return explicit;
    return note;
  }

  factory Showcase.fromJson(Map<String, dynamic> json) => Showcase(
        title: json['title'] as String? ?? '',
        url: json['url'] as String? ?? '',
        kind: json['kind'] as String? ?? 'site',
        note: json['note'] as String? ?? '',
        image: json['image'] as String? ?? '',
        localizedNote: Map<String, String>.from(json['localizedNote'] as Map? ?? const {}),
      );

  Map<String, dynamic> toJson() => {
        'title': title,
        'url': url,
        'kind': kind,
        'note': note,
        if (image.isNotEmpty) 'image': image,
        'localizedNote': localizedNote,
      };
}

/// Commercial terms for an entry. Everything except [model] is optional
/// because most of the catalog is free or open source.
class PricingInfo {
  const PricingInfo({
    this.model = 'free',
    this.from = '',
    this.freeQuota = '',
    this.url = '',
    this.currency = 'USD',
    this.checkedAt = '',
  });

  /// free | openSource | freemium | usage | subscription | credits | commission | hybrid
  final String model;

  /// Short human readable entry price, e.g. "from ~$20 / month".
  final String from;
  final String freeQuota;
  final String url;
  final String currency;
  final String checkedAt;

  bool get hasPrice => from.isNotEmpty;
  bool get isFree => model == 'free' || model == 'openSource';
  bool get isPaidOnly => hasPrice && freeQuota.isEmpty;

  factory PricingInfo.fromJson(Map<String, dynamic> json) => PricingInfo(
        model: json['model'] as String? ?? 'free',
        from: json['from'] as String? ?? '',
        freeQuota: json['freeQuota'] as String? ?? '',
        url: json['url'] as String? ?? '',
        currency: json['currency'] as String? ?? 'USD',
        checkedAt: json['checkedAt'] as String? ?? '',
      );

  Map<String, dynamic> toJson() => {
        'model': model,
        if (from.isNotEmpty) 'from': from,
        if (freeQuota.isNotEmpty) 'freeQuota': freeQuota,
        if (url.isNotEmpty) 'url': url,
        'currency': currency,
        if (checkedAt.isNotEmpty) 'checkedAt': checkedAt,
      };
}
