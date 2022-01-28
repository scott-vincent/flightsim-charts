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
void locationToChartPos(double lat, double lon, Position* pos);
void chartPosToLocation(int x, int y, Location* loc);
void locationToString(Location* loc, char* str);
void aircraftLocToChartPos(AircraftPosition* pos);
bool drawOtherAircraft(Position* displayPos1, Position* displayPos2, LocData* loc, Position* pos);
