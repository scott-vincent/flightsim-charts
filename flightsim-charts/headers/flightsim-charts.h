#pragma once
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>

// Constants
const int MAX_AIRCRAFT = 150;
const int MAX_CHARTS = 1000;
const int AIRCRAFT_RANGE = 8000;  // metres
const int WINGSPAN_SMALL = 60;    // feet
const char DefaultChart[] = "Airport Charts\\EG\\LL\\EGLL Heathrow.png";
const int DefaultFPS = 8;
const char DegreesSymbol[] = "\xC2\xB0";

struct DrawData {
    ALLEGRO_BITMAP* bmp;
    int x;
    int y;
    int width;
    int height;
    double scale;
};

struct AircraftDrawData {
    ALLEGRO_BITMAP* bmp;
    ALLEGRO_BITMAP* otherBmp;
    int x;
    int y;
    int halfWidth;
    int halfHeight;
    double scale;
    ALLEGRO_BITMAP* smallBmp;
    ALLEGRO_BITMAP* smallOtherBmp;
    int smallHalfWidth;
    int smallHalfHeight;
};

struct MouseData {
    bool dragging;
    int dragStartX;
    int dragStartY;
    int dragX;
    int dragY;
};

struct LocData {
    double lat;
    double lon;
    double heading;
    double wingSpan;
    char callsign[32];
    char model[32];
};

struct WindData {
    double direction;
    double speed;
};

struct TagData {
    char tagText[68];
    DrawData tag;
};

struct ChartData {
    int state;
    int x[2];
    int y[2];
    double lat[2];
    double lon[2];
};

struct Settings {
    int x;
    int y;
    int width;
    int height;
    char chart[256];
    int framesPerSec;
};

struct CalibratedData {
    char filename[256];
    ChartData data;
};

struct Position {
    int x;
    int y;
};

struct Location {
    double lat;
    double lon;
};

struct TeleportData {
    // Data to send to SimConnect must come first
    Location loc;
    double heading;
    double bank;
    double pitch;

    int dataSize;
    Position pos;
    bool inProgress;
};

struct SnapshotData {
    Location loc;
    double heading;
    double bank;
    double pitch;
    double alt;
    double speed;

    int dataSize;
    bool save;
    bool restore;
};

// Prototypes
int showMessage(const char* message, bool isError, const char* title = NULL, bool canCancel = false);
