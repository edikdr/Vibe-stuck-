// Executes gesture actions through SendInput / shell.
#pragma once

#include <string>

#include "core/settings.hpp"

namespace vl {

// Returns false if the action could not be performed (e.g. app path missing).
bool executeAction(const Action& a);

std::wstring actionTitleRu(ActionType t);
std::wstring actionGlyph(ActionType t);
std::wstring describeAction(const Action& a);  // "Ctrl + Shift + M", "Пауза / Play"...

}  // namespace vl
