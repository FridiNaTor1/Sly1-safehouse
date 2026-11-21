// See hudbox.h.
#include "hudbox.h"
#include "settings.h"
#include "../blot.h"
#include "../tv.h"

int ShHudPlacement()
{
	return (std::clamp)(ShSettings().hudPlacement, 0, SH_HUDPLACEMENT_Max - 1);
}

void ShSetHudPlacement(int placement)
{
	ShSettingsMutable().hudPlacement = (std::clamp)(placement, 0, SH_HUDPLACEMENT_Max - 1);
	ResizeTvUiForFramebuffer();
	RepositionAllBlots();
}
