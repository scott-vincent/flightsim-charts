#include <windows.h>
#include <iostream>
#include <allegro5/allegro.h>
#include <allegro5/allegro_windows.h>
#include "flightsim-charts.h"
#include "ChartFile.h"
#include "ChartCoords.h"

// Externals
extern ALLEGRO_DISPLAY* _display;
extern int _titleState;
extern int _titleDelay;
extern FlightPlanData _flightPlan[MAX_FLIGHT_PLAN];
extern int _flightPlanCount;
extern ObstacleData* _obstacles;
extern int _obstacleCount;
extern bool _showObstacleNames;
extern ElevationData* _elevations;
extern int _elevationCount;

// Constants
const char* UkElevationData =
    "25,32,05,00,11,39,39,23,11,22,25,14,08,16,08,07,06,05,05\n"
    "31,34,05,00,16,25,33,31,21,12,15,13,15,08,05,07,09,05,02\n"
    "23,09,00,11,12,17,28,27,21,18,14,12,10,10,09,08,13,06,06\n"
    "00,00,02,10,21,17,30,33,20,14,14,12,13,11,09,07,07,06,06\n"
    "00,00,00,00,09,13,21,17,20,13,13,14,13,13,14,10,13,04,05\n"
    "00,00,00,06,17,24,24,16,13,13,10,14,12,12,11,10,00,11,11\n"
    "00,05,12,14,14,16,20,07,00,00,00,00,00,00,00,00,09,11,09\n"
    "00,05,02,06,00,00,00,00,07,08,10,09,00,00,09,10,14,13,11\n"
    "00,00,00,00,00,00,00,00,07,09,10,10,11,08,10,10,00,00,00\n";
const double UkLatFrom = 53.25;
const double UkLonFrom = -6.75;

// Prototypes
void loadFlightPlan(const char* filename);
void clearFlightPlan();
void clearElevations();
void loadObstacles(const char* filename);
void clearObstacles();


/// <summary>
/// Select a flight plan file
/// </summary>
void newFlightPlan()
{
    al_set_window_title(_display, "Select Flight Plan");
    _titleState = -2;

    char* flightPlan = fileSelectorDialog(al_get_win_window_handle(_display), ".pln\0*.pln\0");
    if (*flightPlan == '\0') {
        return;
    }

    loadFlightPlan(flightPlan);
}

/// <summary>
/// Load flight plan
/// </summary>
void loadFlightPlan(const char *filename)
{
    clearFlightPlan();

    FILE* inf = fopen(filename, "r");
    if (inf == NULL) {
        return;
    }

    char line[256];
    int x;
    int y;
    double lat;
    double lon;

    int num = 0;
    char name[256];
    char position[256];
    Locn loc;

    strcpy(name, "UNKNOWN");
    while (fgets(line, 256, inf)) {
        char* startPos = line;
        while (*startPos == ' ') {
            startPos++;
        }

        if (strncmp(startPos, "<ATCWaypoint id=\"", 17) == 0) {
            strcpy(name, startPos + 17);
            char *endPos = strchr(name, '"');
            *endPos = '\0';
        }
        else if (strncmp(startPos, "<WorldPosition>", 15) == 0) {
            strcpy(position, startPos + 15);
            char* endPos = strchr(position, '<');
            *endPos = '\0';

            convertTextReadableLocation(position, &loc);

            if (_flightPlanCount < MAX_FLIGHT_PLAN) {
                name[31] = '\0';
                strcpy(_flightPlan[_flightPlanCount].name, name);
                _flightPlan[_flightPlanCount].loc.lat = loc.lat;
                _flightPlan[_flightPlanCount].loc.lon = loc.lon;
            }

            _flightPlanCount++;
        }
    }

    fclose(inf);

    // Need at least 1 leg (2 waypoints)
    if (_flightPlanCount < 2) {
        _flightPlanCount = 0;
    }

    for (int i = 0; i < _flightPlanCount; i++) {
        createTagBitmap(_flightPlan[i].name, &_flightPlan[i].tag, false);
    }

    return;
}

/// <summary>
/// Clear flight plan
/// </summary>
void clearFlightPlan()
{
    for (int i = 0; i < _flightPlanCount; i++) {
        cleanupTagBitmap(&_flightPlan[i].tag);
    }

    _flightPlanCount = 0;
}

/// <summary>
/// Create UK elevations
/// </summary>
void newElevations()
{
    al_set_window_title(_display, "Creating UK Elevations");
    _titleState = -2;

    clearElevations();

    // Allocate memory for 200 elevations at a time
    int mallocedElevations = 200;
    _elevations = (ElevationData*)malloc(mallocedElevations * sizeof(ElevationData));
    if (_elevations == NULL) {
        printf("Ran out of memory\n");
        exit(1);
    }

    const char* pos = UkElevationData;
    double lat = UkLatFrom;
    double lon = UkLonFrom;

    while (*pos != '\0') {
        char* nextPos;
        int elevation = strtol(pos, &nextPos, 0);

        if (elevation > 0) {
            char nameTag[16];
            sprintf(nameTag, "%d", elevation * 100);
            createTagBitmap(nameTag, &_elevations[_elevationCount].tag, false, true);

            _elevations[_elevationCount].loc.lat = lat;
            _elevations[_elevationCount].loc.lon = lon;

            _elevationCount++;
        }

        if (_elevationCount == mallocedElevations) {
            mallocedElevations += 200;
            ElevationData* savedElevations = _elevations;
            _elevations = (ElevationData*)realloc(_elevations, mallocedElevations * sizeof(ElevationData));
            if (_elevations == NULL) {
                printf("Ran out of memory\n");
                free(savedElevations);
                exit(1);
            }
        }

        if (*nextPos == ',') {
            pos = nextPos + 1;
            lon += 0.5;
        }
        else if (*nextPos == '\n') {
            pos = nextPos + 1;
            lat -= 0.5;
            lon = UkLonFrom;
        }
        else {
            pos = nextPos;
        }
    }
}

/// <summary>
/// Clear obstacles
/// </summary>
void clearElevations()
{
    if (_elevationCount == 0) {
        return;
    }

    for (int i = 0; i < _elevationCount; i++) {
        cleanupTagBitmap(&_elevations[i].tag);
    }

    free(_elevations);
    _elevationCount = 0;
}

/// <summary>
/// Select am obstacle file
/// </summary>
void newObstacles()
{
    al_set_window_title(_display, "Select Obstacle File");
    _titleState = -2;

    char* obstacleFile = fileSelectorDialog(al_get_win_window_handle(_display), ".csv\0*.csv\0");
    if (*obstacleFile == '\0') {
        return;
    }

    al_set_window_title(_display, "Loading Obstacle File ...");
    loadObstacles(obstacleFile);
}

/// <summary>
/// Splits a line into columns.
/// A column may be enclosed in quotes if it contains a comma.
/// </summary>
void getColumns(char* line, char columns[9][256])
{
    char* startPos = line;
    int col = 0;

    while (*startPos != '\n' && *startPos != '\0') {
        char* endPos;

        if (*startPos == '"') {
            startPos++;
            endPos = strchr(startPos, '"');
        }
        else {
            endPos = strchr(startPos, ',');
            if (!endPos) {
                endPos = strchr(startPos, '\n');
            }
            if (!endPos) {
                endPos = startPos;
            }
        }

        int len = endPos - startPos;
        if (len > 255) {
            len = 255;
        }
        strncpy(columns[col], startPos, len);
        columns[col][len] = '\0';
        col++;

        if (col == 9) {
            break;
        }

        if (*endPos == '"') {
            endPos++;
        }

        if (*endPos == ',') {
            endPos++;
        }

        startPos = endPos;
    }
}

bool hasMatchedQuotes(char* text)
{
    int count = 0;

    for (int i = 0; i < strlen(text); i++) {
        if (text[i] == '"') {
            count++;
        }
    }

    return count % 2 == 0;
}

/// <summary>
/// Converts a latitude or longitude from text to double.
/// e.g. "0021244.00W" converts to -0.21244
/// </summary>
double convertLatLon(char* text)
{
    double num = strtod(text, NULL);

    int deg = num / 10000;
    int min = (num - deg * 10000) / 100;
    double sec = num - (deg * 10000.0 + min * 100.0);

    double latLon = deg + (min / 60.0) + (sec / 3600.0);

    char lastCh = text[strlen(text) - 1];
    if (lastCh == 'S' || lastCh == 'W') {
        latLon = -latLon;
    }

    return latLon;
}

/// <summary>
/// Load obstacle file
/// </summary>
void loadObstacles(const char* filename)
{
    clearObstacles();

    // Allocate memory for 100 obstacles at a time
    int mallocedObstacles = 100;
    _obstacles = (ObstacleData*)malloc(mallocedObstacles * sizeof(ObstacleData));
    if (_obstacles == NULL) {
        printf("Ran out of memory\n");
        exit(1);
    }

    FILE* inf = fopen(filename, "r");
    if (inf == NULL) {
        return;
    }

    char line[2048];
    char columns[9][256];

    if (!fgets(line, 2048, inf)) {
        al_set_window_title(_display, "Error: Obstacle file is empty");
        _titleState = -9;
        _titleDelay = 100;
        return;
    }

    bool isNats = true;
    if (strstr(line, "Type,Name,Ident,Latitude,Longitude,Elevation") != NULL) {
        // Little NavMap format
        isNats = false;
    }

    if (isNats) {
        // NATS format
        if (strstr(line, "Designation/Identification,") == NULL) {
            al_set_window_title(_display, "Error: Obstacle file has bad/missing header");
            _titleState = -9;
            _titleDelay = 100;
            return;
        }
    }

    while (fgets(line, 2048, inf)) {
        //Make sure we have an even number of quotes as entries can contain embedded CRLFs
        while (!hasMatchedQuotes(line)) {
            char moreLine[2048];
            if (!fgets(moreLine, 2048, inf)) {
                al_set_window_title(_display, "Error: Incomplete file - No matching quote");
                _titleState = -9;
                _titleDelay = 100;
                return;
            }

            char* joinPos = strchr(line, '\n');
            if (!joinPos) {
                joinPos = line + strlen(line);
            }

            *(joinPos++) = ',';
            strcpy(joinPos, moreLine);
        }

        getColumns(line, columns);

        char name[256];
        double lat, lon;
        int elevation;

        if (isNats) {
            strcpy(name, columns[1]);
            elevation = strtol(columns[3], NULL, 0);

            char* sep = strchr(columns[2], ',');
            if (!sep) {
                char msg[256];
                sprintf(msg, "Error: Missing comma in latLon string: %s\n", columns[2]);
                al_set_window_title(_display, msg);
                _titleState = -9;
                _titleDelay = 100;
                return;
            }

            *sep = '\0';
            lat = convertLatLon(columns[2]);
            lon = convertLatLon(sep + 1);
        }
        else {
            strcpy(name, columns[8]);
            lat = strtod(columns[3], NULL);
            lon = strtod(columns[4], NULL);
            elevation = strtol(columns[5], NULL, 0);
        }

        char* endName = strchr(name, ',');
        if (endName) {
            *endName = '\0';
        }

        if (strstr(name, "WINDMOTOR")) {
            strcpy(_obstacles[_obstacleCount].name, "WIND TURBINE");
        }
        else {
            strncpy(_obstacles[_obstacleCount].name, name, 32);
            _obstacles[_obstacleCount].name[31] = '\0';
        }

        _obstacles[_obstacleCount].loc.lat = lat;
        _obstacles[_obstacleCount].loc.lon = lon;
        sprintf(_obstacles[_obstacleCount].elevation, "%d ft", elevation);

        _obstacleCount++;

        if (_obstacleCount == mallocedObstacles) {
            mallocedObstacles += 100;
            ObstacleData* savedObstacles = _obstacles;
            _obstacles = (ObstacleData*)realloc(_obstacles, mallocedObstacles * sizeof(ObstacleData));
            if (_obstacles == NULL) {
                printf("Ran out of memory\n");
                free(savedObstacles);
                exit(1);
            }
        }
    }

    fclose(inf);

    for (int i = 0; i < _obstacleCount; i++) {
        if (_showObstacleNames) {
            createTagBitmap(_obstacles[i].name, &_obstacles[i].tag, true);
        }
        else {
            _obstacles[i].tag.bmp = NULL;
        }
        createTagBitmap(_obstacles[i].elevation, &_obstacles[i].moreTag, true);
    }
}

/// <summary>
/// Clear obstacles
/// </summary>
void clearObstacles()
{
    if (_obstacleCount == 0) {
        return;
    }

    for (int i = 0; i < _obstacleCount; i++) {
        cleanupTagBitmap(&_obstacles[i].tag);
        cleanupTagBitmap(&_obstacles[i].moreTag);
    }

    free(_obstacles);
    _obstacleCount = 0;
    _showObstacleNames = false;
}
