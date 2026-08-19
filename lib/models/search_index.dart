/// Pre-normalized text of one catalog entry.
///
/// Normalizing is lowercasing plus two regular expressions over the whole
/// searchable text of an entry. Doing that for a few thousand entries on every
/// keystroke is what made search feel heavy, so the shipped database stores the
/// result and [SearchEngine] only ever compares already-normalized strings.
/// Entries that never came from the database — custom entries, entries fetched
/// by the live update — build theirs once and keep it.
class SearchIndex {
  const SearchIndex({
    required this.name,
    required this.category,
    required this.tags,
    required this.text,
  });

  /// Normalized display name.
  final String name;

  /// Normalized category label.
  final String category;

  /// Normalized tags, one per element.
  final List<String> tags;

  /// Normalized haystack: name, kind, category, summaries, access, tags, tips.
  final String text;
}
