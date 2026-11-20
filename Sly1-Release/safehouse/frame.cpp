// See frame.h.
#include "frame.h"
#include "../clock.h"
#include "../gl.h"
#include <safehouse/frame_pacer.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <string>

namespace
{
cane::FramePacer& Pacer()
{
	static cane::FramePacer pacer;
	return pacer;
}

// The refresh rate of the monitor the window is on (0: unknown).
int RefreshRate(GLFWwindow* window)
{
	GLFWmonitor* monitor = glfwGetWindowMonitor(window);
	if (monitor == nullptr)
	{
		int x = 0, y = 0, w = 0, h = 0;
		glfwGetWindowPos(window, &x, &y);
		glfwGetWindowSize(window, &w, &h);
		int count = 0;
		GLFWmonitor** monitors = glfwGetMonitors(&count);
		for (int i = 0; monitors != nullptr && i < count && monitor == nullptr; ++i)
		{
			int mx = 0, my = 0;
			glfwGetMonitorPos(monitors[i], &mx, &my);
			const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
			if (mode != nullptr && x + w / 2 >= mx && x + w / 2 < mx + mode->width && y + h / 2 >= my && y + h / 2 < my + mode->height)
				monitor = monitors[i];
		}
		if (monitor == nullptr)
			monitor = glfwGetPrimaryMonitor();
	}
	const GLFWvidmode* mode = monitor != nullptr ? glfwGetVideoMode(monitor) : nullptr;
	return mode != nullptr ? mode->refreshRate : 0;
}

struct SWAP
{
	int interval = -1;
	bool fPaces = false;
	unsigned generation = 0;
	// the inputs last seen
	bool fVsync = false;
	int frameRate = -1;
	int refreshRate = -1;
	int windowMode = -1;
	long cframe = 0;
};
SWAP s_swap;

void UpdateSwap()
{
	GLFWwindow* window = g_gl.window;
	if (window == nullptr)
		return;
	// (the monitor's refresh rate is looked at twice a second at 60 FPS: a window moved to another monitor)
	const bool fLook = s_swap.frameRate < 0 || (s_swap.cframe++ % 30) == 0 || s_swap.fVsync != g_fVsync ||
		s_swap.frameRate != g_targetFrameRate || s_swap.windowMode != static_cast<int>(g_windowMode);
	if (!fLook)
		return;
	const int refreshRate = g_fVsync ? RefreshRate(window) : 0;
	if (s_swap.fVsync == g_fVsync && s_swap.frameRate == g_targetFrameRate && s_swap.refreshRate == refreshRate &&
		s_swap.windowMode == static_cast<int>(g_windowMode))
		return;
	s_swap.fVsync = g_fVsync;
	s_swap.frameRate = g_targetFrameRate;
	s_swap.refreshRate = refreshRate;
	s_swap.windowMode = static_cast<int>(g_windowMode);

	int interval = 0;
	bool fPaces = false;
	if (g_fVsync)
	{
		interval = 1;
		// (Windows reports 59.94 Hz modes as 59: the real refresh runs more than 1 % faster than a 59 cap, it never paces)
		const bool fTruncated = refreshRate < 100 && refreshRate % 10 == 9;
		if (refreshRate > 0 && g_targetFrameRate >= refreshRate && !fTruncated)
			fPaces = true;
		else if (refreshRate > 0 && g_targetFrameRate * 2 == refreshRate)
		{
			interval = 2;   // (Mesa on Wayland clamps it to 1: the pacer sees the swap not pacing and holds the cap itself)
			fPaces = true;
		}
	}
	cane::gl::SwapInterval(interval);   // (the seam: Vulkan too, gl.h)
	s_swap.interval = interval;
	s_swap.fPaces = fPaces;
	++s_swap.generation;
	std::printf("frame pacing: %s, VSync %s (swap interval %d, refresh %d Hz)%s\n",
		ShFrameRateUncapped(g_targetFrameRate) ? "uncapped" : (std::to_string(g_targetFrameRate) + " FPS").c_str(),
		g_fVsync ? "on" : "off", interval, refreshRate, fPaces ? ", the display paces" : "");
}
}

bool ShFrameRateUncapped(int frameRate) { return frameRate >= SH_FRAMERATE_UNCAPPED; }

void ShFrameEnd()
{
	UpdateSwap();
	// (Uncapped: a 1 ms frame at least, frame.h)
	const int frameRate = ShFrameRateUncapped(g_targetFrameRate) ? SH_FRAMERATE_UNCAPPED : (g_targetFrameRate < 30 ? 30 : g_targetFrameRate);
	Pacer().Wait(frameRate, s_swap.fPaces, s_swap.generation);
}

void ShFrameResync()
{
	Pacer().Resync();
}
