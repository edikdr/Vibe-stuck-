import 'package:flutter/material.dart';

/// The single source of colour, radius and elevation for the app.
///
/// Everything visual is named by role, not by hue: widgets ask for
/// [AtlasTheme.textFaint] or [AtlasTheme.line], never for a hex literal. That
/// is what made the 0.5.0 darkening a one-file change, and it is why a future
/// light theme only has to redefine this class.
class AtlasTheme {
  const AtlasTheme._();

  // ---- ground ------------------------------------------------------------
  // Blue-biased near-blacks. A neutral grey here reads as unfinished, and the
  // 0.4.0 panels sat too close to the page: the steps below are deliberate.
  static const ink = Color(0xFF04060B);
  static const panel = Color(0xFF090E17);
  static const panelRaised = Color(0xFF0E1420);
  static const panelSunken = Color(0xFF06080F);

  // ---- separation --------------------------------------------------------
  static const line = Color(0xFF161D2A);
  static const lineStrong = Color(0xFF212A3A);

  // ---- text ladder -------------------------------------------------------
  static const textPrimary = Color(0xFFE8ECF5);
  static const textSecondary = Color(0xFF9CA8C0);
  static const textMuted = Color(0xFF7E8BA4);
  static const textFaint = Color(0xFF66748F);
  static const textGhost = Color(0xFF333D50);

  // ---- accent and signals ------------------------------------------------
  // One accent, spent on selection and identity only. The three signal
  // colours mean something specific and are never used decoratively.
  static const accent = Color(0xFF7D6BFF);
  static const info = Color(0xFF3ED6C5);
  static const positive = Color(0xFF6FDF9B);
  static const warning = Color(0xFFE9A95C);

  // ---- shape -------------------------------------------------------------
  static const rCard = 14.0;
  static const rControl = 12.0;
  static const rChip = 8.0;
  static const rTag = 6.0;
  static const rSheet = 18.0;

  static ThemeData get dark {
    final scheme = ColorScheme.fromSeed(
      seedColor: accent,
      brightness: Brightness.dark,
      surface: panel,
      primary: accent,
      onSurface: textPrimary,
    );
    return ThemeData(
      colorScheme: scheme,
      scaffoldBackgroundColor: ink,
      canvasColor: ink,
      useMaterial3: true,
      dividerTheme: const DividerThemeData(color: line, thickness: 1, space: 1),
      textTheme: const TextTheme(
        displaySmall: TextStyle(fontWeight: FontWeight.w800, letterSpacing: -1.2),
        headlineSmall: TextStyle(fontWeight: FontWeight.w800, letterSpacing: -0.4),
        titleLarge: TextStyle(fontWeight: FontWeight.w700, letterSpacing: -0.2),
        titleMedium: TextStyle(fontWeight: FontWeight.w600),
        bodyLarge: TextStyle(height: 1.45),
        bodyMedium: TextStyle(height: 1.4),
      ),
      cardTheme: CardThemeData(
        color: panel,
        elevation: 0,
        margin: EdgeInsets.zero,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(rCard),
          side: const BorderSide(color: line),
        ),
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: panel,
        hintStyle: const TextStyle(color: textFaint),
        border: _inputBorder(line),
        enabledBorder: _inputBorder(line),
        focusedBorder: _inputBorder(accent),
        contentPadding: const EdgeInsets.symmetric(horizontal: 15, vertical: 14),
      ),
      chipTheme: ChipThemeData(
        backgroundColor: panelRaised,
        selectedColor: const Color(0x267D6BFF),
        side: const BorderSide(color: line),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(rChip)),
        labelStyle: const TextStyle(fontWeight: FontWeight.w600, fontSize: 12.5),
        showCheckmark: false,
        padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 4),
      ),
      navigationBarTheme: NavigationBarThemeData(
        backgroundColor: panel.withValues(alpha: 0.96),
        surfaceTintColor: Colors.transparent,
        indicatorColor: const Color(0x267D6BFF),
        indicatorShape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(rChip)),
        elevation: 0,
      ),
      navigationRailTheme: NavigationRailThemeData(
        backgroundColor: panel,
        indicatorColor: const Color(0x267D6BFF),
        indicatorShape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(rChip)),
      ),
      popupMenuTheme: PopupMenuThemeData(
        color: panelRaised,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(rControl),
          side: const BorderSide(color: line),
        ),
      ),
      tooltipTheme: TooltipThemeData(
        decoration: BoxDecoration(
          color: panelRaised,
          borderRadius: BorderRadius.circular(rTag),
          border: Border.all(color: lineStrong),
        ),
        textStyle: const TextStyle(fontSize: 12, color: textSecondary),
      ),
      snackBarTheme: SnackBarThemeData(
        backgroundColor: panelRaised,
        contentTextStyle: const TextStyle(color: textPrimary),
        behavior: SnackBarBehavior.floating,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(rControl),
          side: const BorderSide(color: lineStrong),
        ),
      ),
      bottomSheetTheme: const BottomSheetThemeData(
        backgroundColor: panel,
        surfaceTintColor: Colors.transparent,
      ),
    );
  }

  static OutlineInputBorder _inputBorder(Color color) => OutlineInputBorder(
        borderRadius: BorderRadius.circular(rControl),
        borderSide: BorderSide(color: color),
      );
}
