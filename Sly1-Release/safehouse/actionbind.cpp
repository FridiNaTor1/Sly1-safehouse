// Advanced input mapping. See actionbind.h.
// (CANE_SAFEHOUSE builds only: the code uses SafeHouse's shared platform layer)
#if defined(CANE_SAFEHOUSE)
#include "actionbind.h"
#include "mouseinput.h"
#include "pcinput.h"
#include "../po.h"
#include "../sw.h"
#include <safehouse/controller_family.h>
#include <safehouse/platform.h>

namespace sly1actions
{
namespace input = cane::sly1input;

input::Profile profile = input::defaults();

void Load()
{
    profile = input::load(cane::userDirectory());
}

bool Save()
{
    return input::save(cane::userDirectory(), profile);
}

bool Active()
{
    return profile.enabled;
}

Context CurrentContext()
{
    if (g_psw == nullptr)
        return Context::Any;
    PO* ppo = PpoCur();
    if (ppo == nullptr)
        return Context::Any;
    const BASIC* pbasic = ppo;
    struct CIDCONTEXT { CID cid; Context context; };
    static constexpr CIDCONTEXT s_acidcontext[] =
    {
        { CID_TURRET, Context::Turret }, { CID_SUV, Context::Van }, { CID_TANK, Context::Tank },
        { CID_PUFFER, Context::Puffer }, { CID_JETPACK, Context::Jetpack }, { CID_RYTHM, Context::Rhythm },
        { CID_VAULT, Context::Vault },
    };
    for (const CIDCONTEXT& c : s_acidcontext)
        if (FIsBasicDerivedFrom(pbasic, c.cid))
            return c.context;
    return Context::Any;
}

namespace
{
bool FKeyHeld(GLFWwindow* window, int key)
{
    return key >= GLFW_KEY_SPACE && key <= GLFW_KEY_LAST && glfwGetKey(window, key) == GLFW_PRESS;
}

bool FPadHeld(const GLFWgamepadstate& pad, int binding)
{
    if (binding >= 0 && binding <= GLFW_GAMEPAD_BUTTON_LAST)
        return pad.buttons[binding] == GLFW_PRESS;
    if (binding == GAMEPAD_BINDING_LEFT_TRIGGER)
        return pad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > 0.25f;
    if (binding == GAMEPAD_BINDING_RIGHT_TRIGGER)
        return pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.25f;
    return false;
}
}

void HeldButtons(GLFWwindow* window, const GLFWgamepadstate* pad, bool fBlockMenuMouse, bool fRhythmKeys, JOY::BUTTONS& held)
{
    held.fill(false);
    const Context context = CurrentContext();
    // (the keyboard: the rhythm minigame's actions only while its keys are live; in its menus the common ones)
    const Context contextKeys = context == Context::Rhythm && !fRhythmKeys ? Context::Any : context;
    for (size_t i = 0; i < input::kActionCount; ++i)
    {
        const int key = profile.keyboard[i];
        bool fHeld = false;
        if (input::applies(profile, Action(i), contextKeys) &&
            !(fRhythmKeys && input::kActions[i].context != Context::Rhythm && FRhythmKey(key)))
        {
            if (FMouseBinding(key))
                fHeld = !(fBlockMenuMouse && (key == MouseBindingFromButton(GLFW_MOUSE_BUTTON_LEFT) ||
                                              key == MouseBindingFromButton(GLFW_MOUSE_BUTTON_RIGHT))) &&
                    FMouseBindingHeld(key);
            else
                fHeld = FKeyHeld(window, key);
        }
        if (pad != nullptr && input::applies(profile, Action(i), context))
            fHeld = fHeld || FPadHeld(*pad, profile.gamepad[i]);
        held[input::kActions[i].pad] = held[input::kActions[i].pad] || fHeld;
    }
}

bool RhythmKeysLive(bool fMenu)
{
    return !fMenu && CurrentContext() == Context::Rhythm;
}

int RhythmKey(int slot)
{
    if (slot < 0 || slot >= cane::settings::kSly1RhythmKeyCount)
        return GLFW_KEY_UNKNOWN;
    if (!Active())
        return g_rhythmKeyBindings[size_t(slot)];
    for (size_t i = 0; i < input::kActionCount; ++i)
        if (input::kActions[i].context == Context::Rhythm && input::kActions[i].pad == cane::settings::kSly1RhythmPads[slot])
            return profile.keyboard[i];
    return GLFW_KEY_UNKNOWN;
}

bool FRhythmKey(int key)
{
    if (key == GLFW_KEY_UNKNOWN)
        return false;
    for (int slot = 0; slot < cane::settings::kSly1RhythmKeyCount; ++slot)
        if (RhythmKey(slot) == key)
            return true;
    return false;
}

int BindingForButton(int button, bool controller)
{
    if (button < 0 || button >= BTN_MAX)
        return -1;
    // (the keyboard in the rhythm minigame: its own keys, whatever the mapping)
    const int slot = cane::settings::sly1RhythmSlot(button);
    if (!controller && slot >= 0 && CurrentContext() == Context::Rhythm)
        return RhythmKey(slot);
    if (Active())
    {
        const Context context = CurrentContext();
        for (size_t i = 0; i < input::kActionCount; ++i)
            if (input::kActions[i].pad == button && input::applies(profile, Action(i), context))
                return controller ? profile.gamepad[i] : profile.keyboard[i];
    }
    return controller ? g_gamepadBindings[button] : g_keyboardBindings[button];
}

int CaptureController()
{
    static bool s_fArmed = false;
    int found = -1;
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid)
    {
        GLFWgamepadstate pad{};
        if (!glfwJoystickPresent(jid) || !glfwJoystickIsGamepad(jid) || cane::input::ignoredSteamDuplicate(jid) ||
            !glfwGetGamepadState(jid, &pad))
            continue;
        for (int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST && found < 0; ++b)
            if (pad.buttons[b] == GLFW_PRESS && b != GLFW_GAMEPAD_BUTTON_GUIDE)
                found = b;
        if (found < 0 && pad.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > 0.25f)
            found = GAMEPAD_BINDING_LEFT_TRIGGER;
        if (found < 0 && pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.25f)
            found = GAMEPAD_BINDING_RIGHT_TRIGGER;
    }
    // (armed once nothing is held: the button that chose the row is not taken)
    if (found < 0)
    {
        s_fArmed = true;
        return -1;
    }
    if (!s_fArmed)
        return -1;
    s_fArmed = false;
    return found;
}
}
#endif
