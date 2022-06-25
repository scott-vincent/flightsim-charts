#pragma once
#include <windows.h>

void saveSettings();
void loadSettings();
void loadCalibrationData(ChartData* chartData, char* filename = NULL);
void saveCalibration(int x, int y, Locn* loc);
bool fileSelectorDialog(HWND displayHwnd);
void getClipboardLocation(Locn* loc);
void findCalibratedCharts(CalibratedData* calib, int *count);
