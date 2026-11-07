// Entry point: single instance, COM / Media Foundation init, then App::run().
#include "app/app.hpp"

#include <mfapi.h>
#include <objbase.h>
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmdLine, int) {
    const bool minimized = cmdLine && wcsstr(cmdLine, L"--minimized") != nullptr;

    // Second launch (e.g. from the Start menu) just brings the running instance up.
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\VibeHand.Singleton");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND w = FindWindowW(L"VibeHandMsg", nullptr)) {
            AllowSetForegroundWindow(ASFW_ANY);
            PostMessageW(w, vl::win::WM_VL_SHOWMAIN, 0, 0);
        }
        return 0;
    }

    // Lowest-impact defaults for a resident app; the chosen profile refines this.
    SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY);
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
    SetDllDirectoryW(L"");  // no DLL loading from the current directory

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    MFStartup(MF_VERSION, MFSTARTUP_LITE);

    int rc = 0;
    {
        vl::App app;
        rc = app.run(inst, minimized);
    }

    MFShutdown();
    CoUninitialize();
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return rc;
}
