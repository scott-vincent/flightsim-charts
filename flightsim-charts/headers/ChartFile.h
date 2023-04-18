#pragma once
#include <windows.h>

void saveSettings();
void loadSettings();
void loadCalibrationData(ChartData* chartData, char* filename = NULL);
void saveCalibration(int x, int y, Locn* loc);
char *fileSelectorDialog(HWND displayHwnd, const char* filter);
void getClipboardLocation(Locn* loc);
void convertTextReadableLocation(char* text, Locn* loc);
void findCalibratedCharts(CalibratedData* calib, int *count);
