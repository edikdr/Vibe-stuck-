import 'package:flutter/material.dart';

import '../theme.dart';

/// The grab hook: one control that both reports how far down a long list you
/// are and pulls you back to the top.
///
/// Wrap any scrollable and pass it the same [ScrollController]:
///
/// ```dart
/// ScrollTopHook(
///   controller: _controller,
///   child: CustomScrollView(controller: _controller, slivers: [...]),
/// )
/// ```
///
/// It owns the only scroll listener on that view, so nothing else in the tree
/// has to watch offsets. The ring is the scroll position, which is why the
/// button doubles as a progress readout instead of needing a separate bar.
///
/// After a jump it offers one way back down to where you were, then withdraws
/// the offer — a stale "back down" button is worse than no button at all.
class ScrollTopHook extends StatefulWidget {
  const ScrollTopHook({
    required this.controller,
    required this.child,
    this.threshold = 320,
    this.bottomInset = 0,
    this.topLabel = 'Back to top',
    this.backLabel = 'Back down',
    super.key,
  });

  final ScrollController controller;
  final Widget child;

  /// Offset in pixels below which the hook stays hidden.
  final double threshold;

  /// Extra bottom padding, so the hook clears a bottom navigation bar.
  final double bottomInset;

  final String topLabel;
  final String backLabel;

  /// The hook currently mounted and visible, if any.
  ///
  /// Exposed so a keyboard shortcut declared in the app shell can reach it
  /// without the shell knowing which screen is on top — the same approach the
  /// app already uses for `searchFocusNode`.
  static _ScrollTopHookState? _active;

  /// Pulls the active scrollable to the top. Returns false when there is
  /// nothing scrolled to pull. Safe to call from anywhere.
  static bool requestTop() {
    final active = _active;
    if (active == null || !active.mounted) return false;
    return active._toTop();
  }

  @override
  State<ScrollTopHook> createState() => _ScrollTopHookState();
}

class _ScrollTopHookState extends State<ScrollTopHook> {
  /// Rounded percent, so a one-pixel scroll cannot trigger a rebuild the ring
  /// could not render anyway.
  int _percent = 0;
  bool _visible = false;

  /// Offset stashed by the last jump, restored by "back down". Null means the
  /// offer is not on the table.
  double? _mark;

  @override
  void initState() {
    super.initState();
    widget.controller.addListener(_onScroll);
    ScrollTopHook._active = this;
  }

  @override
  void didUpdateWidget(ScrollTopHook oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.controller != widget.controller) {
      oldWidget.controller.removeListener(_onScroll);
      widget.controller.addListener(_onScroll);
    }
  }

  @override
  void dispose() {
    widget.controller.removeListener(_onScroll);
    if (ScrollTopHook._active == this) ScrollTopHook._active = null;
    super.dispose();
  }

  void _onScroll() {
    if (!widget.controller.hasClients) return;
    final position = widget.controller.position;
    final max = position.maxScrollExtent;
    final offset = position.pixels.clamp(0.0, max <= 0 ? 0.0 : max);
    final percent = max <= 0 ? 0 : (offset / max * 100).round();
    final visible = offset > widget.threshold;

    // A manual scroll back to the top means the reader no longer needs the
    // return offer, so retire it rather than leaving it to time out.
    final mark = offset <= widget.threshold ? null : _mark;

    if (percent == _percent && visible == _visible && mark == _mark) return;
    setState(() {
      _percent = percent;
      _visible = visible;
      _mark = mark;
    });
  }

  /// Animation is skipped entirely for very short hops and whenever the reader
  /// has asked the platform to reduce motion.
  Duration _durationFor(double distance) {
    if (!mounted || MediaQuery.of(context).disableAnimations) return Duration.zero;
    if (distance < 160) return Duration.zero;
    return Duration(milliseconds: (distance / 2.6).clamp(240, 620).round());
  }

  void _jumpTo(double target) {
    if (!widget.controller.hasClients) return;
    final distance = (widget.controller.position.pixels - target).abs();
    final duration = _durationFor(distance);
    if (duration == Duration.zero) {
      widget.controller.jumpTo(target);
    } else {
      widget.controller.animateTo(target, duration: duration, curve: Curves.easeOutCubic);
    }
  }

  bool _toTop() {
    if (!widget.controller.hasClients) return false;
    final from = widget.controller.position.pixels;
    if (from <= widget.threshold) return false;
    setState(() => _mark = from);
    _jumpTo(0);
    return true;
  }

  void _back() {
    final mark = _mark;
    if (mark == null) return;
    setState(() => _mark = null);
    _jumpTo(mark);
  }

  @override
  Widget build(BuildContext context) {
    final showResume = _mark != null;
    return Stack(
      children: [
        widget.child,
        Positioned(
          right: 14,
          bottom: 14 + widget.bottomInset,
          child: IgnorePointer(
            ignoring: !(_visible || showResume),
            child: AnimatedSlide(
              duration: const Duration(milliseconds: 220),
              curve: Curves.easeOutCubic,
              offset: (_visible || showResume) ? Offset.zero : const Offset(0, 0.4),
              child: AnimatedOpacity(
                duration: const Duration(milliseconds: 220),
                opacity: (_visible || showResume) ? 1 : 0,
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.end,
                  children: [
                    if (showResume) ...[
                      _ResumePill(label: widget.backLabel, onTap: _back),
                      const SizedBox(height: 8),
                    ],
                    _Dial(percent: _percent, label: widget.topLabel, onTap: _toTop),
                  ],
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }
}

/// The button itself: a progress ring you can grab.
class _Dial extends StatelessWidget {
  const _Dial({required this.percent, required this.label, required this.onTap});

  final int percent;
  final String label;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Tooltip(
      message: '$label · $percent%',
      child: Material(
        color: AtlasTheme.panelRaised.withValues(alpha: 0.94),
        shape: const CircleBorder(side: BorderSide(color: AtlasTheme.lineStrong)),
        clipBehavior: Clip.antiAlias,
        child: InkWell(
          onTap: onTap,
          customBorder: const CircleBorder(),
          child: SizedBox(
            width: 50,
            height: 50,
            child: Stack(
              alignment: Alignment.center,
              children: [
                SizedBox(
                  width: 42,
                  height: 42,
                  // Animated so the ring sweeps to the new position instead of
                  // snapping when a filter change shortens the list.
                  child: TweenAnimationBuilder<double>(
                    tween: Tween<double>(end: percent / 100),
                    duration: const Duration(milliseconds: 180),
                    curve: Curves.easeOut,
                    builder: (context, value, _) => CircularProgressIndicator(
                      value: value,
                      strokeWidth: 2.2,
                      backgroundColor: AtlasTheme.line,
                      valueColor: const AlwaysStoppedAnimation(AtlasTheme.accent),
                    ),
                  ),
                ),
                const Icon(Icons.arrow_upward_rounded, size: 19, color: AtlasTheme.textPrimary),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _ResumePill extends StatelessWidget {
  const _ResumePill({required this.label, required this.onTap});

  final String label;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return Material(
      color: AtlasTheme.panelRaised.withValues(alpha: 0.94),
      borderRadius: BorderRadius.circular(999),
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(999),
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(999),
            border: Border.all(color: AtlasTheme.lineStrong),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Icon(Icons.arrow_downward_rounded, size: 14, color: AtlasTheme.textMuted),
              const SizedBox(width: 6),
              Text(
                label,
                style: const TextStyle(
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                  color: AtlasTheme.textSecondary,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
