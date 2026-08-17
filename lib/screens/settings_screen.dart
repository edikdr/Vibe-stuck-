import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../app_state.dart';
import '../i18n/app_language.dart';
import '../platform/platform_info.dart';
import '../services/showcase_resolver.dart';
import '../services/sync/background_worker.dart';
import '../theme.dart';

/// Updates, images, language and data transfer.
///
/// The daily update section is the honest version of "auto update": it shows
/// which of the three layers is actually running on this platform instead of
/// promising background work the OS may never grant.
class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  final _feedController = TextEditingController();
  int _cachedImages = 0;
  bool _primed = false;

  /// [AppScope] is an inherited widget, so it may only be read once the element
  /// has dependencies — initState is too early and asserts in debug builds.
  /// Everything that needs app state is primed here instead, exactly once.
  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (_primed) return;
    _primed = true;
    _feedController.text = AppScope.of(context).feedUrl;
    _refreshCacheCount();
  }

  @override
  void dispose() {
    _feedController.dispose();
    super.dispose();
  }

  Future<void> _refreshCacheCount() async {
    final count = await AppScope.of(context).imageCache.cachedCount();
    if (mounted) setState(() => _cachedImages = count);
  }

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final s = state.s;

    return Scaffold(
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.fromLTRB(16, 18, 16, 40),
          children: [
            Text(s.t('updatesTitle'),
                style: const TextStyle(fontSize: 24, fontWeight: FontWeight.w800)),
            const SizedBox(height: 6),
            Text(s.t('updatesSubtitle'),
                style: const TextStyle(fontSize: 13, height: 1.45, color: AtlasTheme.textMuted)),
            const SizedBox(height: 20),

            // ---------------------------------------------------- daily update
            _Card(
              children: [
                SwitchListTile(
                  contentPadding: EdgeInsets.zero,
                  value: state.autoSync,
                  onChanged: state.updateAutoSync,
                  title: Text(s.t('dailyUpdate'),
                      style: const TextStyle(fontWeight: FontWeight.w700)),
                  subtitle: Text(s.t('dailyUpdateHint'),
                      style: const TextStyle(fontSize: 12.5, height: 1.4)),
                ),
                if (state.autoSync) ...[
                  const Divider(height: 22),
                  Row(
                    children: [
                      const Icon(Icons.schedule_rounded, size: 18, color: AtlasTheme.info),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Text(s.t('updateHour'),
                            style: const TextStyle(fontSize: 13.5, fontWeight: FontWeight.w600)),
                      ),
                      OutlinedButton(
                        onPressed: () async {
                          final picked = await showTimePicker(
                            context: context,
                            initialTime: TimeOfDay(hour: state.syncHour, minute: state.syncMinute),
                          );
                          if (picked != null) {
                            await state.setSyncTime(picked.hour, picked.minute);
                          }
                        },
                        child: Text(_hhmm(state.syncHour, state.syncMinute)),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  _Line(
                    label: s.t('nextUpdate'),
                    value: _dateTime(state.nextScheduledSync),
                  ),
                  Padding(
                    padding: const EdgeInsets.only(bottom: 6),
                    child: Text(
                      s.t('lastSync', {
                        'date': state.lastSync == null ? s.t('never') : _dateTime(state.lastSync!),
                      }),
                      style: const TextStyle(fontSize: 12.5, color: AtlasTheme.textFaint),
                    ),
                  ),
                  const SizedBox(height: 12),
                  _BackgroundStatus(mode: state.backgroundMode),
                  if (state.backgroundResult.isNotEmpty) ...[
                    const SizedBox(height: 10),
                    Text('${s.t('backgroundDone')}: ${state.backgroundResult}',
                        style: const TextStyle(fontSize: 12, color: AtlasTheme.textFaint)),
                  ],
                ],
                const SizedBox(height: 14),
                Row(
                  children: [
                    Expanded(
                      child: FilledButton.icon(
                        onPressed: state.syncing ? null : () => state.syncCatalog(),
                        icon: state.syncing
                            ? const SizedBox(
                                width: 16,
                                height: 16,
                                child: CircularProgressIndicator(strokeWidth: 2),
                              )
                            : const Icon(Icons.sync_rounded, size: 18),
                        label: Text(state.syncing ? s.t('checking') : s.t('checkNow')),
                      ),
                    ),
                  ],
                ),
                if (state.syncMessage != null) ...[
                  const SizedBox(height: 10),
                  Text(state.syncMessage!,
                      style: const TextStyle(fontSize: 12.5, color: AtlasTheme.positive)),
                ],
                for (final error in state.syncErrors) ...[
                  const SizedBox(height: 6),
                  Text('· $error',
                      style: const TextStyle(fontSize: 12, color: AtlasTheme.warning)),
                ],
              ],
            ),
            const SizedBox(height: 16),

            // -------------------------------------------------- update sources
            _Section(title: s.t('automaticSources')),
            _Card(
              children: [
                _Toggle(
                  title: s.t('officialMcp'),
                  hint: s.t('officialMcpHint'),
                  value: state.syncMcp,
                  onChanged: state.updateSyncMcp,
                ),
                _Toggle(
                  title: s.t('skillRepos'),
                  hint: s.t('skillReposHint'),
                  value: state.syncSkills,
                  onChanged: state.updateSyncSkills,
                ),
                _Toggle(
                  title: s.t('packageVersions'),
                  hint: s.t('packageVersionsHint'),
                  value: state.syncVersions,
                  onChanged: state.updateSyncVersions,
                ),
                _Toggle(
                  title: s.t('apiLinks'),
                  hint: s.t('apiLinksHint'),
                  value: state.syncLinks,
                  onChanged: state.updateSyncLinks,
                ),
                const Divider(height: 22),
                Text(s.t('customFeed'),
                    style: const TextStyle(fontSize: 13.5, fontWeight: FontWeight.w600)),
                const SizedBox(height: 4),
                Text(s.t('customFeedHint'),
                    style: const TextStyle(fontSize: 12, height: 1.4, color: AtlasTheme.textFaint)),
                const SizedBox(height: 10),
                TextField(
                  controller: _feedController,
                  decoration: const InputDecoration(hintText: 'https://example.com/catalog.json'),
                  onSubmitted: state.updateFeedUrl,
                ),
              ],
            ),
            const SizedBox(height: 16),

            // -------------------------------------------------------- images
            _Section(title: s.t('imagesSection')),
            _Card(
              children: [
                Text(s.t('imagesHint'),
                    style: const TextStyle(fontSize: 12.5, height: 1.45, color: AtlasTheme.textMuted)),
                const SizedBox(height: 14),
                Text(s.t('imageProvider'),
                    style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
                const SizedBox(height: 8),
                Wrap(
                  spacing: 8,
                  children: [
                    for (final provider in PreviewProvider.values)
                      ChoiceChip(
                        label: Text(provider == PreviewProvider.off
                            ? s.t('providerOff')
                            : provider.label),
                        selected: state.previewProvider == provider,
                        onSelected: (_) => state.setPreviewProvider(provider),
                      ),
                  ],
                ),
                const SizedBox(height: 14),
                Row(
                  children: [
                    Expanded(
                      child: Text(
                        s.t('cachedImages', {'count': _cachedImages}),
                        style: const TextStyle(fontSize: 12.5, color: AtlasTheme.textFaint),
                      ),
                    ),
                    TextButton.icon(
                      onPressed: () async {
                        await state.clearImageCache();
                        await _refreshCacheCount();
                        if (context.mounted) {
                          ScaffoldMessenger.of(context)
                            ..hideCurrentSnackBar()
                            ..showSnackBar(SnackBar(content: Text(s.t('imageCacheCleared'))));
                        }
                      },
                      icon: const Icon(Icons.cleaning_services_outlined, size: 16),
                      label: Text(s.t('clearImageCache')),
                    ),
                  ],
                ),
              ],
            ),
            const SizedBox(height: 16),

            // ------------------------------------------------------ language
            _Section(title: s.t('language')),
            _Card(
              children: [
                Text(s.t('languageHint'),
                    style: const TextStyle(fontSize: 12.5, height: 1.45, color: AtlasTheme.textMuted)),
                const SizedBox(height: 12),
                Wrap(
                  spacing: 8,
                  children: [
                    for (final language in AppLanguage.values)
                      ChoiceChip(
                        label: Text(language.nativeName),
                        selected: state.language == language,
                        onSelected: (_) => state.setLanguage(language),
                      ),
                  ],
                ),
              ],
            ),
            const SizedBox(height: 16),

            // ---------------------------------------------------- update log
            _Section(title: s.t('updateLog')),
            _Card(
              children: [
                if (state.events.isEmpty)
                  Text(s.t('updateLogEmpty'),
                      style: const TextStyle(fontSize: 12.5, color: AtlasTheme.textFaint))
                else ...[
                  for (final event in state.events.take(24))
                    Padding(
                      padding: const EdgeInsets.only(bottom: 8),
                      child: Row(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text('·  ', style: TextStyle(color: AtlasTheme.info)),
                          Expanded(
                            child: Text(event.message,
                                style: const TextStyle(fontSize: 12.5, height: 1.4)),
                          ),
                          Text(_time(event.createdAt),
                              style: const TextStyle(fontSize: 11, color: AtlasTheme.textFaint)),
                        ],
                      ),
                    ),
                  const SizedBox(height: 4),
                  Align(
                    alignment: Alignment.centerRight,
                    child: TextButton(
                      onPressed: state.clearEvents,
                      child: Text(s.t('clearLog')),
                    ),
                  ),
                ],
              ],
            ),
            const SizedBox(height: 16),

            // --------------------------------------------------- data export
            _Section(title: s.t('dataTransfer')),
            _Card(
              children: [
                Text(s.t('dataTransferHint'),
                    style: const TextStyle(fontSize: 12.5, height: 1.45, color: AtlasTheme.textMuted)),
                const SizedBox(height: 12),
                Row(
                  children: [
                    Expanded(
                      child: OutlinedButton.icon(
                        onPressed: () async {
                          await Clipboard.setData(ClipboardData(text: state.createBackup()));
                          if (context.mounted) {
                            ScaffoldMessenger.of(context)
                              ..hideCurrentSnackBar()
                              ..showSnackBar(SnackBar(content: Text(s.t('exported'))));
                          }
                        },
                        icon: const Icon(Icons.upload_rounded, size: 18),
                        label: Text(s.t('export')),
                      ),
                    ),
                    const SizedBox(width: 10),
                    Expanded(
                      child: OutlinedButton.icon(
                        onPressed: () async {
                          final data = await Clipboard.getData(Clipboard.kTextPlain);
                          final raw = data?.text ?? '';
                          if (raw.isEmpty) return;
                          try {
                            await state.importBackup(raw);
                            if (context.mounted) {
                              ScaffoldMessenger.of(context)
                                ..hideCurrentSnackBar()
                                ..showSnackBar(SnackBar(content: Text(s.t('imported'))));
                            }
                          } catch (_) {
                            if (context.mounted) {
                              ScaffoldMessenger.of(context)
                                ..hideCurrentSnackBar()
                                ..showSnackBar(SnackBar(content: Text(s.t('invalidBackup'))));
                            }
                          }
                        },
                        icon: const Icon(Icons.download_rounded, size: 18),
                        label: Text(s.t('import')),
                      ),
                    ),
                  ],
                ),
              ],
            ),
            const SizedBox(height: 20),
            Center(
              child: Text('VibeStack Atlas · ${PlatformInfo.label}',
                  style: const TextStyle(fontSize: 11.5, color: AtlasTheme.textGhost)),
            ),
          ],
        ),
      ),
    );
  }
}

class _BackgroundStatus extends StatelessWidget {
  const _BackgroundStatus({required this.mode});
  final BackgroundMode mode;

  @override
  Widget build(BuildContext context) {
    final s = AppScope.of(context).s;
    final (text, color, icon) = switch (mode) {
      BackgroundMode.systemScheduler => (s.t('bgActive'), AtlasTheme.positive, Icons.verified_rounded),
      BackgroundMode.inApp => (s.t('bgInApp'), AtlasTheme.info, Icons.timer_outlined),
      BackgroundMode.disabled => (s.t('bgUnavailable'), AtlasTheme.textFaint, Icons.pause_circle_outline),
    };

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
          decoration: BoxDecoration(
            color: color.withValues(alpha: 0.10),
            borderRadius: BorderRadius.circular(AtlasTheme.rChip),
            border: Border.all(color: color.withValues(alpha: 0.32)),
          ),
          child: Row(
            children: [
              Icon(icon, size: 16, color: color),
              const SizedBox(width: 9),
              Expanded(
                child: Text('${s.t('backgroundStatus')}: $text',
                    style: TextStyle(fontSize: 12.5, fontWeight: FontWeight.w600, color: color)),
              ),
            ],
          ),
        ),
        if (PlatformInfo.isDesktop) ...[
          const SizedBox(height: 8),
          Text(s.t('bgDesktopHint'),
              style: const TextStyle(fontSize: 11.5, height: 1.45, color: AtlasTheme.textFaint)),
        ],
      ],
    );
  }
}

class _Toggle extends StatelessWidget {
  const _Toggle({
    required this.title,
    required this.hint,
    required this.value,
    required this.onChanged,
  });

  final String title;
  final String hint;
  final bool value;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) {
    return SwitchListTile(
      contentPadding: EdgeInsets.zero,
      value: value,
      onChanged: onChanged,
      title: Text(title, style: const TextStyle(fontSize: 13.5, fontWeight: FontWeight.w600)),
      subtitle: Text(hint, style: const TextStyle(fontSize: 12, height: 1.35)),
    );
  }
}

class _Card extends StatelessWidget {
  const _Card({required this.children});
  final List<Widget> children;

  /// A [Material] rather than a decorated [Container]: the rows inside are
  /// ListTiles, and they paint their ink splash onto the nearest Material. With
  /// a plain Container in between, the panel colour hid every tap ripple.
  @override
  Widget build(BuildContext context) {
    return Material(
      color: AtlasTheme.panel,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(AtlasTheme.rCard),
        side: const BorderSide(color: AtlasTheme.line),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 14, 16, 16),
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: children),
      ),
    );
  }
}

class _Section extends StatelessWidget {
  const _Section({required this.title});
  final String title;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(4, 0, 4, 10),
      child: Text(title.toUpperCase(),
          style: const TextStyle(
              fontSize: 11.5, fontWeight: FontWeight.w800, letterSpacing: 1.1, color: AtlasTheme.textMuted)),
    );
  }
}

class _Line extends StatelessWidget {
  const _Line({required this.label, required this.value});
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Row(
        children: [
          Expanded(
            child: Text(label, style: const TextStyle(fontSize: 12.5, color: AtlasTheme.textFaint)),
          ),
          Text(value, style: const TextStyle(fontSize: 12.5, fontWeight: FontWeight.w600)),
        ],
      ),
    );
  }
}

String _two(int value) => value.toString().padLeft(2, '0');
String _hhmm(int hour, int minute) => '${_two(hour)}:${_two(minute)}';
String _time(DateTime value) => '${_two(value.hour)}:${_two(value.minute)}';
String _dateTime(DateTime value) =>
    '${_two(value.day)}.${_two(value.month)} ${_two(value.hour)}:${_two(value.minute)}';
