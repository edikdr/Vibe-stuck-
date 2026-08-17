import 'package:flutter/widgets.dart';

enum AppLanguage { en, ru, de }

extension AppLanguageInfo on AppLanguage {
  String get code => name;
  Locale get locale => Locale(code);
  String get nativeName => switch (this) {
        AppLanguage.en => 'English',
        AppLanguage.ru => 'Русский',
        AppLanguage.de => 'Deutsch',
      };
}
