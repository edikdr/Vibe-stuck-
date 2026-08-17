import 'dart:io' show exit;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import 'app_state.dart';
import 'i18n/app_strings.dart';
import 'platform/platform_info.dart';
import 'screens/add_item_screen.dart';
import 'screens/catalog_screen.dart';
import 'screens/favorites_screen.dart';
import 'screens/settings_screen.dart';
import 'services/sync/sync_runner.dart';
import 'theme.dart';
import 'widgets/scroll_to_top.dart';

/// Flag used by the generated desktop system task (Task Scheduler on Windows,
/// a systemd user timer on Linux). It runs the same update the app runs, then
/// exits without ever creating a window.
const headlessFlag = '--headless-sync';

Future<void> main(List<String> args) async {
  WidgetsFlutterBinding.ensureInitialized();

  if (args.contains(headlessFlag)) {
    final result = await SyncRunner().run(force: args.contains('--force'));
    debugPrint('[atlas] headless sync: $result');
    exit(result.ran ? 0 : 2);
  }

  final state = AppState();
  await state.load();
  runApp(AtlasApp(state: state));
}

class AtlasApp extends StatelessWidget {
  const AtlasApp({required this.state, super.key});
  final AppState state;

  @override
  Widget build(BuildContext context) {
    return AppScope(
      state: state,
      child: Builder(
        builder: (context) {
          final current = AppScope.of(context);
          return MaterialApp(
            onGenerateTitle: (_) => current.s.t('appName'),
            debugShowCheckedModeBanner: false,
            theme: AtlasTheme.dark,
            locale: current.language.locale,
            supportedLocales: AppLanguage.values.map((value) => value.locale),
            localizationsDelegates: const [
              GlobalMaterialLocalizations.delegate,
              GlobalWidgetsLocalizations.delegate,
              GlobalCupertinoLocalizations.delegate,
            ],
            home: const AppShell(),
          );
        },
      ),
    );
  }
}

/// Desktop keyboard intents. Declared here so the shortcut table stays in one
/// place while individual screens only expose the actions.
class FocusSearchIntent extends Intent {
  const FocusSearchIntent();
}

class SyncNowIntent extends Intent {
  const SyncNowIntent();
}

class SectionIntent extends Intent {
  const SectionIntent(this.index);
  final int index;
}

class ClearSelectionIntent extends Intent {
  const ClearSelectionIntent();
}

/// Keyboard twin of the scroll hook, so the desktop build can reach the top of
/// a 562-entry list without touching the mouse.
class ScrollTopIntent extends Intent {
  const ScrollTopIntent();
}

/// Set by [CatalogScreen] so Ctrl+F can reach the search field from the shell.
final searchFocusNode = FocusNode(debugLabel: 'atlas-search');

class AppShell extends StatefulWidget {
  const AppShell({super.key});

  @override
  State<AppShell> createState() => _AppShellState();
}

class _AppShellState extends State<AppShell> with WidgetsBindingObserver {
  var _index = 0;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  /// Catch-up run: if the scheduled moment passed while the app was closed or
  /// backgrounded, the update happens as soon as it is visible again.
  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed) {
      AppScope.of(context).handleResume();
    }
  }

  void _goTo(int index) {
    if (index < 0 || index > 3) return;
    setState(() => _index = index);
  }

  @override
  Widget build(BuildContext context) {
    final state = AppScope.of(context);
    final s = state.s;
    final destinations = [
      NavigationDestination(icon: const Icon(Icons.explore_outlined), selectedIcon: const Icon(Icons.explore), label: s.t('navCatalog')),
      NavigationDestination(icon: const Icon(Icons.bookmark_border), selectedIcon: const Icon(Icons.bookmark), label: s.t('navFavorites')),
      NavigationDestination(icon: const Icon(Icons.add_box_outlined), selectedIcon: const Icon(Icons.add_box), label: s.t('navAdd')),
      NavigationDestination(icon: const Icon(Icons.tune_outlined), selectedIcon: const Icon(Icons.tune), label: s.t('navUpdates')),
    ];
    final pages = [
      const CatalogScreen(),
      const FavoritesScreen(),
      AddItemScreen(onSaved: () => _goTo(0)),
      const SettingsScreen(),
    ];

    final shell = LayoutBuilder(
      builder: (context, constraints) {
        if (constraints.maxWidth < Breakpoints.medium) {
          return Scaffold(
            body: IndexedStack(index: _index, children: pages),
            bottomNavigationBar: NavigationBar(
              selectedIndex: _index,
              onDestinationSelected: _goTo,
              destinations: destinations,
            ),
          );
        }
        final extended = constraints.maxWidth >= Breakpoints.expanded;
        return Scaffold(
          body: Row(
            children: [
              SafeArea(
                child: NavigationRail(
                  selectedIndex: _index,
                  onDestinationSelected: _goTo,
                  extended: extended,
                  leading: Padding(
                    padding: const EdgeInsets.fromLTRB(8, 20, 8, 28),
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        const _AtlasMark(),
                        if (extended) ...[
                          const SizedBox(width: 12),
                          const Text('VibeStack Atlas', style: TextStyle(fontWeight: FontWeight.w800, fontSize: 17)),
                        ],
                      ],
                    ),
                  ),
                  trailing: extended ? const _ShortcutHint() : null,
                  destinations: destinations
                      .map((item) => NavigationRailDestination(
                            icon: item.icon,
                            selectedIcon: item.selectedIcon,
                            label: Text(item.label),
                          ))
                      .toList(),
                ),
              ),
              const VerticalDivider(width: 1),
              Expanded(child: IndexedStack(index: _index, children: pages)),
            ],
          ),
        );
      },
    );

    if (!PlatformInfo.supportsKeyboardShortcuts) return shell;

    return Shortcuts(
      shortcuts: const <ShortcutActivator, Intent>{
        SingleActivator(LogicalKeyboardKey.keyF, control: true): FocusSearchIntent(),
        SingleActivator(LogicalKeyboardKey.keyF, meta: true): FocusSearchIntent(),
        SingleActivator(LogicalKeyboardKey.keyR, control: true): SyncNowIntent(),
        SingleActivator(LogicalKeyboardKey.keyR, meta: true): SyncNowIntent(),
        SingleActivator(LogicalKeyboardKey.digit1, control: true): SectionIntent(0),
        SingleActivator(LogicalKeyboardKey.digit2, control: true): SectionIntent(1),
        SingleActivator(LogicalKeyboardKey.digit3, control: true): SectionIntent(2),
        SingleActivator(LogicalKeyboardKey.digit4, control: true): SectionIntent(3),
        SingleActivator(LogicalKeyboardKey.escape): ClearSelectionIntent(),
        SingleActivator(LogicalKeyboardKey.home): ScrollTopIntent(),
        SingleActivator(LogicalKeyboardKey.home, control: true): ScrollTopIntent(),
        SingleActivator(LogicalKeyboardKey.home, meta: true): ScrollTopIntent(),
      },
      child: Actions(
        actions: <Type, Action<Intent>>{
          FocusSearchIntent: CallbackAction<FocusSearchIntent>(onInvoke: (_) {
            _goTo(0);
            searchFocusNode.requestFocus();
            return null;
          }),
          SyncNowIntent: CallbackAction<SyncNowIntent>(onInvoke: (_) {
            state.syncCatalog();
            return null;
          }),
          SectionIntent: CallbackAction<SectionIntent>(onInvoke: (intent) {
            _goTo(intent.index);
            return null;
          }),
          ClearSelectionIntent: CallbackAction<ClearSelectionIntent>(onInvoke: (_) {
            state.select(null);
            return null;
          }),
          ScrollTopIntent: CallbackAction<ScrollTopIntent>(onInvoke: (_) {
            ScrollTopHook.requestTop();
            return null;
          }),
        },
        child: Focus(autofocus: true, child: shell),
      ),
    );
  }
}

class _ShortcutHint extends StatelessWidget {
  const _ShortcutHint();

  @override
  Widget build(BuildContext context) {
    if (!PlatformInfo.supportsKeyboardShortcuts) return const SizedBox.shrink();
    final s = AppScope.of(context).s;
    return Expanded(
      child: Align(
        alignment: Alignment.bottomLeft,
        child: Padding(
          padding: const EdgeInsets.fromLTRB(16, 8, 8, 20),
          child: Text(
            '${s.t('shortcuts')}\nCtrl+F · Ctrl+R · Ctrl+1…4 · Home · Esc',
            style: const TextStyle(fontSize: 11, height: 1.5, color: AtlasTheme.textFaint),
          ),
        ),
      ),
    );
  }
}

class _AtlasMark extends StatelessWidget {
  const _AtlasMark();

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 40,
      height: 40,
      decoration: BoxDecoration(
        gradient: const LinearGradient(colors: [AtlasTheme.accent, AtlasTheme.info]),
        borderRadius: BorderRadius.circular(AtlasTheme.rControl),
        boxShadow: const [BoxShadow(color: Color(0x4D7D6BFF), blurRadius: 22)],
      ),
      child: const Icon(Icons.hub_rounded, color: Colors.white, size: 22),
    );
  }
}
