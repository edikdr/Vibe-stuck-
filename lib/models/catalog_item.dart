import 'showcase.dart';

export 'showcase.dart';

enum ItemKind { api, library, mcp, skill }

enum AccessType { noKey, freeTier, openSource, free, mixed, paid }

class CatalogItem {
  const CatalogItem({
    required this.id,
    required this.name,
    required this.kind,
    required this.category,
    required this.summary,
    required this.url,
    required this.access,
    required this.compatibility,
    required this.tags,
    required this.tip,
    required this.verifiedAt,
    this.install = '',
    this.source = 'curated',
    this.sourceLanguage = 'ru',
    this.localizedSummary = const {},
    this.localizedTip = const {},
    this.isLive = false,
    this.isCustom = false,
    this.categoryGroup = 'devtools',
    this.showcases = const [],
    this.pricing = const PricingInfo(),
  });

  final String id;
  final String name;
  final ItemKind kind;
  final String category;
  final String summary;
  final String url;
  final AccessType access;
  final List<String> compatibility;
  final List<String> tags;
  final String tip;
  final String verifiedAt;
  final String install;
  final String source;
  final String sourceLanguage;
  final Map<String, String> localizedSummary;
  final Map<String, String> localizedTip;
  final bool isLive;
  final bool isCustom;
  final String categoryGroup;
  final List<Showcase> showcases;
  final PricingInfo pricing;

  bool get hasExamples => showcases.isNotEmpty;
  bool get hasPricing => pricing.hasPrice;

  String get searchableText => [
        name,
        kind.name,
        category,
        summary,
        access.name,
        ...compatibility,
        ...tags,
        tip,
      ].join(' ').toLowerCase();

  factory CatalogItem.fromJson(Map<String, dynamic> json) {
    final source = json['source'] as String? ?? 'curated';
    return CatalogItem(
      id: json['id'] as String,
      name: json['name'] as String,
      kind: ItemKind.values.byName(json['kind'] as String),
      category: json['category'] as String,
      summary: json['summary'] as String,
      url: json['url'] as String,
      access: AccessType.values.byName(json['access'] as String),
      compatibility: List<String>.from(json['compatibility'] as List? ?? const ['both']),
      tags: List<String>.from(json['tags'] as List? ?? const []),
      tip: json['tip'] as String? ?? '',
      verifiedAt: json['verifiedAt'] as String? ?? '2026-08-12',
      install: json['install'] as String? ?? '',
      source: source,
      sourceLanguage: json['sourceLanguage'] as String? ?? (source == 'curated' ? 'ru' : 'en'),
      localizedSummary: Map<String, String>.from(json['localizedSummary'] as Map? ?? const {}),
      localizedTip: Map<String, String>.from(json['localizedTip'] as Map? ?? const {}),
      isLive: json['isLive'] as bool? ?? false,
      isCustom: json['isCustom'] as bool? ?? false,
      categoryGroup: json['categoryGroup'] as String? ?? 'devtools',
      showcases: (json['showcases'] as List? ?? const [])
          .whereType<Map>()
          .map((raw) => Showcase.fromJson(Map<String, dynamic>.from(raw)))
          .toList(growable: false),
      pricing: PricingInfo.fromJson(
          Map<String, dynamic>.from(json['pricing'] as Map? ?? const {})),
    );
  }

  Map<String, dynamic> toJson() => {
        'id': id,
        'name': name,
        'kind': kind.name,
        'category': category,
        'summary': summary,
        'url': url,
        'access': access.name,
        'compatibility': compatibility,
        'tags': tags,
        'tip': tip,
        'verifiedAt': verifiedAt,
        'install': install,
        'source': source,
        'sourceLanguage': sourceLanguage,
        'localizedSummary': localizedSummary,
        'localizedTip': localizedTip,
        'isLive': isLive,
        'isCustom': isCustom,
        'categoryGroup': categoryGroup,
        'showcases': showcases.map((item) => item.toJson()).toList(),
        'pricing': pricing.toJson(),
      };
}

class UpdateEvent {
  const UpdateEvent({
    required this.id,
    required this.kind,
    required this.message,
    required this.createdAt,
  });

  final String id;
  final String kind;
  final String message;
  final DateTime createdAt;

  factory UpdateEvent.fromJson(Map<String, dynamic> json) => UpdateEvent(
        id: json['id'] as String,
        kind: json['kind'] as String,
        message: json['message'] as String,
        createdAt: DateTime.parse(json['createdAt'] as String),
      );

  Map<String, dynamic> toJson() => {
        'id': id,
        'kind': kind,
        'message': message,
        'createdAt': createdAt.toIso8601String(),
      };
}
