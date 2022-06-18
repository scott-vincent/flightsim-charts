#pragma once
#include "flightsim-charts.h"

struct AircraftPosition {
    int x;
    int y;
    int labelX;
    int labelY;
    char text[256];
};

struct Intersect {
    int val;
    double dist;
};

void displayToChartPos(int x, int y, Position* pos);
void chartToDisplayPos(int x, int y, Position* pos);
void locationToChartPos(Location* loc, Position* pos);
void chartPosToLocation(int x, int y, Location* loc);
void locationToString(Location* loc, char* str);
double greatCircleDistance(Location* loc1, Location* loc2);
void aircraftLocToChartPos(AircraftPosition* pos);
bool drawOtherAircraft(Position* displayPos1, Position* displayPos2, Location* loc, Position* pos);
CalibratedData* findClosestChart(CalibratedData* calib, int count, Location* loc);
void adjustFollowLocation(LocData* loc, double ownWingSpan);
