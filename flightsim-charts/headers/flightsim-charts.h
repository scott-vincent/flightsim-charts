#pragma once
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>

// Constants
const int MAX_AIRCRAFT = 800;
const int MAX_CHARTS = 1000;
const int AIRCRAFT_RANGE = 20000;   // metres
const int MAX_RANGE = 200000;       // metres
const int WINGSPAN_SMALL = 60;      // feet
const char DefaultChart[] = "Airport Charts\\EG\\LL\\EGLL Heathrow.png";
const int DefaultFPS = 8;
const char DegreesSymbol[] = "\xC2\xB0";
const char Helis[] = "_A10_A16_A18_AS5_B06_B50_B17_EC3_EC5_R44_R66_WAS_";
const char Airliners[] = "_A20_A21_A31_A32_A33_BCS_B38_B73_B75_B76_";
const char Large_Airliners[] = "_A34_A35_A38_B74_B77_B78_";
const char Jets[] = "_C55_C56_C68_CL6_E55_F90_";

struct Position {
    int x;
    int y;
};

struct Locn {
    double lat;
    double lon;
};

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
    ALLEGRO_BITMAP* helicopterOtherBmp;
    ALLEGRO_BITMAP* gliderOtherBmp;
    ALLEGRO_BITMAP* largeOtherBmp;
    ALLEGRO_BITMAP* jetOtherBmp;
    ALLEGRO_BITMAP* vehicleBmp;
    ALLEGRO_BITMAP* airportBmp;
    ALLEGRO_BITMAP* waypointBmp;
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
    Locn loc;
    double heading;
    double bank;
    double pitch;
    double alt;
    double speed;
    double wingSpan;
    char callsign[32];
    char model[32];
};

struct WindData {
    double direction;
    double speed;
};

struct OtherData {
    Locn loc;
    double heading;
    double wingSpan;
    char callsign[32];
    char model[32];
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

struct TeleportData {
    // Data to send to SimConnect must come first
    Locn loc;
    double heading;
    double bank;
    double pitch;
    double alt;
    double speed;

    int dataSize;
    Position pos;
    bool inProgress;
    bool setAltSpeed;
};

struct SnapshotData {
    Locn loc;
    double heading;
    double bank;
    double pitch;
    double alt;
    double speed;

    int dataSize;
    bool save;
    bool restore;
};

struct FollowData {
    char callsign[32];
    DWORD aircraftId;
    char ownTag[32];
    double ownWingSpan;
    bool inProgress;
};

struct IconData {
    ALLEGRO_BITMAP* bmp;
    int halfWidth;
    int halfHeight;
};

struct AI_Aircraft {
    Locn loc;
    double heading;
    double bank;
    double pitch;
    double alt;
    double speed;

    char callsign[16];
    char airline[16];
    char model[16];

    time_t lastUpdated;
    DWORD objectId;
    TagData tagData;
};

struct AI_Fixed {
    Locn loc;
    double heading;
    double bank;
    double pitch;
    double alt;
    double speed;

    char model[16];
    TagData tagData;
};

const int Max_AI_Aircraft = 5000;
const int Max_AI_Fixed = 500;

// Prototypes
int showMessage(const char* message, bool isError, const char* title = NULL, bool canCancel = false);
void createTagText(char* callsign, char* model, char* tagText);
void createTagBitmap(TagData* otherTag);
void cleanupBitmap(ALLEGRO_BITMAP* bmp);
