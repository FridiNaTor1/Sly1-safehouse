// See settings.h.
#include "settings.h"
#include "pcinput.h"
#include "../save.h"
#include "../Input.h"
#include "../gl.h"
#include "../../platform/platform.h"
#include <safehouse/monitor_select.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
namespace st = cane::settings;

st::Sly1Settings s_model = st::sly1Defaults();
// The values last given to the game or taken from it (after its own validation); a save writes the fields that differ.
VIDEOSAVESETTINGS s_given{};
bool s_fGiven = false;
// The first save after a load: the game's validation of the given values (not the player's choice) becomes the baseline.
bool s_fBaselinePending = false;
bool s_fUnreadable = false;

std::vector<cane::monitor::Info> Monitors(std::vector<GLFWmonitor*>* handles = nullptr)
{
	std::vector<cane::monitor::Info> list;
	int count = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&count);
	GLFWmonitor* primary = glfwGetPrimaryMonitor();
	for (int i = 0; monitors != nullptr && i < count; ++i)
	{
		cane::monitor::Info m;
		if (const char* name = glfwGetMonitorName(monitors[i])) m.name = cane::monitor::trim(name);
		glfwGetMonitorPos(monitors[i], &m.x, &m.y);
		m.primary = monitors[i] == primary;
		list.push_back(m);
		if (handles) handles->push_back(monitors[i]);
	}
	return list;
}

// The bindings: SafeHouse writes mouse buttons as 2000 + button (system_settings.h), ProjectCane as -1000 - button.
int KeyToGame(int key)
{
	if (key >= st::kMouseBindingBase && key < st::kMouseBindingBase + st::kMouseButtonCount)
		return MouseBindingFromButton(key - st::kMouseBindingBase);
	if (st::mouseBinding(key)) return FMouseBinding(key) ? key : GLFW_KEY_UNKNOWN;   // (the wheel: Input.h's codes are SafeHouse's)
	return key;
}
int KeyFromGame(int key)
{
	if (FMouseBinding(key)) return st::kMouseBindingBase + MouseButtonFromBinding(key);
	return key;
}

VIDEOSAVESETTINGS ToGame(const st::Sly1Settings& s)
{
	const st::SystemSettings& y = s.system;
	VIDEOSAVESETTINGS v{};
	v.fogType = y.fogType;
	v.msaaEnabled = y.msaaEnabled;
	v.msaaSamples = y.msaaSamples;
	v.frameRate = y.frameRate;
	v.internalResolutionHeight = y.internalResolutionHeight;
	v.drawDistance = y.drawDistance;
	v.windowMode = y.windowMode;
	v.vsyncEnabled = y.vsyncEnabled;
	v.aspectMode = y.aspectMode;
	v.guiScale = y.guiScale;
	v.guiStyle = y.guiStyle;
	for (int i = 0; i < BTN_MAX && i < st::kBindingCount; ++i)
	{
		v.keyboardBindings[i] = KeyToGame(y.keyboardBindings[i]);
		v.gamepadBindings[i] = y.gamepadBindings[i];
	}
	v.disableControllerInputWhenUnfocused = y.disableControllerInputWhenUnfocused;
	v.lastSaveSlot = y.lastSaveSlot;
	v.stereoEnabled = y.stereoEnabled;
	v.musicVolume = y.musicVolume;
	v.soundEffectsVolume = y.soundEffectsVolume;
	v.dialogueVolume = y.dialogueVolume;
	v.mouseCameraEnabled = y.mouseCamera;
	v.cutsceneSkipControlsEnabled = s.sly1.dialogueSkip;
	v.mouseCameraSensitivity = static_cast<float>(y.mouseSensitivity) / 100.0f;
	v.autoIconify = s.sly1.autoIconify;
	v.anisotropicFiltering = y.anisotropy;
	v.outputGamma = s.sly1.gamma;
	v.textureFiltering = s.sly1.textureFiltering;
	// (the monitor by index: SafeHouse's identifier tells apart two monitors of the same name)
	const std::vector<cane::monitor::Info> monitors = Monitors();
	v.monitorIndex = (std::max)(0, cane::monitor::resolve(monitors, cane::monitor::load(y.monitor)));
	v.resolutionScalePercent = s.sly1.resolutionScale;
	v.controllerDeadzone = s.sly1.deadZone;
	std::snprintf(v.audioOutputDeviceId, sizeof(v.audioOutputDeviceId), "%s", s.sly1.audioDeviceId.c_str());
	v.monitorName[0] = '\0';
	return v;
}

// The fields of v that differ from was, into s.
void FromGame(st::Sly1Settings& s, const VIDEOSAVESETTINGS& v, const VIDEOSAVESETTINGS* was)
{
	st::SystemSettings& y = s.system;
#define SH_CHANGED(field) (was == nullptr || std::memcmp(&v.field, &was->field, sizeof(v.field)) != 0)
	if (SH_CHANGED(fogType)) y.fogType = v.fogType;
	if (SH_CHANGED(msaaEnabled) || SH_CHANGED(msaaSamples)) y.msaaEnabled = v.msaaEnabled, y.msaaSamples = v.msaaSamples;
	if (SH_CHANGED(frameRate)) y.frameRate = v.frameRate;
	if (SH_CHANGED(internalResolutionHeight)) y.internalResolutionHeight = v.internalResolutionHeight;
	if (SH_CHANGED(drawDistance)) y.drawDistance = v.drawDistance;
	if (SH_CHANGED(windowMode)) y.windowMode = v.windowMode;
	if (SH_CHANGED(vsyncEnabled)) y.vsyncEnabled = v.vsyncEnabled;
	if (SH_CHANGED(aspectMode)) y.aspectMode = v.aspectMode;
	if (SH_CHANGED(guiScale)) y.guiScale = v.guiScale;
	if (SH_CHANGED(guiStyle)) y.guiStyle = v.guiStyle;
	for (int i = 0; i < BTN_MAX && i < st::kBindingCount; ++i)
	{
		if (was == nullptr || v.keyboardBindings[i] != was->keyboardBindings[i]) y.keyboardBindings[i] = KeyFromGame(v.keyboardBindings[i]);
		if (was == nullptr || v.gamepadBindings[i] != was->gamepadBindings[i]) y.gamepadBindings[i] = v.gamepadBindings[i];
	}
	if (SH_CHANGED(disableControllerInputWhenUnfocused)) y.disableControllerInputWhenUnfocused = v.disableControllerInputWhenUnfocused;
	if (SH_CHANGED(lastSaveSlot)) y.lastSaveSlot = v.lastSaveSlot;
	if (SH_CHANGED(stereoEnabled)) y.stereoEnabled = v.stereoEnabled;
	if (SH_CHANGED(musicVolume)) y.musicVolume = v.musicVolume;
	if (SH_CHANGED(soundEffectsVolume)) y.soundEffectsVolume = v.soundEffectsVolume;
	if (SH_CHANGED(dialogueVolume)) y.dialogueVolume = v.dialogueVolume;
	if (SH_CHANGED(mouseCameraEnabled)) y.mouseCamera = v.mouseCameraEnabled;
	if (SH_CHANGED(cutsceneSkipControlsEnabled)) s.sly1.dialogueSkip = v.cutsceneSkipControlsEnabled;
	if (SH_CHANGED(mouseCameraSensitivity))
		y.mouseSensitivity = std::clamp(static_cast<int>(std::lround(v.mouseCameraSensitivity * 100.0f)), 10, 500);
	if (SH_CHANGED(autoIconify)) s.sly1.autoIconify = v.autoIconify;
	if (SH_CHANGED(anisotropicFiltering)) y.anisotropy = v.anisotropicFiltering;
	if (SH_CHANGED(outputGamma)) s.sly1.gamma = v.outputGamma;
	if (SH_CHANGED(textureFiltering)) s.sly1.textureFiltering = v.textureFiltering;
	if (SH_CHANGED(monitorIndex))
	{
		const std::vector<cane::monitor::Info> monitors = Monitors();
		const int index = v.monitorIndex;
		const bool primary = index >= 0 && index < static_cast<int>(monitors.size()) && monitors[static_cast<size_t>(index)].primary;
		cane::monitor::store(y.monitor, primary ? std::string() : cane::monitor::idOf(monitors, index));
	}
	if (SH_CHANGED(resolutionScalePercent)) s.sly1.resolutionScale = v.resolutionScalePercent;
	if (SH_CHANGED(controllerDeadzone)) s.sly1.deadZone = v.controllerDeadzone;
	if (SH_CHANGED(audioOutputDeviceId)) s.sly1.audioDeviceId = v.audioOutputDeviceId;
#undef SH_CHANGED
}
}

bool ShLoadSystemSettings(VIDEOSAVESETTINGS* settings)
{
	const auto userDir = cane::userDirectory();
	const st::Sly2Load result = st::loadSly1(userDir, s_model);
	s_fUnreadable = result == st::Sly2Load::Unreadable;
	std::printf("settings: %s (%s)\n", cane::utf8(st::sly1SettingsPath(userDir)).c_str(),
		result == st::Sly2Load::Ok ? "read" : result == st::Sly2Load::Migrated ? "made from Saves/system_settings.dat" :
		result == st::Sly2Load::Missing ? "new" : "unreadable: not written this run");
	PcInputFromSettings(s_model.system);   // (the input options beside VIDEOSAVESETTINGS: pcinput.h)
	if (result == st::Sly2Load::Missing)
		return false;
	*settings = ToGame(s_model);
	s_given = *settings;
	s_fGiven = true;
	s_fBaselinePending = true;
	return true;
}

bool ShSaveSystemSettings(const VIDEOSAVESETTINGS& settings)
{
	if (s_fUnreadable)
		return false;
	if (s_fBaselinePending)
	{
		// (the game's validation of the file's values: the file keeps them as they are)
		s_fBaselinePending = false;
		s_given = settings;
	}
	FromGame(s_model, settings, s_fGiven ? &s_given : nullptr);
	s_given = settings;
	s_fGiven = true;
	// (the file read again: keys this build does not know, written by hand or by a newer launcher, are kept)
	const auto userDir = cane::userDirectory();
	st::Sly1Settings onDisk;
	if (st::loadSly1(userDir, onDisk) == st::Sly2Load::Ok)
		s_model.unknown = onDisk.unknown;
	return st::saveSly1(userDir, s_model);
}

const cane::settings::Sly1Settings& ShSettings() { return s_model; }
cane::settings::Sly1Settings& ShSettingsMutable() { return s_model; }

int ShRendererSetting()
{
	st::Sly1Settings s = st::sly1Defaults();
	if (st::loadSly1(cane::userDirectory(), s) == st::Sly2Load::Missing)
		return 0;
	return (std::clamp)(s.system.renderer, 0, 2);
}
