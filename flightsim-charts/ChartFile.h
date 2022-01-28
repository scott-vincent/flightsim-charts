#pragma once
#include <windows.h>

void saveProgramData();
void loadProgramData();
void loadCalibration();
void saveCalibration(int x, int y, Location* loc);
void fileSelectorDialog(HWND displayHwnd);
void launchOpenStreetMap();
void getClipboardLocation(Location* loc);
