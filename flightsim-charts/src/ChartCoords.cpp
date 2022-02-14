#include <windows.h>
#include <iostream>
#include "ChartCoords.h"

// Externals
extern double DegreesToRadians;
extern int _displayWidth;
extern int _displayHeight;
extern DrawData _chart;
extern DrawData _view;
extern LocData _aircraftLoc;
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
void locationToChartPos(double lat, double lon, Position* pos)
{
    double lonCalibDiff = _chartData.lon[1] - _chartData.lon[0];
    double lonDiff = lon - _chartData.lon[0];
    double xScale = lonDiff / lonCalibDiff;

    double latCalibDiff = _chartData.lat[1] - _chartData.lat[0];
    double latDiff = lat - _chartData.lat[0];
    double yScale = latDiff / latCalibDiff;

    int xCalibDiff = _chartData.x[1] - _chartData.x[0];
    int yCalibDiff = _chartData.y[1] - _chartData.y[0];

    pos->x = _chartData.x[0] + xCalibDiff * xScale;
    pos->y = _chartData.y[0] + yCalibDiff * yScale;
}

/// <summary>
/// Convert chart position to location. Chart must be calibrated.
/// </summary>
void chartPosToLocation(int x, int y, Location* loc)
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
/// Returns formatted co-ordinate, e.g. 51� 28' 29.60"
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
/// Returns formatted location, e.g. 51� 28' 29.60" N 0� 29' 5.19" W
/// </summary>
void locationToString(Location* loc, char* str)
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
double greatCircleDistance(Location loc1, double lat2, double lon2)
{
    const double RadiusOfEarthNm = 3440.1;

    double lat1r = loc1.lat * DegreesToRadians;
    double lon1r = loc1.lon * DegreesToRadians;
    double lat2r = lat2 * DegreesToRadians;
    double lon2r = lon2 * DegreesToRadians;

    double latDiff = lat1r - lat2r;
    double lonDiff = lon1r - lon2r;

    // Haversine formula
    double a = pow(sin(latDiff / 2.0), 2.0) + cos(lat1r) * cos(lat2r) * pow(sin(lonDiff / 2), 2);
    double c = 2 * atan2(sqrt(a), sqrt(1.0 - a));

    return RadiusOfEarthNm * c;
}

/// <summary>
/// Convert aircraft location to aircraft chart position.
/// If aircraft is outside the display, bring it back on at the
/// nearest edge and indicate how many nm away the aircraft is.
/// </summary>
void aircraftLocToChartPos(AircraftPosition* adjustedPos)
{
    Position pos;
    locationToChartPos(_aircraftLoc.lat, _aircraftLoc.lon, &pos);

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

    // Draw a line from centre of display to aircraft and see
    // where the line intersects a display edge.
    int centreX = _chart.x + _mouseData.dragX / _view.scale;
    int centreY = _chart.y + _mouseData.dragY / _view.scale;
    bool intersected = false;

    if (pos.x < displayPos1.x) {
        int xIntersectDiff = centreX - displayPos1.x;
        double xDiff = centreX - pos.x;
        double scale = xDiff / xIntersectDiff;
        int yDiff = centreY - pos.y;
        int intersectY = centreY - yDiff / scale;

        if (intersectY >= displayPos1.y && intersectY <= displayPos2.y) {
            intersected = true;
            adjustedPos->x = displayPos1.x;
            adjustedPos->y = intersectY;
            adjustedPos->labelX = 1;
            adjustedPos->labelY = 0;
        }
    }
    else if (pos.x > displayPos2.x) {
        int xIntersectDiff = centreX - displayPos2.x;
        double xDiff = centreX - pos.x;
        double scale = xDiff / xIntersectDiff;
        int yDiff = centreY - pos.y;
        int intersectY = centreY - yDiff / scale;

        if (intersectY >= displayPos1.y && intersectY <= displayPos2.y) {
            intersected = true;
            adjustedPos->x = displayPos2.x;
            adjustedPos->y = intersectY;
            adjustedPos->labelX = -1;
            adjustedPos->labelY = 0;
        }
    }

    if (!intersected) {
        if (pos.y < displayPos1.y) {
            int yIntersectDiff = centreY - displayPos1.y;
            double yDiff = centreY - pos.y;
            double scale = yDiff / yIntersectDiff;
            int xDiff = centreX - pos.x;
            int intersectX = centreX - xDiff / scale;

            adjustedPos->y = displayPos1.y;
            adjustedPos->x = intersectX;
            adjustedPos->labelY = 1;
            adjustedPos->labelX = 0;
        }
        else {
            int yIntersectDiff = centreY - displayPos2.y;
            double yDiff = centreY - pos.y;
            double scale = yDiff / yIntersectDiff;
            int xDiff = centreX - pos.x;
            int intersectX = centreX - xDiff / scale;

            adjustedPos->y = displayPos2.y;
            adjustedPos->x = intersectX;
            adjustedPos->labelY = -1;
            adjustedPos->labelX = 0;
        }
    }

    // Calculate how far off the display the aircraft is in nm.
    Location intersectLoc;
    chartPosToLocation(adjustedPos->x, adjustedPos->y, &intersectLoc);
    double distance = greatCircleDistance(intersectLoc, _aircraftLoc.lat, _aircraftLoc.lon);

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

bool drawOtherAircraft(Position* displayPos1, Position* displayPos2, LocData* loc, Position* pos)
{
    Position chartPos;
    locationToChartPos(loc->lat, loc->lon, &chartPos);

    if (chartPos.x < displayPos1->x || chartPos.x > displayPos2->x
        || chartPos.y < displayPos1->y || chartPos.y > displayPos2->y)
    {
        return false;
    }

    chartToDisplayPos(chartPos.x, chartPos.y, pos);

    return true;
}

CalibratedData* findClosestChart(CalibratedData* calib, int count, LocData* loc)
{
    CalibratedData* closest = calib;
    double minDistance = MAXINT;

    // Calculate distance to centre point of each calibration to find nearest one
    for (int i = 0; i < count; i++) {
        CalibratedData* nextCalib = calib + i;

        // Ignore any bad calibration data
        if (nextCalib->data.state == 2) {
            Location centre;
            centre.lat = (nextCalib->data.lat[0] + nextCalib->data.lat[1]) / 2.0;
            centre.lon = (nextCalib->data.lon[0] + nextCalib->data.lon[1]) / 2.0;

            double distance = greatCircleDistance(centre, loc->lat, loc-> lon);

            if (distance < minDistance) {
                closest = nextCalib;
                minDistance = distance;
            }
        }
    }

    return closest;
}
