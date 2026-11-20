// The mouse: camera, aim, bindings and menus. See mouseinput.h.
// (CANE_SAFEHOUSE builds only: the code uses SafeHouse's shared platform layer)
#if defined(CANE_SAFEHOUSE)
#include "mouseinput.h"
#include "../Input.h"
#include "../debug.h"
#include "../sw.h"
#include "../gl.h"
#include "../cm.h"
#include "../ui.h"
#include "../prompt.h"
#include "../wm.h"
#include "../mouse.h"
#include "../content.h"
#include "imgui/imgui.h"
#include <safehouse/controller_family.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

MOUSEOPTIONS g_mouseopt;

bool FMouseCameraJoy(const JOY* pjoy)
{
	return pjoy == &g_joy && FMouseCaptured();
}

namespace
{
constexpr int kMouseButtonCount = 8;
// Radians of turn per mouse count at 100%.
constexpr float kRadPerCountOrbit = 0.0030f;
constexpr float kRadPerCountAim = 0.0012f;

bool s_fCaptured = false;
bool s_fHaveLast = false;
double s_xLast = 0.0, s_yLast = 0.0;
float s_dxCounts = 0.0f, s_dyCounts = 0.0f; // + = right / down
int s_wheel = 0;                            // + = up
bool s_afButton[kMouseButtonCount] = {};
bool s_afButtonPrev[kMouseButtonCount] = {};
bool s_afButtonBlocked[kMouseButtonCount] = {};
bool s_fImGuiMouse = false;
bool s_fMenuCursorValid = false;
float s_xMenu = 0.0f, s_yMenu = 0.0f;
float s_xMenuLast = -1.0f, s_yMenuLast = -1.0f;
bool s_fMenuMoved = false;
float s_travel = 0.0f;

bool FWheelBound()
{
	return std::any_of(g_keyboardBindings.begin(), g_keyboardBindings.end(), [](int key) {
		return key == MOUSE_BINDING_WHEEL_UP || key == MOUSE_BINDING_WHEEL_DOWN; });
}

bool FWantCapture(GLFWwindow* window)
{
	if (!g_fMouseCamera || g_fDebugGuiOpen)
		return false;
	if (glfwGetWindowAttrib(window, GLFW_FOCUSED) != GLFW_TRUE ||
		glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE)
		return false;
	if (g_psw == nullptr || g_ui.uis != UIS_Playing || g_prompt.blots != BLOTS_Hidden || g_wmc.fActive)
		return false;
	// The debug free camera drives the cursor itself.
	if (g_pcm != nullptr && g_pcm->ccpr > 0 && g_pcm->acpr[0].cpp == CPP_ManualOverride)
		return false;
	return true;
}

void SetCapture(GLFWwindow* window, bool fCapture)
{
	if (fCapture)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		if (glfwRawMouseMotionSupported())
			glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, g_mouseopt.fRawInput ? GLFW_TRUE : GLFW_FALSE);
	}
	else
	{
		if (glfwRawMouseMotionSupported())
			glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	s_fCaptured = fCapture;
	s_fHaveLast = false;
	MOUSE::ResetDelta();
}

void NoteMouse()
{
	cane::input::tracker().noteMouse();
}
} // namespace

void MouseFocusChanged()
{
	// A click used to activate the window must not reach gameplay, menus or binding capture.
	std::fill_n(s_afButtonBlocked, kMouseButtonCount, true);
	s_fHaveLast = false;
	s_dxCounts = s_dyCounts = 0.0f;
	s_wheel = 0;
	(void)MOUSE::GetScrollDX();
	(void)MOUSE::GetScrollDY();
}

void UpdateMouseInput(GLFWwindow* window)
{
	s_dxCounts = s_dyCounts = 0.0f;
	s_wheel = 0;
	s_fMenuMoved = false;
	s_fMenuCursorValid = false;
	if (window == nullptr)
		return;

	// Wheel notches since the previous update (mouse.cpp accumulates them).
	const double wheel = MOUSE::GetScrollDY();
	s_wheel = static_cast<int>(std::lround(wheel));
	if (s_wheel == 0 && wheel != 0.0)
		s_wheel = wheel > 0.0 ? 1 : -1;

	// ImGui (the debug windows) owns the mouse while it hovers one of them.
	s_fImGuiMouse = ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse && !s_fCaptured;
	const bool fFocused = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
	bool fClicked = false;
	for (int button = 0; button < kMouseButtonCount; ++button)
	{
		s_afButtonPrev[button] = s_afButton[button];
		const bool fHeld = glfwGetMouseButton(window, button) == GLFW_PRESS;
		if (!fHeld)
			s_afButtonBlocked[button] = false;
		s_afButton[button] = fFocused && !s_fImGuiMouse && fHeld && !s_afButtonBlocked[button];
		fClicked |= s_afButton[button] && !s_afButtonPrev[button];
	}
	if (fClicked || (s_wheel != 0 && fFocused))
		NoteMouse();

	// (the binocucom and the menus switch the cursor mode themselves too: a capture they undid is redone)
	const bool fWant = FWantCapture(window);
	if (fWant != s_fCaptured || (fWant && glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED))
		SetCapture(window, fWant);

	double x = 0.0, y = 0.0;
	glfwGetCursorPos(window, &x, &y);
	if (s_fCaptured)
	{
		if (s_fHaveLast)
		{
			const double dx = x - s_xLast, dy = y - s_yLast;
			// Ignore the jump GLFW can report right after the capture changes.
			if (std::abs(dx) < 4000.0 && std::abs(dy) < 4000.0)
			{
				s_dxCounts = static_cast<float>(dx);
				s_dyCounts = static_cast<float>(dy);
			}
		}
		s_xLast = x;
		s_yLast = y;
		s_fHaveLast = true;
		// A deliberate swipe (not a trackpad brush) makes the prompts show keys.
		s_travel = s_travel * 0.9f + std::abs(s_dxCounts) + std::abs(s_dyCounts);
		if (s_travel > 60.0f)
		{
			NoteMouse();
			s_travel = 0.0f;
		}
		return;
	}

	// The free cursor: window coordinates to framebuffer pixels inside the presented picture.
	int windowW = 0, windowH = 0, fbW = 0, fbH = 0;
	glfwGetWindowSize(window, &windowW, &windowH);
	glfwGetFramebufferSize(window, &fbW, &fbH);
	if (!fFocused || windowW <= 0 || windowH <= 0 || fbW <= 0 || fbH <= 0)
		return;
	const float fbX = static_cast<float>(x) * static_cast<float>(fbW) / static_cast<float>(windowW);
	const float fbY = static_cast<float>(y) * static_cast<float>(fbH) / static_cast<float>(windowH);
	s_xMenu = fbX - static_cast<float>(g_gl.presentX);
	s_yMenu = fbY - static_cast<float>(g_gl.presentY);
	s_fMenuCursorValid = !s_fImGuiMouse;
	if (std::abs(s_xMenu - s_xMenuLast) >= 1.0f || std::abs(s_yMenu - s_yMenuLast) >= 1.0f)
	{
		s_fMenuMoved = s_xMenuLast >= 0.0f; // (the first sample is not a move)
		s_xMenuLast = s_xMenu;
		s_yMenuLast = s_yMenu;
		if (s_fMenuMoved && g_mouseopt.fMenus && g_prompt.blots != BLOTS_Hidden)
			NoteMouse();
	}
}

void ApplyMouseOptions()
{
	if (s_fCaptured && g_gl.window != nullptr && glfwRawMouseMotionSupported())
		glfwSetInputMode(g_gl.window, GLFW_RAW_MOUSE_MOTION, g_mouseopt.fRawInput ? GLFW_TRUE : GLFW_FALSE);
}

bool FMouseBindingHeld(int binding)
{
	if (binding == MOUSE_BINDING_WHEEL_UP)
		return s_wheel > 0;
	if (binding == MOUSE_BINDING_WHEEL_DOWN)
		return s_wheel < 0;
	const int button = MouseButtonFromBinding(binding);
	return FMouseBinding(binding) && button >= 0 && button < kMouseButtonCount && s_afButton[button];
}

int MouseBindingPressed(GLFWwindow* window)
{
	if (window == nullptr || glfwGetWindowAttrib(window, GLFW_FOCUSED) != GLFW_TRUE)
		return -1;
	for (int button = 0; button < kMouseButtonCount; ++button)
		if (s_afButton[button] && !s_afButtonPrev[button])
			return MouseBindingFromButton(button);
	if (s_wheel > 0)
		return MOUSE_BINDING_WHEEL_UP;
	if (s_wheel < 0)
		return MOUSE_BINDING_WHEEL_DOWN;
	return -1;
}

void ConsumeMouseMenuClicks()
{
	for (int button : { GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT })
		if (s_afButton[button])
		{
			s_afButtonBlocked[button] = true;
			s_afButton[button] = false;
		}
}

bool FMouseCaptured() { return s_fCaptured; }

float MouseCameraOrbit()
{
	if (!s_fCaptured)
		return 0.0f;
	return s_dxCounts * kRadPerCountOrbit * g_mouseCameraSensitivity * (g_mouseopt.fInvertX ? -1.0f : 1.0f);
}

void MouseAimDelta(float* pdxRight, float* pdyUp)
{
	*pdxRight = *pdyUp = 0.0f;
	if (!s_fCaptured)
		return;
	const float scale = kRadPerCountAim * (static_cast<float>(g_mouseopt.aimSensitivity) / 100.0f);
	*pdxRight = s_dxCounts * scale * (g_mouseopt.fInvertX ? -1.0f : 1.0f);
	*pdyUp = -s_dyCounts * scale * (g_mouseopt.fInvertY ? -1.0f : 1.0f);
}

float MouseWheelZoom()
{
	if (!s_fCaptured || !g_mouseopt.fWheelZoom || FWheelBound())
		return 0.0f;
	return static_cast<float>(s_wheel);
}

static bool FMouseMenuEnabled() { return g_mouseopt.fMenus && !s_fCaptured && !s_fImGuiMouse; }

bool MouseMenuCursor(float* px, float* py)
{
	if (!FMouseMenuEnabled() || !s_fMenuCursorValid)
		return false;
	*px = s_xMenu;
	*py = s_yMenu;
	return true;
}
bool FMouseMenuMoved() { return FMouseMenuEnabled() && s_fMenuMoved; }
bool FMouseLeftPressed()
{
	return FMouseMenuEnabled() && s_afButton[GLFW_MOUSE_BUTTON_LEFT] && !s_afButtonPrev[GLFW_MOUSE_BUTTON_LEFT];
}
bool FMouseRightPressed()
{
	return FMouseMenuEnabled() && s_afButton[GLFW_MOUSE_BUTTON_RIGHT] && !s_afButtonPrev[GLFW_MOUSE_BUTTON_RIGHT];
}
int MouseWheelNotches() { return FMouseMenuEnabled() ? s_wheel : 0; }

const char* PchzMouseBindingName(int binding)
{
	switch (binding)
	{
	case MOUSE_BINDING_WHEEL_UP: return ContentUiText("Wheel Up", "Molette haut", "Mausrad hoch", "Rueda arriba", "Rotella su");
	case MOUSE_BINDING_WHEEL_DOWN: return ContentUiText("Wheel Down", "Molette bas", "Mausrad runter", "Rueda abajo", "Rotella giù");
	default: break;
	}
	switch (MouseButtonFromBinding(binding))
	{
	case GLFW_MOUSE_BUTTON_LEFT: return ContentUiText("Mouse Left", "Souris gauche", "Maus links", "Ratón izquierdo", "Mouse sinistro");
	case GLFW_MOUSE_BUTTON_RIGHT: return ContentUiText("Mouse Right", "Souris droite", "Maus rechts", "Ratón derecho", "Mouse destro");
	case GLFW_MOUSE_BUTTON_MIDDLE: return ContentUiText("Mouse Middle", "Souris milieu", "Maus Mitte", "Ratón central", "Mouse centrale");
	case GLFW_MOUSE_BUTTON_4: return ContentUiText("Mouse 4", "Souris 4", "Maus 4", "Ratón 4", "Mouse 4");
	case GLFW_MOUSE_BUTTON_5: return ContentUiText("Mouse 5", "Souris 5", "Maus 5", "Ratón 5", "Mouse 5");
	case GLFW_MOUSE_BUTTON_6: return ContentUiText("Mouse 6", "Souris 6", "Maus 6", "Ratón 6", "Mouse 6");
	case GLFW_MOUSE_BUTTON_7: return ContentUiText("Mouse 7", "Souris 7", "Maus 7", "Ratón 7", "Mouse 7");
	case GLFW_MOUSE_BUTTON_8: return ContentUiText("Mouse 8", "Souris 8", "Maus 8", "Ratón 8", "Mouse 8");
	default: return ContentUiText("Mouse Button", "Bouton de souris", "Maustaste", "Botón del ratón", "Pulsante mouse");
	}
}
#endif
