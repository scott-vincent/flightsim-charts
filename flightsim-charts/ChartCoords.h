#pragma once
#include "flightsim-charts.h"

struct AircraftPosition {
    int x;
    int y;
    int labelX;
    int labelY;
    char text[256];
};

void displayToChartPos(int x, int y, Position* pos);
void chartToDisplayPos(int x, int y, Position* pos);
void aircraftLocToChartPos(AircraftPosition* pos);
bool drawOtherAircraft(Position* displayPos1, Position* displayPos2, LocData* loc, Position* pos);
