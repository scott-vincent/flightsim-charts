#pragma once
#include <windows.h>

void saveSettings();
void loadSettings();
void loadCalibrationData(ChartData* chartData, char* filename = NULL);
void saveCalibration(int x, int y, Location* loc);
bool fileSelectorDialog(HWND displayHwnd);
void launchOpenStreetMap();
void getClipboardLocation(Location* loc);
void findCalibratedCharts(CalibratedData* calib, int *count);
