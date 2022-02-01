#pragma once
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>

// Constants
const int MAX_AIRCRAFT = 150;
const int AIRCRAFT_RANGE = 8000;  // metres
const int WINGSPAN_SMALL = 60;    // feet
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
    char title[256];
    int state;
    int x[2];
    int y[2];
    double lat[2];
    double lon[2];
};

struct ProgramData {
    int x;
    int y;
    int width;
    int height;
    char chart[256];
};

struct Position {
    int x;
    int y;
};

struct Location {
    double lat;
    double lon;
};

// Prototypes
extern void showMessage(const char*);
