// See pcinput.h.
// (CANE_SAFEHOUSE builds only: the code uses SafeHouse's shared platform layer)
#if defined(CANE_SAFEHOUSE)
#include "pcinput.h"
#include "../Input.h"
#include "mouseinput.h"
#include "../save.h"
#include "settings.h"
#include <safehouse/controller_family.h>
#include <safehouse/system_settings.h>

#include <algorithm>
#include <cstdlib>
#include <string>

int g_promptStyle = 0;
bool g_fAlwaysOriginalPsLayout = false;
bool g_fAutoCameraRotation = true;
void (*g_pfnPcInputSave)() = nullptr;

namespace st = cane::settings;

void PcInputFromSettings(const st::SystemSettings& s)
{
    for (int i = 0; i < st::kCameraKeyCount; ++i)
        g_cameraKeyBindings[i] = s.cameraKeyBindings[i];
    g_promptStyle = std::clamp(s.promptStyle, 0, cane::input::kPromptStyleLast);
    g_fAlwaysOriginalPsLayout = s.alwaysOriginalPsPrompts != 0;
    g_fAutoCameraRotation = s.autoCameraRotation != 0;
    g_fControllerVibration = s.controllerVibration != 0;
    g_mouseopt.aimSensitivity = std::clamp(s.mouseAimSensitivity, 10, 500);
    g_mouseopt.fInvertX = s.mouseInvertX != 0;
    g_mouseopt.fInvertY = s.mouseInvertY != 0;
    g_mouseopt.fRawInput = s.mouseRawInput != 0;
    g_mouseopt.fWheelZoom = s.mouseWheelZoom != 0;
    g_mouseopt.fMenus = s.mouseMenus != 0;
    ApplyMouseOptions();
    ApplyPcInputEnv();   // (the test switches stay over the file's values)
}

void PcInputToSettings(st::SystemSettings& s)
{
    for (int i = 0; i < st::kCameraKeyCount; ++i)
        s.cameraKeyBindings[i] = g_cameraKeyBindings[i];
    s.promptStyle = g_promptStyle;
    s.alwaysOriginalPsPrompts = g_fAlwaysOriginalPsLayout ? 1 : 0;
    s.autoCameraRotation = g_fAutoCameraRotation ? 1 : 0;
    s.controllerVibration = g_fControllerVibration ? 1 : 0;
    s.mouseAimSensitivity = g_mouseopt.aimSensitivity;
    s.mouseInvertX = g_mouseopt.fInvertX ? 1 : 0;
    s.mouseInvertY = g_mouseopt.fInvertY ? 1 : 0;
    s.mouseRawInput = g_mouseopt.fRawInput ? 1 : 0;
    s.mouseWheelZoom = g_mouseopt.fWheelZoom ? 1 : 0;
    s.mouseMenus = g_mouseopt.fMenus ? 1 : 0;
}

void PcInputSave()
{
    if (g_pfnPcInputSave != nullptr)
    {
        g_pfnPcInputSave();
        return;
    }
    // (Settings.ini, safehouse/settings.h: these options into the file's record, upstream's own through
    // SaveSystemSettings' VIDEOSAVESETTINGS)
    PcInputToSettings(ShSettingsMutable().system);
    SaveSystemSettings();
}

void ApplyPcInputEnv()
{
    const char* psz = std::getenv("SLY1_PCOPT");
    if (psz == nullptr)
        return;
    const std::string text(psz);
    size_t i = 0;
    while (i < text.size())
    {
        size_t j = text.find(',', i);
        if (j == std::string::npos)
            j = text.size();
        const std::string item = text.substr(i, j - i);
        const size_t eq = item.find('=');
        if (eq != std::string::npos)
        {
            const std::string k = item.substr(0, eq);
            const int n = std::atoi(item.c_str() + eq + 1);
            const int index = k.size() > 3 ? std::atoi(k.c_str() + 3) : -1;
            if (k == "promptstyle") g_promptStyle = std::clamp(n, 0, cane::input::kPromptStyleLast);
            else if (k == "alwaysps") g_fAlwaysOriginalPsLayout = n != 0;
            else if (k == "mousecamera") g_fMouseCamera = n != 0;
            else if (k == "autocam") g_fAutoCameraRotation = n != 0;
            else if (k == "menus") g_mouseopt.fMenus = n != 0;
            else if (k.rfind("key", 0) == 0 && index >= 0 && index < BTN_MAX) g_keyboardBindings[index] = n;
            else if (k.rfind("pad", 0) == 0 && index >= 0 && index < BTN_MAX) g_gamepadBindings[index] = n;
        }
        i = j + 1;
    }
}
#endif
