// SafeHouse builds only (CANE_SAFEHOUSE): the on-screen touch pad's layout on iOS / Android (platform/touch_layout.h,
// the apps' TouchLayout), as Sly 2's pcinput.cpp sets it: the cinematic layout while a movie or a skippable cutscene
// plays (the pad hidden, holding the screen holds Circle: skipcut.cpp's hold), the menu layout (the D-pad and the menu
// keys, no sticks) on the title, the pause menu, the map and the prompts, the gameplay layout otherwise. The pad itself
// reaches the game as the last GLFW gamepad (the apps' GLFW layer), like any controller. Nothing on the desktop.
#include "../fmv.h"
#if defined(CANE_IOS) || defined(__ANDROID__)
#include <safehouse/mobile_app.h>
#include "skipcut.h"
#include "../prompt.h"
#include "../ui.h"
#endif

void ShUpdateTouchContext()
{
#if defined(CANE_IOS) || defined(__ANDROID__)
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
