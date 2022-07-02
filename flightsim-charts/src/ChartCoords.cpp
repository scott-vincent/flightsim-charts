#include <windows.h>
#include <iostream>
#define _USE_MATH_DEFINES
#include <math.h>
#include "ChartCoords.h"

const double RadiusOfEarthNm = 3440.1;

// Externals
extern double DegreesToRadians;
extern int _displayWidth;
extern int _displayHeight;
extern DrawData _chart;
extern DrawData _view;
extern LocData _aircraftData;
extern MouseData _mouseData;
extern ChartData _chartData;

/// <summary>
/// Get position on zoomed chart of display's top left corner
/// </summary>
void getDisplayPos(Position* display)
{
    display->x = _mouseData.dragX + _chart.x * _view.scale - _displayWidth / 2;
    display->y = _mouseData.dragY + _chart.y * _view.scale - _displayHeight / 2;
}

/// <summary>
/// Convert display position to chart position
/// </summary>
void displayToChartPos(int displayX, int displayY, Position* chart)
{
    Position displayPos;
    getDisplayPos(&displayPos);

    int zoomedX = displayPos.x + displayX;
    int zoomedY = displayPos.y + displayY;

    chart->x = zoomedX / _view.scale;
    chart->y = zoomedY / _view.scale;
}

/// <summary>
/// Convert chart position to display position.
/// Returned position may be outside the display area.
/// </summary>
void chartToDisplayPos(int chartX, int chartY, Position* display)
{
    Position displayPos;
    getDisplayPos(&displayPos);

    display->x = chartX * _view.scale - displayPos.x;
    display->y = chartY * _view.scale - displayPos.y;
}

/// <summary>
/// Convert location to chart position. Chart must be calibrated.
/// </summary>
void locationToChartPos(Locn* loc, Position* pos)
{
    double lonCalibDiff = _chartData.lon[1] - _chartData.lon[0];
    double lonDiff = loc->lon - _chartData.lon[0];
    double xScale = lonDiff / lonCalibDiff;

    double latCalibDiff = _chartData.lat[1] - _chartData.lat[0];
    double latDiff = loc->lat - _chartData.lat[0];
    double yScale = latDiff / latCalibDiff;

    int xCalibDiff = _chartData.x[1] - _chartData.x[0];
    int yCalibDiff = _chartData.y[1] - _chartData.y[0];

    pos->x = _chartData.x[0] + xCalibDiff * xScale;
    pos->y = _chartData.y[0] + yCalibDiff * yScale;
}

/// <summary>
/// Convert chart position to location. Chart must be calibrated.
/// </summary>
void chartPosToLocation(int x, int y, Locn* loc)
{
    int xCalibDiff = _chartData.x[1] - _chartData.x[0];
    double xDiff = x - _chartData.x[0];
    double lonScale = xDiff / xCalibDiff;

    int yCalibDiff = _chartData.y[1] - _chartData.y[0];
    double yDiff = y - _chartData.y[0];
    double latScale = yDiff / yCalibDiff;

    double latCalibDiff = _chartData.lat[1] - _chartData.lat[0];
    double lonCalibDiff = _chartData.lon[1] - _chartData.lon[0];

    loc->lat = _chartData.lat[0] + latCalibDiff * latScale;
    loc->lon = _chartData.lon[0] + lonCalibDiff * lonScale;
}

/// <summary>
/// Returns formatted co-ordinate, e.g. 51° 28' 29.60"
/// </summary>
void coordToString(double coord, char* str)
{
    int degs = coord;
    double secs = (coord - degs) * 3600.00;
    int mins = secs / 60.0;
    secs -= mins * 60;

    sprintf(str, "%d%s %d' %.2f\"", degs, DegreesSymbol, mins, secs + 0.005);
}

/// <summary>
/// Returns formatted location, e.g. 51° 28' 29.60" N 0° 29' 5.19" W
/// </summary>
void locationToString(Locn* loc, char* str)
{
    char latStr[32];
    char lonStr[32];
    char latDirn = 'N';
    char lonDirn = 'E';

    coordToString(abs(loc->lat), latStr);
    coordToString(abs(loc->lon), lonStr);

    if (loc->lat < 0) {
        latDirn = 'S';
    }

    if (loc->lon < 0) {
        lonDirn = 'W';
    }

    sprintf(str, "%s %c %s %c", latStr, latDirn, lonStr, lonDirn);
}

/// <summary>
/// Calculate distance between two places using great circle route.
/// Returns the distance in nautical miles.
/// </summary>
/// <returns></returns>
double greatCircleDistance(Locn* loc1, Locn* loc2)
{
    double lat1r = loc1->lat * DegreesToRadians;
    double lon1r = loc1->lon * DegreesToRadians;
    double lat2r = loc2->lat * DegreesToRadians;
    double lon2r = loc2->lon * DegreesToRadians;

    double latDiff = lat1r - lat2r;
    double lonDiff = lon1r - lon2r;

    // Haversine formula
    double a = pow(sin(latDiff / 2.0), 2.0) + cos(lat1r) * cos(lat2r) * pow(sin(lonDiff / 2), 2);
    double c = 2 * atan2(sqrt(a), sqrt(1.0 - a));

    return RadiusOfEarthNm * c;
}

/// <summary>
/// Calculate coordinates of new point given starting
/// point, distance and heading.
/// </summary>
void greatCirclePos(Locn* loc, double headingTrue, double distanceNm)
{
    double lat1r = loc->lat * DegreesToRadians;
    double lon1r = loc->lon * DegreesToRadians;
    double hdgr = headingTrue * DegreesToRadians;
    double nmr = distanceNm / RadiusOfEarthNm;

    double lat2r = asin(sin(lat1r) * cos(nmr) + cos(lat1r) * sin(nmr) * cos(hdgr));
    double lon2r = lon1r + atan2(sin(hdgr) * sin(nmr) * cos(lat1r), cos(nmr) - sin(lat1r) * sin(lat2r));

    loc->lat = lat2r * 180.0 / M_PI;
    loc->lon = lon2r * 180.0 / M_PI;
}

void getIntersectX(int displayPosY, Position* pos, Intersect* intersect)
{
    double angle = _aircraftData.heading - 180;
    int adj = displayPosY - pos->y;
    int opp = (double)adj * tan(angle * DegreesToRadians);
    intersect->val = pos->x - opp;
    intersect->dist = sqrt(adj * adj + opp * opp);
}

void getIntersectY(int displayPosX, Position* pos, Intersect* intersect)
{
    double angle = _aircraftData.heading - 90;
    int adj = displayPosX - pos->x;
    int opp = (double)adj * tan(angle * DegreesToRadians);
    intersect->val = pos->y + opp;
    intersect->dist = sqrt(adj * adj + opp * opp);
}

void getCentreIntersect(Position* displayPos1, Position* displayPos2, Position* pos, AircraftPosition* adjustedPos)
{
    // Draw a line from centre of display to aircraft and see
    // where the line intersects a display edge.
    int centreX = _chart.x + _mouseData.dragX / _view.scale;
    int centreY = _chart.y + _mouseData.dragY / _view.scale;
    bool intersected = false;

    if (pos->x < displayPos1->x) {
        int xIntersectDiff = centreX - displayPos1->x;
        double xDiff = centreX - pos->x;
        double scale = xDiff / xIntersectDiff;
        int yDiff = centreY - pos->y;
        int intersectY = centreY - yDiff / scale;

        if (intersectY >= displayPos1->y && intersectY <= displayPos2->y) {
            intersected = true;
            adjustedPos->x = displayPos1->x;
            adjustedPos->y = intersectY;
        }
    }
    else if (pos->x > displayPos2->x) {
        int xIntersectDiff = centreX - displayPos2->x;
        double xDiff = centreX - pos->x;
        double scale = xDiff / xIntersectDiff;
        int yDiff = centreY - pos->y;
        int intersectY = centreY - yDiff / scale;

        if (intersectY >= displayPos1->y && intersectY <= displayPos2->y) {
            intersected = true;
            adjustedPos->x = displayPos2->x;
            adjustedPos->y = intersectY;
        }
    }

    if (!intersected) {
        if (pos->y < displayPos1->y) {
            int yIntersectDiff = centreY - displayPos1->y;
            double yDiff = centreY - pos->y;
            double scale = yDiff / yIntersectDiff;
            int xDiff = centreX - pos->x;
            int intersectX = centreX - xDiff / scale;

            adjustedPos->y = displayPos1->y;
            adjustedPos->x = intersectX;
        }
        else {
            int yIntersectDiff = centreY - displayPos2->y;
            double yDiff = centreY - pos->y;
            double scale = yDiff / yIntersectDiff;
            int xDiff = centreX - pos->x;
            int intersectX = centreX - xDiff / scale;

            adjustedPos->y = displayPos2->y;
            adjustedPos->x = intersectX;
        }
    }
}

/// <summary>
/// Convert aircraft location to aircraft chart position.
/// If aircraft is outside the display, bring it back on at the
/// nearest edge and indicate how many nm away the aircraft is.
/// </summary>
void aircraftLocToChartPos(AircraftPosition* adjustedPos)
{
    Position pos;
    locationToChartPos(&_aircraftData.loc, &pos);

    Position displayPos1;
    displayToChartPos(0, 0, &displayPos1);

    Position displayPos2;
    displayToChartPos(_displayWidth - 1, _displayHeight - 1, &displayPos2);

    // Shrink the display slightly to add a border
    double border = 15.0 / _view.scale;
    displayPos1.x += border;
    displayPos1.y += border;
    displayPos2.x -= border;
    displayPos2.y -= border;

    if (pos.x >= displayPos1.x && pos.x <= displayPos2.x
        && pos.y >= displayPos1.y && pos.y <= displayPos2.y)
    {
        // Aircraft is inside the display
        adjustedPos->x = pos.x;
        adjustedPos->y = pos.y;
        *adjustedPos->text = '\0';
        return;
    }

    // Project a line forwards from the aircraft on its current
    // heading to see where it intersects each display edge.
    Intersect topX;
    if (pos.y < displayPos1.y && _aircraftData.heading > 90 && _aircraftData.heading < 270) {
        getIntersectX(displayPos1.y, &pos, &topX);
        if (topX.val < displayPos1.x || topX.val > displayPos2.x) {
            topX.dist = 999999;
        }
    }
    else {
        topX.dist = 999999;
    }

    Intersect bottomX;
    if (pos.y > displayPos2.y && (_aircraftData.heading < 90 || _aircraftData.heading > 270)) {
        getIntersectX(displayPos2.y, &pos, &bottomX);
        if (bottomX.val < displayPos1.x || bottomX.val > displayPos2.x) {
            bottomX.dist = 999999;
        }
    }
    else {
        bottomX.dist = 999999;
    }

    Intersect leftY;
    if (pos.x < displayPos1.x && _aircraftData.heading < 180) {
        getIntersectY(displayPos1.x, &pos, &leftY);
        if (leftY.val < displayPos1.y || leftY.val > displayPos2.y) {
            leftY.dist = 999999;
        }
    }
    else {
        leftY.dist = 999999;
    }

    Intersect rightY;
    if (pos.x > displayPos2.x && _aircraftData.heading > 180) {
        getIntersectY(displayPos2.x, &pos, &rightY);
        if (rightY.val < displayPos1.y || rightY.val > displayPos2.y) {
            rightY.dist = 999999;
        }
    }
    else {
        rightY.dist = 999999;
    }

    // Find the closest intersect (if any)
    double minDist = topX.dist;
    adjustedPos->x = topX.val;
    adjustedPos->y = displayPos1.y;

    if (bottomX.dist < minDist) {
        minDist = bottomX.dist;
        adjustedPos->x = bottomX.val;
        adjustedPos->y = displayPos2.y;
    }
    if (leftY.dist < minDist) {
        minDist = leftY.dist;
        adjustedPos->x = displayPos1.x;
        adjustedPos->y = leftY.val;
    }
    if (rightY.dist < minDist) {
        minDist = rightY.dist;
        adjustedPos->x = displayPos2.x;
        adjustedPos->y = rightY.val;
    }

    // If no intersects use intersect from centre of display to aircraft (ignore heading)
    if (minDist == 999999) {
        getCentreIntersect(&displayPos1, &displayPos2, &pos, adjustedPos);
    }

    // Position label
    if (adjustedPos->y == displayPos1.y) {
        adjustedPos->labelY = 1;
    }
    else if (adjustedPos->y == displayPos2.y) {
        adjustedPos->labelY = -1;
    }
    else {
        adjustedPos->labelY = 0;
    }

    if (adjustedPos->x == displayPos1.x) {
        adjustedPos->labelX = 1;
    }
    else if (adjustedPos->x == displayPos2.x) {
        adjustedPos->labelX = -1;
    }
    else {
        adjustedPos->labelX = 0;
    }

    // Calculate how far off the display the aircraft is in nm.
    Locn intersectLoc;
    chartPosToLocation(adjustedPos->x, adjustedPos->y, &intersectLoc);
    double distance = greatCircleDistance(&intersectLoc, &_aircraftData.loc);

    if (distance >= 1) {
        int distanceX1 = distance + 0.5;
        sprintf(adjustedPos->text, "%d nm", distanceX1);
    }
    else {
        int distanceX10 = (distance + 0.05) * 10;
        int frac = distanceX10 % 10;
        if (frac == 0) {
            sprintf(adjustedPos->text, "%d nm", distanceX10 / 10);
        }
        else {
            sprintf(adjustedPos->text, "%d.%d nm", distanceX10 / 10, frac);
        }
    }
}

bool drawOther(Position* displayPos1, Position* displayPos2, Locn* loc, Position* pos, bool force)
{
    Position chartPos;
    locationToChartPos(loc, &chartPos);

    if (!force) {
        if (chartPos.x < displayPos1->x || chartPos.x > displayPos2->x
            || chartPos.y < displayPos1->y || chartPos.y > displayPos2->y)
        {
            return false;
        }
    }

    chartToDisplayPos(chartPos.x, chartPos.y, pos);

    return true;
}

CalibratedData* findClosestChart(CalibratedData* calib, int count, Locn* loc)
{
    CalibratedData* closest = calib;
    double minDistance = MAXINT;

    // Calculate distance to centre point of each calibration to find nearest one
    for (int i = 0; i < count; i++) {
        CalibratedData* nextCalib = calib + i;

        // Ignore any bad calibration data
        if (nextCalib->data.state == 2) {
            Locn centre;
            centre.lat = (nextCalib->data.lat[0] + nextCalib->data.lat[1]) / 2.0;
            centre.lon = (nextCalib->data.lon[0] + nextCalib->data.lon[1]) / 2.0;

            double distance = greatCircleDistance(&centre, loc);

            if (distance < minDistance) {
                closest = nextCalib;
                minDistance = distance;
            }
        }
    }

    return closest;
}

/// <summary>
/// Want a good view from the cockpit so move own
/// airaft slightly in front of followed aircraft.
/// </summary>
void adjustFollowLocation(LocData* locData, double ownWingSpan)
{
    // Move own aircraft slightly in front. Needs to be
    // further in front if following a larger aircraft.
    double feet = 25.0 + 25.0 * ((locData->wingSpan / ownWingSpan) - 1);

    // Need to be even further in front at higher speeds
    // as there will be more lag.
    if (locData->speed > 40) {
        feet *= 1 + (locData->speed - 40.0) / 500;
    }

    // Followed aircraft always seems to be a little higher
    // than own aircraft so reduce altitude a bit.
    locData->alt -= 3;

    double nm = feet / 6076.12;
    greatCirclePos(&locData->loc, locData->heading, nm);
}
