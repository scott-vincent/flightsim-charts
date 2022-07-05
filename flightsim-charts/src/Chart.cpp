#include <windows.h>
#include <iostream>
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_windows.h>
#include "flightsim-charts.h"
#include "ChartFile.h"
#include "ChartCoords.h"

// Constants
const char ProgramName[] = "FlightSim Charts";
const char AircraftFile[] = "images/aircraft.png";
const char AircraftSmallFile[] = "images/aircraft_small.png";
const char AircraftOtherFile[] = "images/aircraft_other.png";
const char AircraftSmallOtherFile[] = "images/aircraft_small_other.png";
const char HelicopterOtherFile[] = "images/helicopter_other.png";
const char GliderOtherFile[] = "images/glider_other.png";
const char LargeOtherFile[] = "images/aircraft_large_other.png";
const char JetOtherFile[] = "images/small_jet_other.png";
const char MilitaryOtherFile[] = "images/military_jet_other.png";
const char TurbopropOtherFile[] = "images/turboprop_other.png";
const char VehicleFile[] = "images/vehicle.png";
const char AirportFile[] = "images/airport.png";
const char WaypointFile[] = "images/waypoint.png";
const char LabelFile[] = "images/label.png";
const char MarkerFile[] = "images/marker.png";
const char RingFile[] = "images/ring.png";
const char ArrowFile[] = "images/arrow.png";
const int MinScale = 5;
const int InitScale = 40;
const int MaxScale = 200;

// Externals
extern bool _quit;
extern LocData _aircraftData;
extern WindData _windData;
extern OtherData _otherAircraftData[MAX_AIRCRAFT];
extern int _otherAircraftCount;
extern int _otherDataSize;
extern TeleportData _teleport;
extern SnapshotData _snapshot;
extern FollowData _follow;
extern int _range;
extern bool _maxRange;
extern bool _showAi;
extern int _aiFixedCount;
extern AI_Fixed _aiFixed[Max_AI_Fixed];
extern bool _connected;
extern int _aiAircraftCount;
extern AI_Aircraft _aiAircraft[Max_AI_Aircraft];
extern AI_Trail _aiTrail[3];
extern char _watchCallsign[16];
extern bool _watchInProgress;


// Variables
double DegreesToRadians = ALLEGRO_PI / 180.0f;
ALLEGRO_FONT* _font = NULL;
ALLEGRO_DISPLAY* _display = NULL;
ALLEGRO_TIMER* _timer = NULL;
ALLEGRO_EVENT_QUEUE* _eventQueue = NULL;
int _displayWidth;
int _displayHeight;
HWND _displayWindow;
int _winCheckDelay = 0;
DrawData _chart;
DrawData _view;
AircraftDrawData _aircraft;
DrawData _aircraftLabel;
ALLEGRO_BITMAP* _aircraftLabelBmpCopy;
char _labelText[256];
char _tagText[68];
DrawData _marker;
DrawData _ring;
DrawData _arrow;
DrawData _windInfo;
DrawData _windInfoCopy;
MouseData _mouseData;
ChartData _chartData;
Settings _settings;
ALLEGRO_MOUSE_STATE _mouse;
OtherData _snapshotOtherData[MAX_AIRCRAFT];
int _snapshotOtherCount;
TagData _otherTag[MAX_AIRCRAFT];
TagData _oldTag[MAX_AIRCRAFT];
int _tagCount = 0;
int _tagDataSize = sizeof(TagData);
int _drawDataSize = sizeof(DrawData);
int _mouseStartZ = 0;
int _titleState;
int _titleDelay;
bool _showTags = true;
bool _showAiPhotos = false;
bool _showCalibration = false;
Locn _clickedLoc;
Position _clickedPos;
Position _clipboardPos;
int _windDirection = -1;
int _windSpeed = -1;
int _menuItem = -1;
int _lastRotate = MAXINT;
char _closestAircraft[32];
bool _menuCallback = false;
char _aiTitle[512] = "";

enum MENU_ITEMS {
    MENU_LOAD_CHART,
    MENU_LOAD_CLOSEST,
    MENU_LOCATE_AIRCRAFT,
    MENU_FIX_CRASH,
    MENU_ROTATE_AGAIN,
    MENU_ROTATE_LEFT_20,
    MENU_ROTATE_RIGHT_20,
    MENU_ROTATE_LEFT_90,
    MENU_ROTATE_RIGHT_90,
    MENU_TELEPORT_HERE,
    MENU_TELEPORT_HERE_GROUND,
    MENU_TELEPORT_HERE_AIR,
    MENU_TELEPORT_CLIPBOARD,
    MENU_TELEPORT_RESTORE_LOCATION,
    MENU_TELEPORT_SAVE_LOCATION,
    MENU_TELEPORT_FOLLOW,
    MENU_MAX_RANGE,
    MENU_SHOW_TAGS,
    MENU_SHOW_AI_PHOTOS,
    MENU_CLEAR_AI_TRAILS,
    MENU_SHOW_CALIBRATION,
    MENU_RECALIBRATE,
};

// Prototypes
void newChart();
void closestChart();
void clearCustomPoints();


int showMessage(const char *message, bool isError, const char *title, bool canCancel)
{
    if (title == NULL) {
        title = ProgramName;
    }

    UINT msgType;
    if (canCancel) {
        msgType = MB_OKCANCEL;
    }
    else {
        msgType = MB_OK;
    }

    if (isError) {
        msgType |= MB_ICONERROR;
    }
    else {
        msgType |= MB_ICONINFORMATION;
    }

    return MessageBox(_displayWindow, message, title, msgType);
}

int showClipboardMessage(bool isError, bool canCancel = false)
{
    char* title = NULL;
    char calibTitle[256];
    char message[512];

    if (isError) {
        strcpy(message, "Clipboard does not contain the required information.\n");
        strcat(message, "Use one of the following methods.\n\n");
    }
    else {
        title = calibTitle;
        sprintf(calibTitle, "%s - How To Calibrate Your Chart", ProgramName);
        strcpy(message, "Two points are required for calibration. The window title shows which point is being requested.\n\n");
        strcat(message, "Use one of the following methods, then right-click on this chart at the equivalent position.\n\n");
    }

    strcat(message, "Little Navmap:\n");
    strcat(message, "     Right-click on desired location\n");
    strcat(message, "     More\n");
    strcat(message, "     Copy to clipboard\n\n");
    strcat(message, "Google Maps:\n");
    strcat(message, "     Right-click on desired location\n");
    strcat(message, "     Select top line (lat, lon)\n\n");
    strcat(message, "OpenStreetMap:\n");
    strcat(message, "     Right-click on desired location\n");
    strcat(message, "     Centre map here\n");
    strcat(message, "     Copy URL");

    return showMessage(message, isError, title, canCancel);
}

/// <summary>
/// Display a message in the window title bar that disappears after about 3 seconds.
/// </summary>
void showTitleMessage(const char* message)
{
    al_set_window_title(_display, message);
    _titleState = -9;
    _titleDelay = 50;
}

void cancelCalibration()
{
    _titleState = -2;
    _chartData.state = -1;

    // Try to load previous calibration
    loadCalibrationData(&_chartData);

    if (_chartData.state != 2) {
        // Chart still needs calibrating
        _chartData.state = -1;
    }
}

void updateWindowTitle()
{
    const char calibrate[] = "Right-click on this chart to set POSITION";
    const char cancel[] = "or middle-click to cancel";

    char title[512];
    char titleStart[256];

    if (*_tagText == '\0') {
        strcpy(titleStart, ProgramName);
    }
    else if (*_follow.callsign != '\0') {
        sprintf(titleStart, "Following %s", _tagText);
    }
    else {
        strcpy(titleStart, _tagText);
    }

    if (*_settings.chart == '\0') {
        strcpy(title, titleStart);
    }
    else {
        char* name = strrchr(_settings.chart, '\\');
        if (name) {
            name++;
        }
        else {
            name = _settings.chart;
        }

        char chart[256];
        strcpy(chart, name);
        char* ext = strrchr(chart, '.');
        if (ext) {
            *ext = '\0';
        }

        if (*_aiTitle == '\0') {
            sprintf(title, "%s - %s", titleStart, chart);
        }
        else {
            strcpy(title, _aiTitle);
        }
    }

    _titleState = _chartData.state;

    switch (_chartData.state) {
    case -1:
        al_set_window_title(_display, title);
        showMessage("This chart needs calibrating.\nSelect 'Re-calibrate' from the right-click menu.", false);
        break;

    case 0:
        sprintf(title, "%s - %s 1 %s", ProgramName, calibrate, cancel);
        al_set_window_title(_display, title);
        if (showClipboardMessage(false, true) == IDCANCEL) {
            cancelCalibration();
        }
        break;

    case 1:
        sprintf(title, "%s - %s 2 %s", ProgramName, calibrate, cancel);
        al_set_window_title(_display, title);
        break;

    default:
        if (_showCalibration && _clickedLoc.lat != MAXINT) {
            char str[64];
            locationToString(&_clickedLoc, str);
            sprintf(title, "%s   %s", title, str);
            al_set_window_title(_display, title);
        }
        else {
            al_set_window_title(_display, title);
        }
        break;
    }
}

/// <summary>
/// Initialise global variables
/// </summary>
void initVars()
{
    _chart.bmp = NULL;
    _view.bmp = NULL;
    _aircraft.bmp = NULL;
    _aircraft.smallBmp = NULL;
    _aircraft.otherBmp = NULL;
    _aircraft.smallOtherBmp = NULL;
    _aircraft.helicopterOtherBmp = NULL;
    _aircraft.gliderOtherBmp = NULL;
    _aircraft.largeOtherBmp = NULL;
    _aircraft.jetOtherBmp = NULL;
    _aircraft.militaryOtherBmp = NULL;
    _aircraft.turbopropOtherBmp = NULL;
    _aircraft.vehicleBmp = NULL;
    _aircraft.airportBmp = NULL;
    _aircraft.waypointBmp = NULL;
    _aircraftLabel.bmp = NULL;
    _aircraftLabelBmpCopy = NULL;
    _marker.bmp = NULL;
    _ring.bmp = NULL;
    _arrow.bmp = NULL;
    _windInfo.bmp = NULL;
    _windInfoCopy.bmp = NULL;

    _mouseData.dragging = false;
    _mouseData.dragX = 0;
    _mouseData.dragY = 0;

    *_settings.chart = '\0';
    *_labelText = '\0';
    *_tagText = '\0';

    // Default window position and size if no settings saved
    _settings.x = 360;
    _settings.y = 100;
    _settings.width = 1200;
    _settings.height = 800;
    _settings.framesPerSec = 0;

    _chartData.state = -1;
    _titleState = -2;
    _clickedLoc.lat = MAXINT;
    _clickedPos.x = -1;
    _clipboardPos.x = -1;
    *_closestAircraft = '\0';
}

/// <summary>
/// Finds closest aircraft to where mouse was right-clicked
/// </summary>
void findClosestAircraft(Locn* loc)
{
    *_closestAircraft = '\0';

    double closest = MAXINT;
    for (int i = 0; i < _otherAircraftCount; i++) {
        // Exclude self
        if (strcmp(_tagText, _otherTag[i].tagText) == 0) {
            continue;
        }

        // Exclude static aircraft
        if (strcmp(_otherAircraftData[i].callsign, "ASXGSA") == 0 || strcmp(_otherAircraftData[i].callsign, "AS-MTP2") == 0) {
            continue;
        }

        double distance = greatCircleDistance(loc, &_otherAircraftData[i].loc);
        if (closest > distance) {
            strcpy(_closestAircraft, _otherAircraftData[i].callsign);
            closest = distance;
        }
    }
}

HMENU createMenu()
{
    HMENU rotateMenu = CreatePopupMenu();
    AppendMenu(rotateMenu, MF_STRING, MENU_ROTATE_LEFT_20, "-20°");
    AppendMenu(rotateMenu, MF_STRING, MENU_ROTATE_RIGHT_20, "+20°");
    AppendMenu(rotateMenu, MF_STRING, MENU_ROTATE_LEFT_90, "-90°");
    AppendMenu(rotateMenu, MF_STRING, MENU_ROTATE_RIGHT_90, "+90°");

    HMENU teleportMenu = CreatePopupMenu();
    AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_HERE_GROUND, "To here (on ground)");
    AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_HERE, "To here (same alt and speed)");
    AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_HERE_AIR, "To here (2000ft @ 150kn)");
    AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_CLIPBOARD, "To clipboard location");
    AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_RESTORE_LOCATION, "Restore location");
    AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_SAVE_LOCATION, "Save location");

    char menuText[64];
    if (*_follow.callsign == '\0') {
        displayToChartPos(_teleport.pos.x, _teleport.pos.y, &_clickedPos);
        chartPosToLocation(_clickedPos.x, _clickedPos.y, &_teleport.loc);
        findClosestAircraft(&_teleport.loc);
        sprintf(menuText, "Follow %s", _closestAircraft);
        AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_FOLLOW, menuText);
    }
    else {
        sprintf(menuText, "Unfollow %s", _follow.callsign);
        AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_FOLLOW, menuText);
    }

    HMENU menu = CreatePopupMenu();
    AppendMenu(menu, MF_STRING, MENU_LOAD_CHART, "Load chart");
    AppendMenu(menu, MF_STRING, MENU_LOAD_CLOSEST, "Load closest");
    AppendMenu(menu, MF_STRING, MENU_LOCATE_AIRCRAFT, "Locate aircraft");
    AppendMenu(menu, MF_STRING, MENU_FIX_CRASH, "Fix crash");
    AppendMenu(menu, MF_STRING, MENU_ROTATE_AGAIN, "Rotate again");
    AppendMenu(menu, MF_STRING | MF_POPUP, (UINT_PTR)rotateMenu, "Rotate aircraft");
    AppendMenu(menu, MF_STRING | MF_POPUP, (UINT_PTR)teleportMenu, "Teleport aircraft");
    AppendMenu(menu, MF_STRING, MENU_MAX_RANGE, "Maximum Range");
    AppendMenu(menu, MF_STRING, MENU_SHOW_TAGS, "Show tags");

    if (_showAi) {
        AppendMenu(menu, MF_STRING, MENU_SHOW_AI_PHOTOS, "Show AI photos in browser");
        AppendMenu(menu, MF_STRING, MENU_CLEAR_AI_TRAILS, "Clear AI trails");
    }

    AppendMenu(menu, MF_STRING, MENU_SHOW_CALIBRATION, "Show calibration");
    AppendMenu(menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(menu, MF_STRING, MENU_RECALIBRATE, "Re-calibrate chart");

    return menu;
}

int enabledState(bool isEnabled)
{
    if (isEnabled) {
        return MF_ENABLED;
    }
    else {
        return MF_DISABLED;
    }
}

int checkedState(bool isChecked)
{
    if (isChecked) {
        return MF_CHECKED;
    }
    else {
        return MF_UNCHECKED;
    }
}

/// <summary>
/// Determine which menu items should be enabled and which items should be checked
/// </summary>
void updateMenu(HMENU menu)
{
    bool active = _aircraftData.loc.lat != MAXINT;
    bool calibrated = _chartData.state == 2;

    EnableMenuItem(menu, MENU_LOAD_CLOSEST, enabledState(active));
    EnableMenuItem(menu, MENU_LOCATE_AIRCRAFT, enabledState(active && calibrated));
    EnableMenuItem(menu, MENU_FIX_CRASH, enabledState(active && calibrated && !_teleport.inProgress));
    EnableMenuItem(menu, MENU_ROTATE_AGAIN, enabledState(active && !_teleport.inProgress && _lastRotate != MAXINT));
    EnableMenuItem(menu, MENU_ROTATE_LEFT_20, enabledState(active && !_teleport.inProgress));
    EnableMenuItem(menu, MENU_ROTATE_RIGHT_20, enabledState(active && !_teleport.inProgress));
    EnableMenuItem(menu, MENU_ROTATE_LEFT_90, enabledState(active && !_teleport.inProgress));
    EnableMenuItem(menu, MENU_ROTATE_RIGHT_90, enabledState(active && !_teleport.inProgress));
    EnableMenuItem(menu, MENU_TELEPORT_HERE_GROUND, enabledState(active && calibrated && !_teleport.inProgress));
    EnableMenuItem(menu, MENU_TELEPORT_HERE, enabledState(active && calibrated && !_teleport.inProgress));
    EnableMenuItem(menu, MENU_TELEPORT_HERE_AIR, enabledState(active && calibrated && !_teleport.inProgress));
    EnableMenuItem(menu, MENU_TELEPORT_CLIPBOARD, enabledState(active && !_teleport.inProgress));
    EnableMenuItem(menu, MENU_TELEPORT_RESTORE_LOCATION, enabledState(active && !_snapshot.save && _snapshot.loc.lat != MAXINT));
    EnableMenuItem(menu, MENU_TELEPORT_SAVE_LOCATION, enabledState(active && !_snapshot.save));
    EnableMenuItem(menu, MENU_TELEPORT_FOLLOW, enabledState(active && !_follow.inProgress && *_closestAircraft != '\0'));
    EnableMenuItem(menu, MENU_MAX_RANGE, enabledState(active && calibrated));
    EnableMenuItem(menu, MENU_SHOW_TAGS, enabledState(calibrated));

    if (_showAi) {
        EnableMenuItem(menu, MENU_SHOW_AI_PHOTOS, MF_ENABLED);
        CheckMenuItem(menu, MENU_SHOW_AI_PHOTOS, checkedState(_showAiPhotos));
        EnableMenuItem(menu, MENU_CLEAR_AI_TRAILS, MF_ENABLED);
    }

    EnableMenuItem(menu, MENU_SHOW_CALIBRATION, enabledState(calibrated));

    CheckMenuItem(menu, MENU_MAX_RANGE, checkedState(_maxRange));
    CheckMenuItem(menu, MENU_SHOW_TAGS, checkedState(_showTags));
    CheckMenuItem(menu, MENU_SHOW_CALIBRATION, checkedState(_showCalibration));
}

bool windowCallback(ALLEGRO_DISPLAY* display, UINT message,
    WPARAM wparam, LPARAM lparam, LRESULT* result, void* userdata)
{
    // Activate context menu on right button release
    if (message == WM_RBUTTONUP && _teleport.pos.x != MAXINT) {
        POINT menuPos;
        menuPos.x = _teleport.pos.x;
        menuPos.y = _teleport.pos.y;
        ClientToScreen(_displayWindow, &menuPos);

        // FS2020 messes up cursor pos if it was active immediately before
        // this right click so fix it here.
        SetCursorPos(menuPos.x, menuPos.y);

        HMENU menu = createMenu();
        updateMenu(menu);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, menuPos.x, menuPos.y, 0, _displayWindow, NULL);
        DestroyMenu(menu);
    }
    else if (message == WM_COMMAND) {
        // Menu item must be actioned on main thread to avoid contention
        _menuItem = LOWORD(wparam);
    }

    return false;
}

void rotateAircraft(double adjustAngle)
{
    _lastRotate = adjustAngle;
    _teleport.loc.lat = _aircraftData.loc.lat;
    _teleport.loc.lon = _aircraftData.loc.lon;
    _teleport.heading = _aircraftData.heading + _lastRotate;
    _teleport.inProgress = true;
}

void actionMenuItem()
{
    switch (_menuItem) {

    case MENU_LOAD_CHART:
    {
        newChart();
        _showCalibration = false;
        break;
    }
    case MENU_LOAD_CLOSEST:
    {
        closestChart();
        _showCalibration = false;
        break;
    }
    case MENU_LOCATE_AIRCRAFT:
    {
        // Centre chart on current aircraft location
        Position pos;
        locationToChartPos(&_aircraftData.loc, &pos);
        _chart.x = pos.x;
        _chart.y = pos.y;
        break;
    }
    case MENU_FIX_CRASH:
    {
        // Centre chart on current aircraft location and set aircraft to 2000ft @ 150kn
        Position pos;
        locationToChartPos(&_aircraftData.loc, &pos);
        _chart.x = pos.x;
        _chart.y = pos.y;
        chartPosToLocation(pos.x, pos.y, &_teleport.loc);
        _teleport.heading = _aircraftData.heading;
        _teleport.alt = 2000;
        _teleport.speed = 150;
        _teleport.setAltSpeed = true;
        _teleport.inProgress = true;
        break;
    }
    case MENU_ROTATE_AGAIN:
    {
        rotateAircraft(_lastRotate);
        break;
    }
    case MENU_ROTATE_LEFT_20:
    {
        rotateAircraft(-20);
        break;
    }
    case MENU_ROTATE_RIGHT_20:
    {
        rotateAircraft(20);
        break;
    }
    case MENU_ROTATE_LEFT_90:
    {
        rotateAircraft(-90);
        break;
    }
    case MENU_ROTATE_RIGHT_90:
    {
        rotateAircraft(90);
        break;
    }
    case MENU_TELEPORT_HERE_GROUND:
    {
        // Reset altitude and speed
        displayToChartPos(_teleport.pos.x, _teleport.pos.y, &_clickedPos);
        chartPosToLocation(_clickedPos.x, _clickedPos.y, &_teleport.loc);
        _teleport.heading = _aircraftData.heading;
        _teleport.alt = 6;
        _teleport.speed = 0;
        _teleport.setAltSpeed = true;
        _teleport.inProgress = true;
        break;
    }
    case MENU_TELEPORT_HERE:
    {
        // Keeps current altitude and speed
        displayToChartPos(_teleport.pos.x, _teleport.pos.y, &_clickedPos);
        chartPosToLocation(_clickedPos.x, _clickedPos.y, &_teleport.loc);
        _teleport.heading = _aircraftData.heading;
        _teleport.setAltSpeed = false;
        _teleport.inProgress = true;
        break;
    }
    case MENU_TELEPORT_HERE_AIR:
    {
        // Sets altitude to 2000ft and speed to 150kn
        displayToChartPos(_teleport.pos.x, _teleport.pos.y, &_clickedPos);
        chartPosToLocation(_clickedPos.x, _clickedPos.y, &_teleport.loc);
        _teleport.heading = _aircraftData.heading;
        _teleport.alt = 2000;
        _teleport.speed = 150;
        _teleport.setAltSpeed = true;
        _teleport.inProgress = true;
        break;
    }
    case MENU_TELEPORT_CLIPBOARD:
    {
        getClipboardLocation(&_teleport.loc);
        if (_teleport.loc.lat == MAXINT) {
            showClipboardMessage(true);
        }
        else {
            _teleport.heading = _aircraftData.heading;
            _teleport.alt = 6;
            _teleport.speed = 0;
            _teleport.setAltSpeed = true;
            _teleport.inProgress = true;
        }
        break;
    }
    case MENU_TELEPORT_RESTORE_LOCATION:
    {
        _snapshot.restore = true;
        break;
    }
    case MENU_TELEPORT_SAVE_LOCATION:
    {
        _snapshot.save = true;
        break;
    }
    case MENU_TELEPORT_FOLLOW:
    {
        if (*_follow.callsign == '\0') {
            // Start following
            strcpy(_follow.callsign, _closestAircraft);
            strcpy(_follow.ownTag, _tagText);
            _follow.ownWingSpan = _aircraftData.wingSpan;
        }
        else {
            // Stop following
            *_follow.callsign = '\0';
        }
        _follow.inProgress = true;
        break;
    }
    case MENU_MAX_RANGE:
    {
        _maxRange = !_maxRange;
        _range = _maxRange ? MAX_RANGE : AIRCRAFT_RANGE;
        break;
    }
    case MENU_SHOW_TAGS:
    {
        _showTags = !_showTags;
        break;
    }
    case MENU_SHOW_AI_PHOTOS:
    {
        _showAiPhotos = !_showAiPhotos;
        break;
    }
    case MENU_CLEAR_AI_TRAILS:
    {
        if (!_watchInProgress) {
            strcpy(_watchCallsign, "clear");
            _watchInProgress = true;
        }
        break;
    }
    case MENU_SHOW_CALIBRATION:
    {
        _showCalibration = !_showCalibration;
        clearCustomPoints();
        break;
    }
    case MENU_RECALIBRATE:
    {
        _chartData.state = 0;
        _showCalibration = false;
        break;
    }
    }
}

/// <summary>
/// Initialise Allegro
/// </summary>
bool init()
{
    if (!al_init()) {
        printf("Failed to initialise Allegro\n");
        return false;
    }

    if (!al_init_primitives_addon()) {
        printf("Failed to initialise primitives\n");
        return false;
    }

    if (!al_init_font_addon()) {
        printf("Failed to initialise font\n");
        return false;
    }

    if (!al_init_image_addon()) {
        printf("Failed to initialise image\n");
        return false;
    }

    if (!al_install_keyboard()) {
        printf("Failed to initialise keyboard\n");
        return false;
    }

    if (!al_install_mouse()) {
        printf("Failed to initialise mouse\n");
        return false;
    }

    if (!(_eventQueue = al_create_event_queue())) {
        printf("Failed to create event queue\n");
        return false;
    }

    if (!(_font = al_create_builtin_font())) {
        printf("Failed to create font\n");
        return false;
    }

    al_set_new_window_title("FlightSim Charts");

    // Use existing desktop resolution/refresh rate
    int flags = ALLEGRO_WINDOWED | ALLEGRO_RESIZABLE;
    al_set_new_display_flags(flags);

    // Turn on vsync
    al_set_new_display_option(ALLEGRO_VSYNC, 1, ALLEGRO_REQUIRE);

    // Create window
    if ((_display = al_create_display(_settings.width, _settings.height)) == NULL) {
        printf("Failed to create display\n");
        return false;
    }

    // Make sure window isn't minimised
    _displayWindow = al_get_win_window_handle(_display);
    ShowWindow(_displayWindow, SW_SHOWNORMAL);

    // Make sure window is within monitor bounds
    bool visible = false;
    ALLEGRO_MONITOR_INFO monitorInfo;
    int numMonitors = al_get_num_video_adapters();
    for (int i = 0; i < numMonitors; i++) {
        al_get_monitor_info(i, &monitorInfo);

        if (_settings.x >= monitorInfo.x1 && _settings.x < monitorInfo.x2
            && _settings.y >= monitorInfo.y1 && _settings.y < monitorInfo.y2)
        {
            visible = true;
            break;
        }
    }

    if (!visible && numMonitors > 0) {
        _settings.x = monitorInfo.x1;
        _settings.y = monitorInfo.y1;
    }

    // Position window
    al_set_window_position(_display, _settings.x, _settings.y);

    _displayWidth = al_get_display_width(_display);
    _displayHeight = al_get_display_height(_display);

    al_register_event_source(_eventQueue, al_get_keyboard_event_source());
    al_register_event_source(_eventQueue, al_get_mouse_event_source());
    al_register_event_source(_eventQueue, al_get_display_event_source(_display));

    if (!(_timer = al_create_timer(1.0 / _settings.framesPerSec))) {
        printf("Failed to create timer\n");
        return false;
    }

    al_register_event_source(_eventQueue, al_get_timer_event_source(_timer));

    // Listen for menu events
    if (al_win_add_window_callback(_display, &windowCallback, NULL)) {
        _menuCallback = true;
    }
    else {
        printf("Failed to add window callback\n");
        return false;
    }

    return true;
}

void cleanupBitmap(ALLEGRO_BITMAP* bmp)
{
    if (bmp != NULL) {
        al_destroy_bitmap(bmp);
    }
}

/// <summary>
/// Cleanup Allegro
/// </summary>
void cleanup()
{
    cleanupBitmap(_chart.bmp);
    cleanupBitmap(_view.bmp);
    cleanupBitmap(_aircraft.bmp);
    cleanupBitmap(_aircraft.smallBmp);
    cleanupBitmap(_aircraft.otherBmp);
    cleanupBitmap(_aircraft.smallOtherBmp);
    cleanupBitmap(_aircraft.helicopterOtherBmp);
    cleanupBitmap(_aircraft.gliderOtherBmp);
    cleanupBitmap(_aircraft.vehicleBmp);
    cleanupBitmap(_aircraft.largeOtherBmp);
    cleanupBitmap(_aircraft.jetOtherBmp);
    cleanupBitmap(_aircraft.militaryOtherBmp);
    cleanupBitmap(_aircraft.turbopropOtherBmp);
    cleanupBitmap(_aircraft.airportBmp);
    cleanupBitmap(_aircraft.waypointBmp);
    cleanupBitmap(_aircraftLabel.bmp);
    cleanupBitmap(_aircraftLabelBmpCopy);
    cleanupBitmap(_marker.bmp);
    cleanupBitmap(_ring.bmp);
    cleanupBitmap(_arrow.bmp);
    cleanupBitmap(_windInfo.bmp);
    cleanupBitmap(_windInfoCopy.bmp);

    // Clenaup tags
    for (int i = 0; i < _tagCount; i++) {
        cleanupBitmap(_otherTag[i].tag.bmp);
    }

    for (int i = 0; i < _aiAircraftCount; i++) {
        ALLEGRO_BITMAP* bmp = _aiAircraft[i].tagData.tag.bmp;
        _aiAircraft[i].tagData.tag.bmp = NULL;
        cleanupBitmap(bmp);

        bmp = _aiAircraft[i].tagData.moreTag.bmp;
        _aiAircraft[i].tagData.moreTag.bmp = NULL;
        cleanupBitmap(bmp);
    }

    for (int i = 0; i < _aiFixedCount; i++) {
        ALLEGRO_BITMAP* bmp = _aiFixed[i].tagData.tag.bmp;
        _aiFixed[i].tagData.tag.bmp = NULL;
        cleanupBitmap(bmp);
    }

    if (_timer) {
        al_destroy_timer(_timer);
    }

    if (_eventQueue) {
        al_destroy_event_queue(_eventQueue);
    }

    if (_font) {
        al_destroy_font(_font);
    }

    if (_menuCallback) {
        al_win_remove_window_callback(_display, &windowCallback, NULL);
    }

    if (_display) {
        al_destroy_display(_display);
        al_inhibit_screensaver(false);
    }
}

void createTagText(char* callsign, char* model, char* tagText)
{
    char* modelStart = strchr(model, ' ');
    if (!modelStart) {
        modelStart = strrchr(model, '_');
    }
    if (!modelStart) {
        if (strlen(model) < 6 && strcmp(model, "GRND") != 0 && model[0] != '$') {
            sprintf(tagText, "%s %s", callsign, model);
        }
        else {
            strcpy(tagText, callsign);
        }
        return;
    }

    modelStart++;
    char* modelEnd = strchr(modelStart, '.');
    if (modelEnd) {
        *modelEnd = '\0';
    }

    sprintf(tagText, "%s %s", callsign, modelStart);
}

/// <summary>
/// Centre map and zoom fully out
/// </summary>
void resetMap()
{
    _chart.x = _chart.width / 2.0;
    _chart.y = _chart.height / 2.0;

    al_get_mouse_state(&_mouse);
    _mouseStartZ = -(_mouse.z + 999);
    _view.scale = 0;
}

bool initChart()
{
    if (*_settings.chart == '\0') {
        return false;
    }

    cleanupBitmap(_chart.bmp);
    _chart.bmp = NULL;

    _chart.bmp = al_load_bitmap(_settings.chart);
    if (!_chart.bmp) {
        char msg[256];
        sprintf(msg, "Failed to load chart %s\n", _settings.chart);
        showMessage(msg, true);
        *_settings.chart = '\0';
        return false;
    }

    _chart.width = al_get_bitmap_width(_chart.bmp);
    _chart.height = al_get_bitmap_height(_chart.bmp);

    // Centre map and zoom fully out
    resetMap();

    _titleState = -2;
    _chartData.state = -1;
    loadCalibrationData(&_chartData);

    return true;
}

bool initAircraft()
{
    _aircraft.bmp = al_load_bitmap(AircraftFile);
    if (!_aircraft.bmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", AircraftFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.otherBmp = al_load_bitmap(AircraftOtherFile);
    if (!_aircraft.otherBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", AircraftOtherFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.halfWidth = al_get_bitmap_width(_aircraft.bmp) / 2;
    _aircraft.halfHeight = al_get_bitmap_height(_aircraft.bmp) / 2;

    _aircraft.smallBmp = al_load_bitmap(AircraftSmallFile);
    if (!_aircraft.smallBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", AircraftSmallFile);
        return false;
    }

    _aircraft.smallOtherBmp = al_load_bitmap(AircraftSmallOtherFile);
    if (!_aircraft.smallOtherBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", AircraftSmallOtherFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.helicopterOtherBmp = al_load_bitmap(HelicopterOtherFile);
    if (!_aircraft.helicopterOtherBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", HelicopterOtherFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.gliderOtherBmp = al_load_bitmap(GliderOtherFile);
    if (!_aircraft.gliderOtherBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", GliderOtherFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.largeOtherBmp = al_load_bitmap(LargeOtherFile);
    if (!_aircraft.largeOtherBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", LargeOtherFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.jetOtherBmp = al_load_bitmap(JetOtherFile);
    if (!_aircraft.jetOtherBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", JetOtherFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.militaryOtherBmp = al_load_bitmap(MilitaryOtherFile);
    if (!_aircraft.militaryOtherBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", MilitaryOtherFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.turbopropOtherBmp = al_load_bitmap(TurbopropOtherFile);
    if (!_aircraft.turbopropOtherBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", TurbopropOtherFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.vehicleBmp = al_load_bitmap(VehicleFile);
    if (!_aircraft.vehicleBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", VehicleFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.airportBmp = al_load_bitmap(AirportFile);
    if (!_aircraft.airportBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", AirportFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.waypointBmp = al_load_bitmap(WaypointFile);
    if (!_aircraft.waypointBmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", WaypointFile);
        showMessage(msg, true);
        return false;
    }

    _aircraft.smallHalfWidth = al_get_bitmap_width(_aircraft.smallBmp) / 2;
    _aircraft.smallHalfHeight = al_get_bitmap_height(_aircraft.smallBmp) / 2;

    // Use this bitmap for the window icon
    al_set_display_icon(_display, _aircraft.bmp);

    // Label for aircraft info when it is off the screen
    _aircraftLabel.bmp = al_load_bitmap(LabelFile);
    if (!_aircraftLabel.bmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", LabelFile);
        showMessage(msg, true);
        return false;
    }

    _aircraftLabel.width = al_get_bitmap_width(_aircraftLabel.bmp);
    _aircraftLabel.height = al_get_bitmap_height(_aircraftLabel.bmp);

    // Writing text uses excessive GPU so make a copy of the label as a
    // back buffer then we only need to update it when the text changes.
    _aircraftLabelBmpCopy = al_create_bitmap(_aircraftLabel.width, _aircraftLabel.height);

    return true;
}

bool initMarker()
{
    _marker.bmp = al_load_bitmap(MarkerFile);
    if (!_marker.bmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", MarkerFile);
        showMessage(msg, true);
        return false;
    }

    _marker.width = al_get_bitmap_width(_marker.bmp);
    _marker.height = al_get_bitmap_height(_marker.bmp);
    _marker.scale = 0.3;

    return true;
}

bool initRing()
{
    _ring.bmp = al_load_bitmap(RingFile);
    if (!_ring.bmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", RingFile);
        showMessage(msg, true);
        return false;
    }

    _ring.width = al_get_bitmap_width(_ring.bmp);
    _ring.height = al_get_bitmap_height(_ring.bmp);
    _ring.scale = 0.3;

    return true;
}

bool initWind()
{
    _arrow.bmp = al_load_bitmap(ArrowFile);
    if (!_arrow.bmp) {
        char msg[256];
        sprintf(msg, "Missing file: %s\n", ArrowFile);
        showMessage(msg, true);
        return false;
    }

    _arrow.width = al_get_bitmap_width(_arrow.bmp);
    _arrow.height = al_get_bitmap_height(_arrow.bmp);
    _arrow.x = _arrow.width / 2;
    _arrow.y = _arrow.height / 2;

    // Draw max wind info required
    _windInfo.width = 58;
    _windInfo.height = 22;
    _windInfo.bmp = al_create_bitmap(_windInfo.width, _windInfo.height);

    // fill with background colour
    al_set_target_bitmap(_windInfo.bmp);
    al_clear_to_color(al_map_rgb(0xff, 0xff, 0xb2));
    al_set_target_backbuffer(_display);

    _windInfoCopy.bmp = al_create_bitmap(_windInfo.width, _windInfo.height);

    return true;
}

void drawOwnAircraft()
{
    if (_aircraft.x == MAXINT) {
        return;
    }

    Position pos;
    chartToDisplayPos(_aircraft.x, _aircraft.y, &pos);

    ALLEGRO_BITMAP* bmp;
    int halfWidth;
    int halfHeight;

    if (_aircraftData.wingSpan > WINGSPAN_SMALL) {
        bmp = _aircraft.bmp;
        halfWidth = _aircraft.halfWidth;
        halfHeight = _aircraft.halfHeight;
    }
    else {
        bmp = _aircraft.smallBmp;
        halfWidth = _aircraft.smallHalfWidth;
        halfHeight = _aircraft.smallHalfHeight;
    }

    if (_view.scale > 0.2) {
        _aircraft.scale = 0.2;
    }
    else {
        _aircraft.scale = 0.14;
    }

    al_draw_scaled_rotated_bitmap(bmp, halfWidth, halfHeight, pos.x, pos.y, _aircraft.scale, _aircraft.scale, _aircraftData.heading * DegreesToRadians, 0);

    if (*_labelText != '\0') {
        // Show aircraft label
        if (_aircraftLabel.x != 0) {
            pos.x += _aircraftLabel.x * halfWidth * _aircraft.scale * 1.2;
            pos.y -= _aircraftLabel.height / 2;
            if (_aircraftLabel.x == -1) {
                pos.x -= _aircraftLabel.width;
            }
        }
        else {
            pos.y += _aircraftLabel.y * halfHeight * _aircraft.scale * 1.2;
            pos.x -= _aircraftLabel.width / 2;
            if (_aircraftLabel.y == -1) {
                pos.y -= _aircraftLabel.height;
            }
            else if (_aircraftLabel.y == 1) {
                pos.y += 5;
            }
        }

        al_draw_bitmap(_aircraftLabelBmpCopy, pos.x, pos.y, 0);
    }
}

void getIconData(char *model, int altitude, IconData* iconData, int wingSpan)
{
    iconData->halfWidth = _aircraft.smallHalfWidth;
    iconData->halfHeight = _aircraft.smallHalfHeight;

    if (strlen(model) > 5) {
        if (wingSpan <= WINGSPAN_SMALL) {
            iconData->bmp = _aircraft.smallOtherBmp;
            return;
        }

        iconData->halfWidth = _aircraft.smallHalfWidth;
        iconData->halfHeight = _aircraft.smallHalfHeight;
        iconData->bmp = _aircraft.otherBmp;
        return;
    }

    if (strcmp(model, "GLID") == 0) {
        iconData->bmp = _aircraft.gliderOtherBmp;
        return;
    }

    if (strcmp(model, "GRND") == 0 && altitude < 50) {
        iconData->bmp = _aircraft.vehicleBmp;
        return;
    }

    char prefix[8];
    sprintf(prefix, "_%.3s_", model);

    if (strstr(Helis, prefix) != NULL) {
        iconData->bmp = _aircraft.helicopterOtherBmp;
        return;
    }

    if (strstr(Turboprops, prefix) != NULL) {
        iconData->bmp = _aircraft.turbopropOtherBmp;
        return;
    }

    iconData->halfWidth = _aircraft.halfWidth;
    iconData->halfHeight = _aircraft.halfHeight;

    if (strstr(Airliners, prefix) != NULL) {
        iconData->bmp = _aircraft.otherBmp;
        return;
    }

    if (strstr(Large_Airliners, prefix) != NULL) {
        iconData->bmp = _aircraft.largeOtherBmp;
        return;
    }

    if (strstr(Jets, prefix) != NULL) {
        iconData->bmp = _aircraft.jetOtherBmp;
        return;
    }

    if (strstr(Military, prefix) != NULL) {
        iconData->bmp = _aircraft.militaryOtherBmp;
        return;
    }

    // Default
    if (wingSpan > WINGSPAN_SMALL) {
iconData->bmp = _aircraft.otherBmp;
return;
    }

    iconData->bmp = _aircraft.smallOtherBmp;
    iconData->halfWidth = _aircraft.smallHalfWidth;
    iconData->halfHeight = _aircraft.smallHalfHeight;
}

void drawOtherAircraft()
{
    if (_otherAircraftCount == 0) {
        return;
    }

    Position displayPos1;
    displayToChartPos(0, 0, &displayPos1);

    Position displayPos2;
    displayToChartPos(_displayWidth - 1, _displayHeight - 1, &displayPos2);

    // Expand the display slightly so we can draw partly off the edge
    double border = 15.0 / _view.scale;
    displayPos1.x -= border;
    displayPos1.y -= border;
    displayPos2.x += border;
    displayPos2.y += border;

    if (_view.scale > 0.2) {
        _aircraft.scale = 0.2;
    }
    else {
        _aircraft.scale = 0.14;
    }

    Position pos;
    for (int i = 0; i < _otherAircraftCount; i++) {
        // Exclude self
        if (strcmp(_tagText, _otherTag[i].tagText) == 0) {
            continue;
        }

        // Exclude static aircraft
        if (strcmp(_otherAircraftData[i].callsign, "ASXGSA") == 0 || strcmp(_otherAircraftData[i].callsign, "AS-MTP2") == 0) {
            continue;
        }

        if (*_follow.callsign != '\0' && strcmp(_follow.ownTag, _otherTag[i].tagText) == 0) {
            continue;
        }

        // Don't draw other aircraft if outside the display
        if (drawOther(&displayPos1, &displayPos2, &_otherAircraftData[i].loc, &pos)) {
            IconData iconData;
            getIconData(_otherAircraftData[i].model, _otherAircraftData[i].alt, &iconData, _otherAircraftData[i].wingSpan);

            al_draw_scaled_rotated_bitmap(iconData.bmp, iconData.halfWidth, iconData.halfHeight, pos.x, pos.y, _aircraft.scale, _aircraft.scale, _otherAircraftData[i].heading * DegreesToRadians, 0);

            if (_showTags) {
                if (_otherTag[i].tag.bmp != NULL) {
                    // Draw tag to right of aircraft
                    al_draw_bitmap(_otherTag[i].tag.bmp, pos.x + 1 + iconData.halfHeight * 1.5 * _aircraft.scale, pos.y - _otherTag[i].tag.height / 2.0, 0);
                }

                if (_otherTag[i].moreTag.bmp != NULL) {
                    // Draw second tag with alt and speed
                    al_draw_bitmap(_otherTag[i].moreTag.bmp, pos.x + 1 + iconData.halfHeight * 1.5 * _aircraft.scale, pos.y + _otherTag[i].tag.height / 2.0, 0);
                }
            }
        }
    }
}

void drawAiObjects()
{
    if (_connected && _aiFixedCount == 0) {
        return;
    }

    Position displayPos1;
    displayToChartPos(0, 0, &displayPos1);

    Position displayPos2;
    displayToChartPos(_displayWidth - 1, _displayHeight - 1, &displayPos2);

    // Expand the display slightly so we can draw partly off the edge
    double border = 15.0 / _view.scale;
    displayPos1.x -= border;
    displayPos1.y -= border;
    displayPos2.x += border;
    displayPos2.y += border;

    if (_view.scale > 0.2) {
        _aircraft.scale = 0.16;
    }
    else {
        _aircraft.scale = 0.11;
    }

    Position pos;

    // Draw trails first so other objects are drawn on top
    int last = -1;
    ALLEGRO_COLOR colour = al_map_rgb(0xb0, 0x60, 0x20);

    for (int i = 0; i < 3; i++) {
        if (_aiTrail[i].count > 0) {
            last = i;

            for (int j = 1; j < _aiTrail[i].count; j++) {
                Position fromPos;
                Position toPos;

                if (drawOther(&displayPos1, &displayPos2, &_aiTrail[i].loc[j - 1], &fromPos, true) &&
                    drawOther(&displayPos1, &displayPos2, &_aiTrail[i].loc[j], &toPos, true))
                {
                    al_draw_line(fromPos.x, fromPos.y, toPos.x, toPos.y, colour, 2);
                }
            }
        }
    }

    if (last == -1) {
        *_aiTitle = '\0';
    }
    else {
        // Show info in window title
        if (strncmp(_aiTitle, _aiTrail[last].callsign, strlen(_aiTrail[last].callsign)) != 0) {
            sprintf(_aiTitle, "%s   %s   %s", _aiTrail[last].callsign, _aiTrail[last].airline, _aiTrail[last].modelType);
            if (strcmp(_aiTrail[last].fromAirport, "Unknown") != 0) {
                char moreTitle[256];
                sprintf(moreTitle, "    %s", _aiTrail[last].fromAirport);
                strcat(_aiTitle, moreTitle);
            }
            if (strcmp(_aiTrail[last].toAirport, "Unknown") != 0) {
                char moreTitle[256];
                sprintf(moreTitle, "    %s", _aiTrail[last].toAirport);
                strcat(_aiTitle, moreTitle);
            }
            _titleState = -2;
        }
    }

    // If we aren't currently connected draw the AI aircraft as they won't be injected
    if (!_connected) {
        char moreTagText[68];

        for (int i = 0; i < _aiAircraftCount; i++) {
            // Don't draw aircraft if outside the display
            if (drawOther(&displayPos1, &displayPos2, &_aiAircraft[i].loc, &pos)) {
                IconData iconData;
                getIconData(_aiAircraft[i].model, _aiAircraft[i].alt, &iconData, 0);

                al_draw_scaled_rotated_bitmap(iconData.bmp, iconData.halfWidth, iconData.halfHeight, pos.x, pos.y, _aircraft.scale, _aircraft.scale, _aiAircraft[i].heading * DegreesToRadians, 0);

                if (_showTags) {
                    if (_aiAircraft[i].tagData.tag.bmp == NULL) {
                        createTagBitmap(_aiAircraft[i].tagData.tagText, &_aiAircraft[i].tagData.tag);
                    }

                    // Draw tag to right of aircraft
                    al_draw_bitmap(_aiAircraft[i].tagData.tag.bmp, pos.x + 1 + iconData.halfHeight * _aircraft.scale, pos.y - _aiAircraft[i].tagData.tag.height / 2.0, 0);

                    // Add a second tag with alt and speed
                    sprintf(moreTagText, "%.0lf %.0lf", _aiAircraft[i].alt, _aiAircraft[i].speed);
                    if (strcmp(_aiAircraft[i].tagData.moreTagText, moreTagText) != 0) {
                        strcpy(_aiAircraft[i].tagData.moreTagText, moreTagText);
                        cleanupBitmap(_aiAircraft[i].tagData.moreTag.bmp);
                        _aiAircraft[i].tagData.moreTag.bmp = NULL;
                    }

                    if (_aiAircraft[i].tagData.moreTag.bmp == NULL) {
                        createTagBitmap(_aiAircraft[i].tagData.moreTagText, &_aiAircraft[i].tagData.moreTag);
                    }

                    al_draw_bitmap(_aiAircraft[i].tagData.moreTag.bmp, pos.x + 1 + iconData.halfHeight * _aircraft.scale, pos.y + _aiAircraft[i].tagData.tag.height / 2.0, 0);
                }
            }
        }
    }

    // Draw fixed objects, e.g. airports and waypoints
    for (int i = 0; i < _aiFixedCount; i++) {
        // Don't draw if outside the display
        if (_aiFixed[i].loc.lat < 99 && drawOther(&displayPos1, &displayPos2, &_aiFixed[i].loc, &pos)) {
            ALLEGRO_BITMAP* bmp;
            int halfWidth = _aircraft.smallHalfWidth / 2;
            int halfHeight = _aircraft.smallHalfHeight / 2;
            int tagShift = 1 + halfWidth * 1.5;

            if (strcmp(_aiFixed[i].model, "AIRP") == 0) {
                bmp = _aircraft.airportBmp;
            }
            else if (strcmp(_aiFixed[i].model, "WAYP") == 0) {
                bmp = _aircraft.waypointBmp;
                tagShift += halfWidth;
            }
            else if (strcmp(_aiFixed[i].model, "SRCH") == 0) {
                int width = _ring.width * _ring.scale;
                int height = _ring.height * _ring.scale;
                al_draw_scaled_bitmap(_ring.bmp, 0, 0, _ring.width, _ring.height, pos.x - width / 2, pos.y - height / 2.0, width, height, 0);
                continue;
            }
            else {
                continue;
            }

            al_draw_scaled_rotated_bitmap(bmp, halfWidth, halfHeight, pos.x, pos.y, _aircraft.scale, _aircraft.scale, _aiFixed[i].heading * DegreesToRadians, 0);

            if (_showTags) {
                if (_aiFixed[i].tagData.tag.bmp == NULL) {
                    createTagBitmap(_aiFixed[i].tagData.tagText, &_aiFixed[i].tagData.tag);
                }

                // Draw tag to right
                al_draw_bitmap(_aiFixed[i].tagData.tag.bmp, pos.x + tagShift * _aircraft.scale, pos.y - _aiFixed[i].tagData.tag.height / 4, 0);
            }
        }
    }
}

void render()
{
    if (*_settings.chart == '\0') {
        return;
    }

    // Draw chart
    al_draw_scaled_bitmap(_chart.bmp, _view.x, _view.y, _view.width, _view.height, 0, 0, _displayWidth, _displayHeight, 0);

    // Draw other aircraft
    drawOtherAircraft();

    if (_showAi) {
        // Draw fixed objects (if injected), e.g. airports and waypoints
        drawAiObjects();
    }

    // Draw aircraft
    drawOwnAircraft();

    // Draw first marker or both if a message is being displayed
    int destX = _mouseData.dragX + _chart.x * _view.scale - _displayWidth / 2;
    int destY = _mouseData.dragY + _chart.y * _view.scale - _displayHeight / 2;

    if (_chartData.state == 1 || (_chartData.state == 2 && (_titleState == -9 || _showCalibration))) {
        int width = _marker.width * _marker.scale;
        int height = _marker.height * _marker.scale;

        for (int i = 0; i < _chartData.state; i++) {
            int x = _chartData.x[i] * _view.scale - destX;
            int y = _chartData.y[i] * _view.scale - destY;
            al_draw_scaled_bitmap(_marker.bmp, 0, 0, _marker.width, _marker.height, x - width / 2, y - height / 2.0, width, height, 0);
        }
    }

    // Show clicked and clipboard positions when in calibration mode
    if (_chartData.state == 2 && _showCalibration) {
        if (_clickedPos.x != -1) {
            // Show clicked position as a small cross
            int width = _marker.width * _marker.scale / 2.0;
            int height = _marker.height * _marker.scale / 2.0;

            int x = _clickedPos.x * _view.scale - destX;
            int y = _clickedPos.y * _view.scale - destY;
            al_draw_scaled_bitmap(_marker.bmp, 0, 0, _marker.width, _marker.height, x - width / 2, y - height / 2.0, width, height, 0);
        }

        if (_clipboardPos.x != -1) {
            // Show clipboard position as a circle
            int width = _ring.width * _ring.scale;
            int height = _ring.height * _ring.scale;

            int x = _clipboardPos.x * _view.scale - destX;
            int y = _clipboardPos.y * _view.scale - destY;
            al_draw_scaled_bitmap(_ring.bmp, 0, 0, _ring.width, _ring.height, x - width / 2, y - height / 2.0, width, height, 0);
        }
    }

    if (_windDirection != -1) {
        // Draw wind at top centre of display
        int x = _displayWidth / 2;
        int y = 40;
        al_draw_scaled_rotated_bitmap(_arrow.bmp, _arrow.x, _arrow.y, x, y, 0.15, 0.2, (180 + _windDirection) * DegreesToRadians, 0);
        al_draw_bitmap_region(_windInfoCopy.bmp, 0, 0, _windInfoCopy.width, _windInfo.height, x + 26, y - _windInfo.height / 2.0, 0);
    }
}

/// <summary>
/// Initialise everything
/// </summary>
bool doInit()
{
    // Don't fail if no chart loaded yet
    initChart();

    if (!initAircraft()) {
        return false;
    }

    if (!initMarker()) {
        return false;
    }

    if (!initRing()) {
        return false;
    }

    if (!initWind()) {
        return false;
    }

    return true;
}

/// <summary>
/// Render the next frame
/// </summary>
void doRender()
{
    // Clear background
    al_clear_to_color(al_map_rgb(0, 0, 0));

    // Draw chart and aircraft
    render();

    // Allegro can detect window resize but not window move so do it here.
    // Only need to check once every second.
    if (_winCheckDelay > 0) {
        _winCheckDelay--;
    }
    else {
        _winCheckDelay = _settings.framesPerSec;
        RECT winPos;
        if (GetWindowRect(_displayWindow, &winPos)) {
            if (_settings.x != winPos.left || _settings.y != winPos.top)
            {
                _settings.x = winPos.left;
                _settings.y = winPos.top;
                saveSettings();
            }
        }
    }
}

/// <summary>
/// Select and load a new chart
/// </summary>
void newChart()
{
    al_set_window_title(_display, "Select Chart");

    if (!fileSelectorDialog(al_get_win_window_handle(_display))) {
        _titleState = -2;
        return;
    }

    if (initChart()) {
        // Save the last loaded chart name
        saveSettings();
    }
}

/// <summary>
/// Find all the .calibration files in the parent folder and use the calibrated coordinates
/// to work out which chart the aircraft is closest to, then load this chart.
/// </summary>
void closestChart()
{
    if (_aircraftData.loc.lat == MAXINT) {
        return;
    }

    CalibratedData* calib = (CalibratedData*)malloc(sizeof(CalibratedData) * MAX_CHARTS);

    int count;
    findCalibratedCharts(calib, &count);
    if (count == 0) {
        free(calib);
        return;
    }

    CalibratedData* closest = findClosestChart(calib, count, &_aircraftData.loc);
    strcpy(_settings.chart, closest->filename);
    free(calib);

    // Chart could be .png or .jpg
    char* ext = strrchr(_settings.chart, '.');
    strcpy(ext, ".png");
    FILE* inf = fopen(_settings.chart, "r");
    if (inf) {
        fclose(inf);
    }
    else {
        strcpy(ext, ".jpg");
    }

    if (initChart()) {
        // Save the chart name
        saveSettings();
    }
}

/// <summary>
/// When viewing calibration the last point clicked (small cross) and the
/// last location retrieved from the clipboard (circle) may be displayed.
/// </summary>
void clearCustomPoints()
{
    _clickedLoc.lat = MAXINT;
    _clickedPos.x = -1;
    _clipboardPos.x = -1;
    _titleState = -2;
}

void updateOwnLabel(char *newLabel)
{
    if (strcmp(_labelText, newLabel) == 0) {
        return;
    }

    strcpy(_labelText, newLabel);

    if (*_labelText != '\0') {
        al_set_target_bitmap(_aircraftLabelBmpCopy);
        al_draw_bitmap(_aircraftLabel.bmp, 0, 0, 0);
        al_draw_text(_font, al_map_rgb(0xc0, 0xc0, 0x0), 10, 10, 0, _labelText);
        al_set_target_backbuffer(_display);
    }
}

void updateOwnAircraft()
{
    if (_chartData.state != 2 || _aircraftData.loc.lat == MAXINT) {
        _aircraft.x = MAXINT;
        return;
    }

    AircraftPosition pos;
    aircraftLocToChartPos(&pos);

    _aircraft.x = pos.x;
    _aircraft.y = pos.y;
    _aircraftLabel.x = pos.labelX;
    _aircraftLabel.y = pos.labelY;

    // Update label (shows distance to own aircraft if off chart)
    updateOwnLabel(pos.text);

    // Update own tag (shown in window title)
    char newTag[68];
    createTagText(_aircraftData.callsign, _aircraftData.model, newTag);

    if (strcmp(_tagText, newTag) != 0) {
        strcpy(_tagText, newTag);
        _titleState = -2;
    }
}

/// <summary>
/// Create a tag bitmap to show to the right of other aircraft.
/// It displays their callsign and aircraft model.
/// </summary>
void createTagBitmap(char *tagText, DrawData* tag)
{
    tag->width = 4 + strlen(tagText) * 8;
    tag->height = 10;

    tag->bmp = al_create_bitmap(tag->width, tag->height);

    al_set_target_bitmap(tag->bmp);

    al_clear_to_color(al_map_rgb(0xe0, 0xe0, 0xe0));
    al_draw_text(_font, al_map_rgb(0x40, 0x40, 0x40), 2, 2, 0, tagText);

    al_set_target_backbuffer(_display);
}

/// <summary>
/// Move tags if they already exist, create new tags and delete old tags
/// </summary>
void updateOtherTags()
{
    if (_tagCount > 0) {
        memcpy(&_oldTag, &_otherTag, _tagDataSize * _tagCount);
    }

    for (int i = 0; i < _snapshotOtherCount; i++) {
        createTagText(_snapshotOtherData[i].callsign, _snapshotOtherData[i].model, _otherTag[i].tagText);

        char moreTagText[68];
        sprintf(moreTagText, "%.0lf %.0lf", _snapshotOtherData[i].alt, _snapshotOtherData[i].speed);

        bool tagFound = false;
        for (int j = 0; j < _tagCount; j++) {
            if (strcmp(_otherTag[i].tagText, _oldTag[j].tagText) == 0 && _oldTag[j].tag.bmp != NULL) {
                tagFound = true;
                memcpy(&_otherTag[i].tag, &_oldTag[j].tag, _drawDataSize);
                _oldTag[j].tag.bmp = NULL;
                memcpy(&_otherTag[i].moreTag, &_oldTag[j].moreTag, _drawDataSize);
                _oldTag[j].moreTag.bmp = NULL;

                if (strcmp(_otherTag[i].moreTagText, moreTagText) != 0) {
                    strcpy(_otherTag[i].moreTagText, moreTagText);
                    cleanupBitmap(_otherTag[i].moreTag.bmp);
                    _otherTag[i].moreTag.bmp = NULL;
                }
                break;
            }
        }

        if (!tagFound) {
            createTagBitmap(_otherTag[i].tagText, &_otherTag[i].tag);
            createTagBitmap(_otherTag[i].moreTagText, &_otherTag[i].moreTag);
        }
    }

    // Cleanup old tags
    for (int i = 0; i < _tagCount; i++) {
        if (_oldTag[i].tag.bmp != NULL) {
            al_destroy_bitmap(_oldTag[i].tag.bmp);
        }
        if (_oldTag[i].moreTag.bmp != NULL) {
            al_destroy_bitmap(_oldTag[i].moreTag.bmp);
        }
    }

    _tagCount = _snapshotOtherCount;
}

void updateWind()
{
    if (_windDirection == -1) {
        return;
    }

    int windDirection = _windData.direction + 0.5;
    int windSpeed = _windData.speed + 0.5;

    if (_windDirection == windDirection && _windSpeed == windSpeed) {
        return;
    }

    _windDirection = windDirection;
    _windSpeed = windSpeed;

    // Create wind info copy
    al_set_target_bitmap(_windInfoCopy.bmp);
    al_draw_bitmap(_windInfo.bmp, 0, 0, 0);

    char text[16];
    sprintf(text, " %d%s", windDirection, DegreesSymbol);
    al_draw_text(_font, al_map_rgb(0x40, 0x40, 0x40), 2, 2, 0, text);

    sprintf(text, "%d kts", windSpeed);
    al_draw_text(_font, al_map_rgb(0x40, 0x40, 0x40), 2, 12, 0, text);

    al_set_target_backbuffer(_display);

    // Reduce width for lower wind speed
    _windInfoCopy.width = _windInfo.width;
    if (_windSpeed < 100) {
        _windInfoCopy.width -= 8;
    }
    if (_windSpeed < 10) {
        _windInfoCopy.width -= 8;
    }
}

/// <summary>
/// Update everything before the next frame
/// </summary>
void doUpdate()
{
    if (*_settings.chart == '\0') {
        newChart();
    }

    al_get_mouse_state(&_mouse);

    if (_mouseData.dragging) {
        _mouseData.dragX = _mouseData.dragStartX - _mouse.x;
        _mouseData.dragY = _mouseData.dragStartY - _mouse.y;
    }

    _chart.scale = 100 + _mouseStartZ + _mouse.z;

    if (_chart.scale < -100) {
        // Map just loaded or reset
        _mouseStartZ += InitScale - _chart.scale;
        _chart.scale = InitScale;
    }
    else if (_chart.scale < MinScale) {
        _mouseStartZ += MinScale - _chart.scale;
        _chart.scale = MinScale;
    }
    else if (_chart.scale > MaxScale) {
        _mouseStartZ -= _chart.scale - MaxScale;
        _chart.scale = MaxScale;
    }

    // Want zoom to be exponential
    double zoomScale = _chart.scale * _chart.scale / 10000.0;
    if (_view.scale != zoomScale) {
        _view.scale = zoomScale;
    }

    // Calculate view, i.e. part of chart that is visible
    _view.x = _chart.x + (_mouseData.dragX - _displayWidth / 2.0) / _view.scale;
    _view.y = _chart.y + (_mouseData.dragY - _displayHeight / 2.0) / _view.scale;
    _view.width = _displayWidth / _view.scale;
    _view.height = _displayHeight / _view.scale;

    updateOwnAircraft();
    updateWind();

    // Take a new snapshot of the other aircraft
    _snapshotOtherCount = _otherAircraftCount;
    memcpy(&_snapshotOtherData, &_otherAircraftData, _otherDataSize * _snapshotOtherCount);

    // Create any tags that don't already exist
    updateOtherTags();

    // Update window title if required
    if (_titleState != _chartData.state) {
        if (_titleState == -9 && _titleDelay > 0) {
            _titleDelay--;
        }
        else {
            updateWindowTitle();
        }
    }
}

/// <summary>
/// Handle keypress
/// </summary>
void doKeypress(ALLEGRO_EVENT* event)
{
    switch (event->keyboard.keycode) {
    case ALLEGRO_KEY_ESCAPE:
        //_quit = true;
        break;
    }
}

/// <summary>
/// Handle mouse button press or release
/// </summary>
void doMouseButton(ALLEGRO_EVENT* event, bool isPress)
{
    static Position clickedPos;

    al_get_mouse_state(&_mouse);

    if (event->mouse.button == 1) {
        if (isPress) {
            // Left mouse button pressed
            clickedPos.x = _mouse.x;
            clickedPos.y = _mouse.y;
            _mouseData.dragging = true;
            _mouseData.dragStartX = _mouse.x;
            _mouseData.dragStartY = _mouse.y;

            // Show clicked coord in window title if showing calibration
            if (_showCalibration && _chartData.state == 2) {
                displayToChartPos(_mouse.x, _mouse.y, &_clickedPos);
                chartPosToLocation(_clickedPos.x, _clickedPos.y, &_clickedLoc);

                Locn loc;
                getClipboardLocation(&loc);
                locationToChartPos(&loc, &_clipboardPos);

                _titleState = -2;
            }
        }
        else {
            // Left mouse button released
            if (_mouseData.dragging) {
                _mouseData.dragging = false;
                _chart.x += _mouseData.dragX / _view.scale;
                _chart.y += _mouseData.dragY / _view.scale;
                _mouseData.dragX = 0;
                _mouseData.dragY = 0;
            }

            // If mouse wasn't dragged check for click on AI aircraft
            if (_aiAircraftCount > 0 && abs(_mouse.x - clickedPos.x) < 2 && abs( _mouse.y - clickedPos.y) < 2) {
                Position posMin, posMax;
                Locn locMin, locMax;
                displayToChartPos(_mouse.x - 12, _mouse.y - 12, &posMin);
                displayToChartPos(_mouse.x + 12, _mouse.y + 12, &posMax);
                chartPosToLocation(posMin.x, posMax.y, &locMin);
                chartPosToLocation(posMax.x, posMin.y, &locMax);

                for (int i = 0; i < _aiAircraftCount; i++) {
                    if (_aiAircraft[i].loc.lat >= locMin.lat && _aiAircraft[i].loc.lat <= locMax.lat &&
                        _aiAircraft[i].loc.lon >= locMin.lon && _aiAircraft[i].loc.lon <= locMax.lon)
                    {
                        // AI aircraft has been clicked
                        if (!_watchInProgress) {
                            strcpy(_watchCallsign, _aiAircraft[i].callsign);
                            _watchInProgress = true;
                        }
                    }
                }
            }
        }
    }
    else if (event->mouse.button == 2) {
        if (isPress) {
            // Right mouse button pressed
            if (_chartData.state == 0 || _chartData.state == 1) {
                _teleport.pos.x = MAXINT;
                Locn loc;
                getClipboardLocation(&loc);
                if (loc.lat == MAXINT) {
                    showClipboardMessage(true);
                }
                else {
                    saveCalibration(_mouse.x, _mouse.y, &loc);
                    if (_chartData.state == 1) {
                        showTitleMessage("CALIBRATING: Position 1 captured");
                    }
                    else if (_chartData.state == 2) {
                        showTitleMessage("CALIBRATING: Position 2 captured");

                        // Turn on show calibration straight after a chart is calibrated
                        _showCalibration = true;
                    }
                }
            }
            else {
                // Save clicked position
                _teleport.pos.x = _mouse.x;
                _teleport.pos.y = _mouse.y;
            }
        }
    }
    else if (event->mouse.button == 3) {
        if (isPress) {
            // Middle mouse button pressed
            if (_chartData.state == 0 || _chartData.state == 1) {
                cancelCalibration();
            }
            else {
                // Centre map and zoom fully out
                resetMap();
            }
        }
    }
}

///
/// main
///
void showChart()
{
    initVars();
    loadSettings();

    if (!init()) {
        cleanup();
        return;
    }

    if (!doInit()) {
        cleanup();
        return;
    }

    // If aircraft is initialised always start on the closest chart
    closestChart();

    doUpdate();

    bool redraw = true;
    ALLEGRO_EVENT event;

    //LARGE_INTEGER perfFreq, prevTime, nowTime, elapsedTime;
    //QueryPerformanceFrequency(&perfFreq);
    //QueryPerformanceCounter(&prevTime);

    al_start_timer(_timer);

    while (!_quit) {
        al_wait_for_event(_eventQueue, &event);

        switch (event.type) {
        case ALLEGRO_EVENT_TIMER:
            if (_menuItem != -1) {
                actionMenuItem();
                _menuItem = -1;
            }
            doUpdate();
            redraw = true;
            break;

        case ALLEGRO_EVENT_KEY_DOWN:
            doKeypress(&event);
            break;

        case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
            doMouseButton(&event, true);
            break;

        case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
            doMouseButton(&event, false);
            break;

        case ALLEGRO_EVENT_DISPLAY_CLOSE:
            _quit = true;
            break;

        case ALLEGRO_EVENT_DISPLAY_RESIZE:
            _displayWidth = event.display.width;
            _displayHeight = event.display.height;
            al_acknowledge_resize(_display);
            _settings.width = _displayWidth;
            _settings.height = _displayHeight;
            saveSettings();
            break;
        }

        if (redraw && al_is_event_queue_empty(_eventQueue) && !_quit) {
            doRender();
            al_flip_display();
            redraw = false;

            //QueryPerformanceCounter(&nowTime);
            //elapsedTime.QuadPart = (nowTime.QuadPart - prevTime.QuadPart) * 1000000;
            //elapsedTime.QuadPart /= perfFreq.QuadPart;
            //LONGLONG elapsedMillis = elapsedTime.QuadPart / 1000;
            //prevTime.QuadPart = nowTime.QuadPart;
            //printf("Elapsed: %lld\n", elapsedMillis);
        }
    }

    cleanup();
}
