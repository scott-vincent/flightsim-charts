#pragma once
#include <windows.h>
#include "flightsim-charts.h"

struct ChartServerData
{
    double lat = 0.0;
    double lon = 0.0;
    int heading = 999;
    int headingMag = 0;
    int altitude = 0;
    int speed = 0;
    int flaps = 0;
    int trim = 0;
    int verticalSpeed = 0;
};

void chartServerInit();
void updateInstrumentHud(LocData* locData);
int chartServerSend();
void chartServerCleanup();
