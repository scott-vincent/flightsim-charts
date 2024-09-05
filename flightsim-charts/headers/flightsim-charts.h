#pragma once
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>

// Constants
const int MAX_AIRCRAFT = 800;
const int MAX_CHARTS = 1000;
const int MAX_FLIGHT_PLAN = 64;
const int MAX_OBSTACLE = 5000;
const int AIRCRAFT_RANGE = 20000;   // metres
const int MAX_RANGE = 200000;       // metres
const int WINGSPAN_SMALL = 60;      // feet
const char DefaultChart[] = "Airport Charts\\London & Surrounding Area.png";
const int DefaultFPS = 8;
const char DegreesSymbol[] = "\xC2\xB0";

const char Airliner[] = "_A20_A21_A30_A31_A32_A33_BCS_B38_B73_B75_B76_E19_E29_E75_";
const char Large_Airliner[] = "_A34_A35_A38_A3S_B74_B77_B78_";

const char Heli[] = "_A10_A13_A16_A18_AS5_B06_B50_B17_CLO_EC3_EC4_EC5_EC7_EXP_G2C_MM1_R22_R44_R66_S76_WAS_";
const char Jet[] = "_AST_BE4_C25_C51_C52_C55_C56_C68_C75_CL3_CL6_CRJ_E13_E14_E35_E50_E55_EA5_F2T_F90_FA2_FA7_FA8_G28_GA5_GA6_GAL_GL5_GL7_GLE_GLF_H25_HDJ_LJ3_LJ7_PC2_PRM_";
const char Turboprop[] = "_AT4_AT7_B35_BE2_D22_D32_DH8_DHC_P18_SC7_SF3_C303_";

const char Military_Heli[] = "_H47_H64_LYN_PUM_UGLY1_"; // UGLY = Apache
const char Military_Jet[] = "_SB3_F15_HAW_HUN_REDAR_";  // REDAR = Red Arrow
const char Military_Small[] = "_SPI_GTCHI_P51_FUR_";    // GTCHI = Spitfire
const char Military_Other[] = "_A40_B52_C13_C30_E39_E3C_K35_LAN_";
const char Military_Other2[] = "_C17_";                 // Matches full model only (to stop it matching, e.g. C172)

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
    ALLEGRO_BITMAP* militaryHeliBmp;
    ALLEGRO_BITMAP* militaryJetBmp;
    ALLEGRO_BITMAP* militarySmallBmp;
    ALLEGRO_BITMAP* militaryOtherBmp;
    ALLEGRO_BITMAP* turbopropOtherBmp;
    ALLEGRO_BITMAP* vehicleBmp;
    ALLEGRO_BITMAP* airportBmp;
    ALLEGRO_BITMAP* waypointBmp;
    ALLEGRO_BITMAP* homeBmp;
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
    double headingMag;
    double altIndicated;
    double flaps;
    double trim;
    double verticalSpeed;
    double parkBrake;
    double leftBrake;
    double rightBrake;
};

struct WindData {
    double direction;
    double speed;
};

struct OtherData {
    Locn loc;
    double heading;
    double alt;
    double speed;
    double wingSpan;
    char callsign[32];
    char model[32];
};

struct TagData {
    char tagText[68];
    DrawData tag;
    char moreTagText[68];
    DrawData moreTag;
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
    char location[256];
    bool showTags = true;
    bool showFixedTags = true;
    bool showAiInfoTags = true;
    bool showAiPhotos = true;
    bool showAiMilitaryOnly = false;
    bool showInstrumentHud = true;
    bool showAlwaysOnTop = false;
    bool showMiniMenu = false;
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
    int settleDelay;
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
    bool pause;
    bool paused;
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
    bool isMilitary;
};

struct AI_Aircraft {
    Locn loc;
    double heading;
    double bank;
    double pitch;
    double alt;
    double speed;

    char callsign[16];
    char airline[32];
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

struct AI_ModelMatch {
    char prefix[6];
    char model[4];
    char* modelNames;
};

struct AI_Trail {
    char callsign[16];
    char airline[64];
    char modelType[64];
    char image[256];
    char fromAirport[128];
    char toAirport[128];
    Locn loc[5000];
    int count;
};

struct FlightPlanData {
    char name[32];
    Locn loc;
    DrawData tag;
    Position pos;
};

struct ElevationData {
    Locn loc;
    DrawData tag;
};

struct VrpData {
    char name[32];
    Locn loc;
    DrawData tag;
};

struct ObstacleData {
    char name[32];
    char elevation[16];
    Locn loc;
    DrawData tag;
    DrawData moreTag;
};

const int Max_AI_Aircraft = 5000;
const int Max_AI_Fixed = 500;
const int Max_AI_ModelMatch = 25000;

// Prototypes
int showMessage(const char* message, bool isError, const char* title = NULL, bool canCancel = false);
void createTagText(char* callsign, char* model, char* tagText);
void createTagBitmap(char *tagText, DrawData* tag, bool isMeasure = false, bool isElevation = false);
void cleanupBitmap(ALLEGRO_BITMAP* bmp);
void cleanupTagBitmap(DrawData* tag);
