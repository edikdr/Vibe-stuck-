// Executes gesture actions through SendInput / shell.
#pragma once

#include <string>
#include <vector>

#include "core/settings.hpp"

namespace vl {

// An entry of the Start menu ("All apps"): desktop programs and Store apps alike.
struct InstalledApp {
    std::wstring name;
    std::wstring path;  // "shell:AppsFolder\<AppUserModelID>" — ShellExecute opens it
};
// Enumerates shell:AppsFolder (UI thread, COM initialised). Sorted by name. ~0.1–0.5 s.
std::vector<InstalledApp> installedApps();

// Returns false if the action could not be performed (e.g. app path missing).
bool executeAction(const Action& a);

std::wstring actionTitleRu(ActionType t);
// One or two sentences: what exactly happens, the shortcut it stands for, where it is useful.
std::wstring actionDescriptionRu(ActionType t);
std::wstring actionGlyph(ActionType t);
std::wstring describeAction(const Action& a);  // "Ctrl + Shift + M", "Пауза / Play"...

}  // namespace vl
