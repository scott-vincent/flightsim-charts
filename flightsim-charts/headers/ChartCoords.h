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
void locationToChartPos(Locn* loc, Position* pos);
void chartPosToLocation(int x, int y, Locn* loc);
void locationToString(Locn* loc, char* str);
double greatCircleDistance(Locn* loc1, Locn* loc2);
void aircraftLocToChartPos(AircraftPosition* pos);
bool drawOther(Position* displayPos1, Position* displayPos2, Locn* loc, Position* pos, bool force = false);
CalibratedData* findClosestChart(CalibratedData* calib, int count, Locn* loc);
void adjustFollowLocation(LocData* loc, double ownWingSpan);
void findTrackExtremities(FlightPlanData start, FlightPlanData end, Position* line1Start, Position* line1End, Position* line2Start, Position* line2End);
