// SafeHouse builds only (CANE_SAFEHOUSE): the on-screen touch pad's layout on iOS / Android (platform/touch_layout.h,
// the apps' TouchLayout), as Sly 2's pcinput.cpp sets it: the cinematic layout while a movie plays (the pad hidden,
// holding the screen skips: skipcut.cpp's hold), the gameplay layout otherwise. The pad itself reaches the game as the
// last GLFW gamepad (the apps' GLFW layer), like any controller. Nothing on the desktop.
#include "../fmv.h"
#if defined(CANE_IOS) || defined(__ANDROID__)
#include <safehouse/mobile_app.h>
#endif

void ShUpdateTouchContext()
{
#if defined(CANE_IOS) || defined(__ANDROID__)
	cane::mobile::setTouchContext(FCutscenePlaying() ? cane::mobile::TouchContext::Cinematic
		: cane::mobile::TouchContext::Gameplay);
#endif
}
