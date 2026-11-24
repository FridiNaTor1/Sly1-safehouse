// SafeHouse builds only (CANE_SAFEHOUSE): the on-screen touch pad's layout on iOS / Android (platform/touch_layout.h,
// the apps' TouchLayout), as Sly 2's pcinput.cpp sets it: the cinematic layout while a movie or a skippable cutscene
// plays (the pad hidden, holding the screen is the skip: skipcut.cpp's hold, Input.cpp), the menu layout (the D-pad and the menu
// keys, no sticks) on the title, the pause menu, the map and the prompts, the gameplay layout otherwise. The pad itself
// reaches the game as the last GLFW gamepad (the apps' GLFW layer), like any controller. Nothing on the desktop.
#include "launcher.h"
#include "../fmv.h"
#if defined(CANE_IOS) || defined(__ANDROID__)
#include <safehouse/mobile_app.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include "skipcut.h"
#include "../prompt.h"
#include "../ui.h"
#endif

#if defined(CANE_IOS) || defined(__ANDROID__)
namespace
{
int s_touchVibration = 0;   // (the vibrator's strength while the on-screen pad rumbles, 0..255)
}
#endif

void ShTouchRumble(int joystickId, uint8_t highSpeedMotor, uint8_t lowSpeedMotor)
{
#if defined(CANE_IOS) || defined(__ANDROID__)
	// (the small motor is on / off: a light buzz, as Sly 2's rumble.cpp)
	if (joystickId == GLFW_JOYSTICK_LAST)
		s_touchVibration = highSpeedMotor ? std::max<int>(lowSpeedMotor, 96) : lowSpeedMotor;
	else
		s_touchVibration = 0;
#else
	(void)joystickId; (void)highSpeedMotor; (void)lowSpeedMotor;
#endif
}

void ShUpdateTouchContext()
{
#if defined(CANE_IOS) || defined(__ANDROID__)
	// (the vibrator holds a strength a quarter of a second after the last call: renewed every frame)
	if (s_touchVibration > 0)
		cane::mobile::vibrate(s_touchVibration);
	using cane::mobile::TouchContext;
	TouchContext context = TouchContext::Gameplay;
	if (FCutscenePlaying() || FShCutsceneActive())
		context = TouchContext::Cinematic;
	else if (g_prompt.fActive || g_prompt.blots != BLOTS_Hidden)
		context = TouchContext::Menu;
	else
		switch (g_ui.uis)
		{
		case UIS_Splash:
		case UIS_Attract:
		case UIS_Pausing:
		case UIS_Paused:
		case UIS_WorldMap:
			context = TouchContext::Menu;
			break;
		default:
			break;
		}
	cane::mobile::setTouchContext(context);
#endif
}
