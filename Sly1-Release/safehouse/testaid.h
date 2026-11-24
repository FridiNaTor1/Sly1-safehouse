#pragma once
// SafeHouse builds only (CANE_SAFEHOUSE): more test-aid steps for SAFEHOUSE_DEBUGMENU (testaid.cpp lists them).
#include <string>

// debugmenu.cpp's script: a step it does not know itself; false when this does not know it either.
bool ShTestStep(const std::string& action, const std::string& arg);
// debugmenu.cpp, after each world update: the watched state's log lines.
void ShTestFrame();
