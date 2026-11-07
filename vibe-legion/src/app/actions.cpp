#include "app/actions.hpp"

#include "platform/win/system.hpp"

#include <shellapi.h>

#include <vector>

namespace vl {

namespace {

bool isExtended(WORD vk) {
    switch (vk) {
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_HOME: case VK_END:
        case VK_PRIOR: case VK_NEXT: case VK_INSERT: case VK_DELETE: case VK_RMENU: case VK_RCONTROL:
        case VK_LWIN: case VK_RWIN: case VK_APPS: case VK_DIVIDE: case VK_NUMLOCK:
        case VK_MEDIA_PLAY_PAUSE: case VK_MEDIA_NEXT_TRACK: case VK_MEDIA_PREV_TRACK: case VK_MEDIA_STOP:
        case VK_VOLUME_UP: case VK_VOLUME_DOWN: case VK_VOLUME_MUTE:
            return true;
        default:
            return false;
    }
}

INPUT key(WORD vk, bool up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0) | (isExtended(vk) ? KEYEVENTF_EXTENDEDKEY : 0);
    return in;
}

bool combo(std::initializer_list<WORD> keys) {
    std::vector<INPUT> v;
    for (WORD k : keys) v.push_back(key(k, false));
    for (auto it = keys.end(); it != keys.begin();) v.push_back(key(*--it, true));
    return SendInput(static_cast<UINT>(v.size()), v.data(), sizeof(INPUT)) == v.size();
}

bool comboVec(const std::vector<uint16_t>& keys) {
    if (keys.empty()) return false;
    std::vector<INPUT> v;
    for (auto k : keys) v.push_back(key(k, false));
    for (auto it = keys.rbegin(); it != keys.rend(); ++it) v.push_back(key(*it, true));
    return SendInput(static_cast<UINT>(v.size()), v.data(), sizeof(INPUT)) == v.size();
}

bool wheel(int delta) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = static_cast<DWORD>(delta);
    return SendInput(1, &in, sizeof in) == 1;
}

}  // namespace

bool executeAction(const Action& a) {
    switch (a.type) {
        case ActionType::None: return true;
        case ActionType::MediaPlayPause: return combo({VK_MEDIA_PLAY_PAUSE});
        case ActionType::MediaNext: return combo({VK_MEDIA_NEXT_TRACK});
        case ActionType::MediaPrev: return combo({VK_MEDIA_PREV_TRACK});
        case ActionType::MediaStop: return combo({VK_MEDIA_STOP});
        case ActionType::VolumeUp: return combo({VK_VOLUME_UP});
        case ActionType::VolumeDown: return combo({VK_VOLUME_DOWN});
        case ActionType::VolumeMute: return combo({VK_VOLUME_MUTE});
        case ActionType::MicMute: return combo({VK_LWIN, VK_MENU, 'K'});
        case ActionType::KeyCombo: return comboVec(a.keys);
        case ActionType::ScrollUp: return wheel(WHEEL_DELTA);
        case ActionType::ScrollDown: return wheel(-WHEEL_DELTA);
        case ActionType::LaunchApp: {
            if (a.path.empty()) return false;
            const auto r = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", win::widen(a.path).c_str(),
                                                                   a.args.empty() ? nullptr : win::widen(a.args).c_str(),
                                                                   nullptr, SW_SHOWNORMAL));
            return r > 32;
        }
        case ActionType::LockPC: return LockWorkStation() != FALSE;
        case ActionType::ShowDesktop: return combo({VK_LWIN, 'D'});
        case ActionType::TaskView: return combo({VK_LWIN, VK_TAB});
        case ActionType::Screenshot: return combo({VK_LWIN, VK_SHIFT, 'S'});
        case ActionType::SwitchWindow: return combo({VK_MENU, VK_TAB});
        case ActionType::PauseRecognition: return true;  // handled by the app
        case ActionType::kCount: break;
    }
    return false;
}

std::wstring actionTitleRu(ActionType t) {
    switch (t) {
        case ActionType::None: return L"Ничего (только подсказка)";
        case ActionType::MediaPlayPause: return L"Пауза / воспроизведение";
        case ActionType::MediaNext: return L"Следующий трек";
        case ActionType::MediaPrev: return L"Предыдущий трек";
        case ActionType::MediaStop: return L"Стоп";
        case ActionType::VolumeUp: return L"Громче";
        case ActionType::VolumeDown: return L"Тише";
        case ActionType::VolumeMute: return L"Выключить звук";
        case ActionType::MicMute: return L"Микрофон вкл/выкл (Win+Alt+K)";
        case ActionType::KeyCombo: return L"Сочетание клавиш…";
        case ActionType::ScrollUp: return L"Прокрутка вверх";
        case ActionType::ScrollDown: return L"Прокрутка вниз";
        case ActionType::LaunchApp: return L"Запустить приложение…";
        case ActionType::LockPC: return L"Заблокировать ПК";
        case ActionType::ShowDesktop: return L"Показать рабочий стол";
        case ActionType::TaskView: return L"Представление задач";
        case ActionType::Screenshot: return L"Снимок экрана";
        case ActionType::SwitchWindow: return L"Переключить окно (Alt+Tab)";
        case ActionType::PauseRecognition: return L"Пауза распознавания";
        case ActionType::kCount: break;
    }
    return L"";
}

std::wstring actionGlyph(ActionType t) {
    switch (t) {
        case ActionType::MediaPlayPause: return L"";
        case ActionType::MediaNext: return L"";
        case ActionType::MediaPrev: return L"";
        case ActionType::MediaStop: return L"";
        case ActionType::VolumeUp: return L"";
        case ActionType::VolumeDown: return L"";
        case ActionType::VolumeMute: return L"";
        case ActionType::MicMute: return L"";
        case ActionType::KeyCombo: return L"";
        case ActionType::ScrollUp: return L"";
        case ActionType::ScrollDown: return L"";
        case ActionType::LaunchApp: return L"";
        case ActionType::LockPC: return L"";
        case ActionType::ShowDesktop: return L"";
        case ActionType::TaskView: return L"";
        case ActionType::Screenshot: return L"";
        case ActionType::SwitchWindow: return L"";
        case ActionType::PauseRecognition: return L"";
        default: return L"";
    }
}

std::wstring describeAction(const Action& a) {
    if (a.type == ActionType::KeyCombo) {
        std::wstring s;
        for (size_t i = 0; i < a.keys.size(); ++i) s += (i ? L" + " : L"") + win::vkName(a.keys[i]);
        return s.empty() ? L"Сочетание не задано" : s;
    }
    if (a.type == ActionType::LaunchApp) {
        if (a.path.empty()) return L"Приложение не выбрано";
        const auto p = std::filesystem::path(win::widen(a.path));
        return L"Запуск: " + p.filename().wstring();
    }
    return actionTitleRu(a.type);
}

}  // namespace vl
