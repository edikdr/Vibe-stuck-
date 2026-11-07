// The settings window: navigation pane on the left, pages on the right.
#pragma once

#include <string>
#include <unordered_map>

#include "ui/window_base.hpp"

namespace vl::ui {

enum Page { PageOverview, PageGestures, PageFaces, PageCamera, PagePower, PageSystem, kPageCount };

// Editing state that must survive between frames.
struct UiState {
    int page = PageOverview;
    std::string expandedRule;
    std::wstring newPersonName;
    std::string enrollingPerson;
    std::wstring newGestureName;
    std::string recordingGesture;
    float calibCm = 60.f;
    std::unordered_map<std::string, std::wstring> text;  // text buffers by key
    std::string confirmDelete;                            // id awaiting a second click
    double confirmUntil = 0;
};

class MainWindow final : public GuiWindow {
public:
    using GuiWindow::GuiWindow;
    void show(int page);
    UiState& state() { return state_; }

protected:
    void frame(Gui& g, const RectF& client) override;
    LRESULT extra(UINT msg, WPARAM wp, LPARAM lp, bool& handled) override;
    int backdropType() const override;
    void onDestroyed() override;

private:
    UiState state_;
};

// Page renderers (pages.cpp). `area` is the content area right of the nav pane.
void drawOverview(Gui& g, App& app, UiState& st, const RectF& area);
void drawGestures(Gui& g, App& app, UiState& st, const RectF& area);
void drawFaces(Gui& g, App& app, UiState& st, const RectF& area);
void drawCamera(Gui& g, App& app, UiState& st, const RectF& area);
void drawPower(Gui& g, App& app, UiState& st, const RectF& area);
void drawSystem(Gui& g, App& app, UiState& st, const RectF& area);

// Shared bits used by the flyout too.
void drawPreview(Gui& g, App& app, const RectF& r, bool overlays);
Color modeColor(const Palette& p, int mode);

}  // namespace vl::ui
