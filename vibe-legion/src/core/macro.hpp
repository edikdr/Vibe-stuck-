// Macros (several steps in a row), the per-app rule filter and small UTF-8 text helpers.
// Portable and unit-tested; the Windows side only executes the parsed steps.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/settings.hpp"

namespace vl {

// Lower-cases ASCII and Russian letters of a UTF-8 string; the rest is kept.
std::string lowerUtf8(const std::string& s);
std::string trimCopy(const std::string& s);

// "Ctrl+Shift+M", "win + d", "F5", "PgDn" -> virtual-key codes, modifiers first.
bool parseKeyCombo(const std::string& text, std::vector<uint16_t>& keys);

struct MacroStep {
    enum class Kind { Keys, Wait, Text, Run, Builtin };
    Kind kind = Kind::Keys;
    std::vector<uint16_t> keys;              // Keys
    int ms = 0;                              // Wait
    std::string text;                        // Text: what to type; Run: command line
    ActionType action = ActionType::None;    // Builtin: any simple action by its id
};

// Steps are separated by ';' or a new line:
//   Ctrl+Shift+M            a key combination
//   wait 300  | пауза 300   pause, milliseconds (up to 10 s)
//   text Привет | текст …   types the text
//   run C:\x.ps1 | запуск … starts a program or script
//   media_next, mic_mute …  a built-in action by its id
// Returns false and the reason (Russian, names the step) when something is not understood.
constexpr size_t kMaxMacroSteps = 32;
bool parseMacro(const std::string& script, std::vector<MacroStep>& out, std::string* errorRu);

// GestureRule::apps: "discord.exe, chrome" — true when `exe` (file name of the foreground
// program) is in the list; an empty list matches everything. Case and ".exe" do not matter.
bool appListMatches(const std::string& list, const std::string& exe);

}  // namespace vl
