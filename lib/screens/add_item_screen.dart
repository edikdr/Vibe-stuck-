import 'package:flutter/material.dart';

import '../app_state.dart';
import '../i18n/app_strings.dart';
import '../models/catalog_item.dart';
import '../theme.dart';

class AddItemScreen extends StatefulWidget {
  const AddItemScreen({required this.onSaved, super.key});
  final VoidCallback onSaved;

  @override
  State<AddItemScreen> createState() => _AddItemScreenState();
}

class _AddItemScreenState extends State<AddItemScreen> {
  final _formKey = GlobalKey<FormState>();
  final _name = TextEditingController();
  final _summary = TextEditingController();
  final _url = TextEditingController();
  final _category = TextEditingController();
  final _tags = TextEditingController();
  final _tip = TextEditingController();
  final _install = TextEditingController();
  ItemKind _kind = ItemKind.library;
  AccessType _access = AccessType.openSource;
  String _compatibility = 'both';

  @override
  void dispose() {
    for (final controller in [_name, _summary, _url, _category, _tags, _tip, _install]) {
      controller.dispose();
    }
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final s = state.s;
    return SafeArea(
      child: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 760),
            child: Form(
              key: _formKey,
              child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
                Text(s.t('addTitle'), style: Theme.of(context).textTheme.displaySmall),
                const SizedBox(height: 8),
                Text(s.t('addSubtitle'), style: const TextStyle(color: AtlasTheme.textSecondary)),
                const SizedBox(height: 24),
                Row(children: [
                  Expanded(
                    child: DropdownButtonFormField<ItemKind>(
                      initialValue: _kind,
                      decoration: InputDecoration(labelText: s.t('type')),
                      items: ItemKind.values.map((value) => DropdownMenuItem(value: value, child: Text(s.kind(value)))).toList(),
                      onChanged: (value) => setState(() => _kind = value ?? _kind),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: DropdownButtonFormField<AccessType>(
                      initialValue: _access,
                      decoration: InputDecoration(labelText: s.t('access')),
                      items: AccessType.values.map((value) => DropdownMenuItem(value: value, child: Text(s.access(value)))).toList(),
                      onChanged: (value) => setState(() => _access = value ?? _access),
                    ),
                  ),
                ]),
                const SizedBox(height: 14),
                TextFormField(controller: _name, decoration: InputDecoration(labelText: s.t('nameRequired')), validator: _required),
                const SizedBox(height: 14),
                TextFormField(controller: _summary, decoration: InputDecoration(labelText: s.t('summaryRequired')), maxLines: 3, validator: _required),
                const SizedBox(height: 14),
                TextFormField(
                  controller: _url,
                  decoration: InputDecoration(labelText: s.t('officialUrl')),
                  keyboardType: TextInputType.url,
                  validator: (value) {
                    final uri = Uri.tryParse(value?.trim() ?? '');
                    return uri == null || uri.scheme != 'https' ? s.t('invalidHttps') : null;
                  },
                ),
                const SizedBox(height: 14),
                TextFormField(controller: _category, decoration: InputDecoration(labelText: s.t('categoryRequired'), hintText: s.t('categoryExample')), validator: _required),
                const SizedBox(height: 14),
                TextFormField(controller: _tags, decoration: InputDecoration(labelText: s.t('tagsLabel'), hintText: s.t('tagsExample'))),
                const SizedBox(height: 14),
                TextFormField(controller: _tip, decoration: InputDecoration(labelText: s.t('miniTip'), hintText: s.t('tipExample')), maxLines: 2),
                const SizedBox(height: 14),
                TextFormField(controller: _install, decoration: InputDecoration(labelText: s.t('installCommand'), hintText: 'npm install ...'), maxLines: 2),
                const SizedBox(height: 18),
                Text(s.t('compatibility'), style: const TextStyle(fontWeight: FontWeight.w700)),
                const SizedBox(height: 8),
                SegmentedButton<String>(
                  segments: [
                    ButtonSegment(value: 'both', label: Text(s.t('both'))),
                    const ButtonSegment(value: 'codex', label: Text('Codex')),
                    const ButtonSegment(value: 'claude', label: Text('Claude')),
                  ],
                  selected: {_compatibility},
                  onSelectionChanged: (value) => setState(() => _compatibility = value.first),
                ),
                const SizedBox(height: 26),
                SizedBox(
                  width: double.infinity,
                  child: FilledButton.icon(onPressed: _save, icon: const Icon(Icons.add), label: Text(s.t('addToCatalog'))),
                ),
              ]),
            ),
          ),
        ),
      ),
    );
  }

  String? _required(String? value) => value == null || value.trim().isEmpty ? AppScope.of(context).s.t('fillField') : null;

  Future<void> _save() async {
    if (!_formKey.currentState!.validate()) return;
    final slug = _name.text.toLowerCase().replaceAll(RegExp(r'[^a-zäöüßа-я0-9]+'), '-').replaceAll(RegExp(r'^-|-$'), '');
    final state = AppScope.of(context);
    final item = CatalogItem(
      id: 'custom-$slug-${DateTime.now().millisecondsSinceEpoch}',
      name: _name.text.trim(),
      kind: _kind,
      category: _category.text.trim(),
      summary: _summary.text.trim(),
      url: _url.text.trim(),
      access: _access,
      compatibility: [_compatibility],
      tags: _tags.text.split(',').map((value) => value.trim().toLowerCase()).where((value) => value.isNotEmpty).toList(),
      tip: _tip.text.trim(),
      install: _install.text.trim(),
      verifiedAt: DateTime.now().toIso8601String().split('T').first,
      source: 'user',
      sourceLanguage: state.language.code,
      localizedSummary: {state.language.code: _summary.text.trim()},
      localizedTip: _tip.text.trim().isEmpty ? const {} : {state.language.code: _tip.text.trim()},
      isCustom: true,
    );
    await state.addCustom(item);
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(state.s.t('entryAdded'))));
    for (final controller in [_name, _summary, _url, _category, _tags, _tip, _install]) {
      controller.clear();
    }
    widget.onSaved();
  }
}
