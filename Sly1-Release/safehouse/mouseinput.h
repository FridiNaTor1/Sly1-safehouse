#pragma once
// The mouse (CANE_SAFEHOUSE builds): camera, aim, bindings and menus.
//
//   Camera     While playing (no menu, map or debug window open) the cursor is captured; movement orbits
//              the third-person camera (g_fMouseCamera, g_mouseCameraSensitivity) and aims the binocucom
//              and the first-person look.
//   Bindings   Mouse buttons 1-8 and the wheel are keyboard bindings (Input.h MouseBindingFromButton,
//              MOUSE_BINDING_WHEEL_UP / _DOWN).
//   Menus      Hover highlights a menu row, a left click chooses it, a right click goes back and the wheel
//              moves the highlight.

struct GLFWwindow;
struct JOY;

struct MOUSEOPTIONS
{
	int aimSensitivity = 100;    // percent: the binocucom and the first-person look
	bool fInvertX = false;
	bool fInvertY = false;
	bool fRawInput = true;
	bool fWheelZoom = true;      // the wheel zooms the binocucom when no action is bound to it
	bool fMenus = true;
};
extern MOUSEOPTIONS g_mouseopt;

// Once per JOY::Update of the game's own pad: button states, the motion since the previous call and the
// cursor capture.
void UpdateMouseInput(GLFWwindow* window);
// Focus lost or gained: the click that activated the window is ignored until its release.
void MouseFocusChanged();
// Re-applies raw motion after the option changes.
void ApplyMouseOptions();

// Bindings (Input.h's mouse binding codes).
bool FMouseBindingHeld(int binding);
// Binding capture: the first newly pressed mouse button or this update's wheel, or -1.
int MouseBindingPressed(GLFWwindow* window);
const char* PchzMouseBindingName(int binding);

// The camera, for this update's motion (0 while the cursor is free).
bool FMouseCaptured();
// The pad a camera reads is the game's own (g_joy) and the mouse is captured.
bool FMouseCameraJoy(const JOY* pjoy);
float MouseCameraOrbit();                          // radians, + = like the right stick to the right
void MouseAimDelta(float* pdxRight, float* pdyUp); // radians
float MouseWheelZoom();                            // notches, + = zoom in

// Menus. Coordinates are framebuffer pixels inside the presented picture (blot space).
bool MouseMenuCursor(float* px, float* py);
bool FMouseMenuMoved();
bool FMouseLeftPressed();
bool FMouseRightPressed();
int MouseWheelNotches();
// A menu took this update's left / right click: the button stays out of the game's bindings until released.
void ConsumeMouseMenuClicks();

