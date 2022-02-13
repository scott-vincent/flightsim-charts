#include <windows.h>
#include <iostream>
#include <allegro5/allegro.h>
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
const char LabelFile[] = "images/label.png";
const char MarkerFile[] = "images/marker.png";
const char RingFile[] = "images/ring.png";
const char ArrowFile[] = "images/arrow.png";
const int MinScale = 40;
const int MaxScale = 150;

// Externals
extern bool _quit;
extern LocData _aircraftLoc;
extern WindData _windData;
extern LocData _aircraftOtherLoc[MAX_AIRCRAFT];
extern int _aircraftOtherCount;
extern int _locDataSize;
extern TeleportData _teleport;
extern SnapshotData _snapshot;

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
DrawData _zoomed;
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
LocData _snapshotOtherLoc[MAX_AIRCRAFT];
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
bool _showCalibration = false;
Location _clickedLoc;
Position _clickedPos;
Position _clipboardPos;
int _windDirection = -1;
int _windSpeed = -1;
HMENU _menu = NULL;
int _menuItem = -1;
int _lastRotate = MAXINT;

enum MENU_ITEMS {
    MENU_LOAD_CHART,
    MENU_LOAD_CLOSEST,
    MENU_LOCATE_AIRCRAFT,
    MENU_ROTATE_AGAIN,
    MENU_ROTATE_LEFT_20,
    MENU_ROTATE_RIGHT_20,
    MENU_ROTATE_LEFT_90,
    MENU_ROTATE_RIGHT_90,
    MENU_TELEPORT_HERE,
    MENU_TELEPORT_CLIPBOARD,
    MENU_TELEPORT_RESTORE_LOCATION,
    MENU_TELEPORT_SAVE_LOCATION,
    MENU_SHOW_TAGS,
    MENU_SHOW_CALIBRATION,
    MENU_RECALIBRATE
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

    char title[256];
    char titleStart[256];

    if (*_tagText == '\0') {
        strcpy(titleStart, ProgramName);
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

        sprintf(title, "%s - %s", titleStart, chart);
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
    _zoomed.bmp = NULL;
    _aircraft.bmp = NULL;
    _aircraft.smallBmp = NULL;
    _aircraft.otherBmp = NULL;
    _aircraft.smallOtherBmp = NULL;
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
void updateMenu()
{
    bool active = _aircraftLoc.lat != MAXINT;
    bool calibrated = _chartData.state == 2;

    EnableMenuItem(_menu, MENU_LOAD_CLOSEST, enabledState(active));
    EnableMenuItem(_menu, MENU_LOCATE_AIRCRAFT, enabledState(active && calibrated));
    EnableMenuItem(_menu, MENU_ROTATE_AGAIN, enabledState(active && !_teleport.inProgress && _lastRotate != MAXINT));
    EnableMenuItem(_menu, MENU_ROTATE_LEFT_20, enabledState(active && !_teleport.inProgress));
    EnableMenuItem(_menu, MENU_ROTATE_RIGHT_20, enabledState(active && !_teleport.inProgress));
    EnableMenuItem(_menu, MENU_ROTATE_LEFT_90, enabledState(active && !_teleport.inProgress));
    EnableMenuItem(_menu, MENU_ROTATE_RIGHT_90, enabledState(active && !_teleport.inProgress));
    EnableMenuItem(_menu, MENU_TELEPORT_HERE, enabledState(active && calibrated && !_teleport.inProgress));
    EnableMenuItem(_menu, MENU_TELEPORT_CLIPBOARD, enabledState(active && !_teleport.inProgress));
    EnableMenuItem(_menu, MENU_TELEPORT_RESTORE_LOCATION, enabledState(active && !_snapshot.save && _snapshot.loc.lat != MAXINT));
    EnableMenuItem(_menu, MENU_TELEPORT_SAVE_LOCATION, enabledState(active && !_snapshot.save));
    EnableMenuItem(_menu, MENU_SHOW_TAGS, enabledState(calibrated));
    EnableMenuItem(_menu, MENU_SHOW_CALIBRATION, enabledState(calibrated));

    CheckMenuItem(_menu, MENU_SHOW_TAGS, checkedState(_showTags));
    CheckMenuItem(_menu, MENU_SHOW_CALIBRATION, checkedState(_showCalibration));
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

        updateMenu();
        TrackPopupMenu(_menu, TPM_RIGHTBUTTON, menuPos.x, menuPos.y, 0, _displayWindow, NULL);
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
    _teleport.loc.lat = _aircraftLoc.lat;
    _teleport.loc.lon = _aircraftLoc.lon;
    _teleport.heading = _aircraftLoc.heading + _lastRotate;
    _teleport.inProgress = true;
}

void actionMenuItem()
{
    switch (_menuItem) {

    case MENU_LOAD_CHART:
        newChart();
        _showCalibration = false;
        break;

    case MENU_LOAD_CLOSEST:
        closestChart();
        _showCalibration = false;
        break;

    case MENU_LOCATE_AIRCRAFT:
        // Centre chart on current aircraft location
        Position pos;
        locationToChartPos(_aircraftLoc.lat, _aircraftLoc.lon, &pos);
        _chart.x = pos.x;
        _chart.y = pos.y;
        break;

    case MENU_ROTATE_AGAIN:
        rotateAircraft(_lastRotate);
        break;

    case MENU_ROTATE_LEFT_20:
        rotateAircraft(-20);
        break;

    case MENU_ROTATE_RIGHT_20:
        rotateAircraft(20);
        break;

    case MENU_ROTATE_LEFT_90:
        rotateAircraft(-90);
        break;

    case MENU_ROTATE_RIGHT_90:
        rotateAircraft(90);
        break;

    case MENU_TELEPORT_HERE:
        Position clickedPos;
        displayToChartPos(_teleport.pos.x, _teleport.pos.y, &_clickedPos);
        chartPosToLocation(_clickedPos.x, _clickedPos.y, &_teleport.loc);
        _teleport.heading = _aircraftLoc.heading;
        _teleport.inProgress = true;
        break;

    case MENU_TELEPORT_CLIPBOARD:
        getClipboardLocation(&_teleport.loc);
        if (_teleport.loc.lat == MAXINT) {
            showClipboardMessage(true);
        }
        else {
            _teleport.heading = _aircraftLoc.heading;
            _teleport.inProgress = true;
        }
        break;

    case MENU_TELEPORT_RESTORE_LOCATION:
        _snapshot.restore = true;
        break;

    case MENU_TELEPORT_SAVE_LOCATION:
        _snapshot.save = true;
        break;

    case MENU_SHOW_TAGS:
        _showTags = !_showTags;
        break;

    case MENU_SHOW_CALIBRATION:
        _showCalibration = !_showCalibration;
        clearCustomPoints();
        break;

    case MENU_RECALIBRATE:
        _chartData.state = 0;
        _showCalibration = false;
        break;
    }
}

bool initMenu()
{
    HMENU rotateMenu = CreatePopupMenu();
    AppendMenu(rotateMenu, MF_STRING, MENU_ROTATE_LEFT_20, "-20°");
    AppendMenu(rotateMenu, MF_STRING, MENU_ROTATE_RIGHT_20, "+20°");
    AppendMenu(rotateMenu, MF_STRING, MENU_ROTATE_LEFT_90, "-90°");
    AppendMenu(rotateMenu, MF_STRING, MENU_ROTATE_RIGHT_90, "+90°");

    HMENU teleportMenu = CreatePopupMenu();
    AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_HERE, "To here");
    AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_CLIPBOARD, "To clipboard location");
    AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_RESTORE_LOCATION, "Restore location");
    AppendMenu(teleportMenu, MF_STRING, MENU_TELEPORT_SAVE_LOCATION, "Save location");

    _menu = CreatePopupMenu();
    AppendMenu(_menu, MF_STRING, MENU_LOAD_CHART, "Load chart");
    AppendMenu(_menu, MF_STRING, MENU_LOAD_CLOSEST, "Load closest");
    AppendMenu(_menu, MF_STRING, MENU_LOCATE_AIRCRAFT, "Locate aircraft");
    AppendMenu(_menu, MF_STRING, MENU_ROTATE_AGAIN, "Rotate again");
    AppendMenu(_menu, MF_STRING | MF_POPUP, (UINT_PTR)rotateMenu, "Rotate aircraft");
    AppendMenu(_menu, MF_STRING | MF_POPUP, (UINT_PTR)teleportMenu, "Teleport aircraft");
    AppendMenu(_menu, MF_STRING, MENU_SHOW_TAGS, "Show tags");
    AppendMenu(_menu, MF_STRING, MENU_SHOW_CALIBRATION, "Show calibration");
    AppendMenu(_menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(_menu, MF_STRING, MENU_RECALIBRATE, "Re-calibrate chart");

    // Listen for menu events
    if (!al_win_add_window_callback(_display, &windowCallback, NULL)) {
        return false;
    }

    return true;
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

    if (!initMenu()) {
        printf("Failed to initialise menu\n");
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
    cleanupBitmap(_zoomed.bmp);
    cleanupBitmap(_aircraft.bmp);
    cleanupBitmap(_aircraft.smallBmp);
    cleanupBitmap(_aircraft.otherBmp);
    cleanupBitmap(_aircraft.smallOtherBmp);
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

    if (_timer) {
        al_destroy_timer(_timer);
    }

    if (_eventQueue) {
        al_destroy_event_queue(_eventQueue);
    }

    if (_font) {
        al_destroy_font(_font);
    }

    if (_menu) {
        al_win_remove_window_callback(_display, &windowCallback, NULL);
        DestroyMenu(_menu);
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
        strcpy(tagText, callsign);
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
    _zoomed.scale = 0;
}

bool initChart()
{
    if (*_settings.chart == '\0') {
        return false;
    }

    cleanupBitmap(_chart.bmp);
    _chart.bmp = NULL;

    cleanupBitmap(_zoomed.bmp);
    _zoomed.bmp = NULL;

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

    _titleState = -1;
    loadCalibrationData(&_chartData);

    return true;
}

void initZoomedChart()
{
    if (*_settings.chart == '\0') {
        return;
    }

    _zoomed.width = _chart.width * _zoomed.scale;
    _zoomed.height = _chart.height * _zoomed.scale;

    _zoomed.bmp = al_create_bitmap(_zoomed.width, _zoomed.height);
    al_set_target_bitmap(_zoomed.bmp);
    al_draw_scaled_bitmap(_chart.bmp, 0, 0, _chart.width, _chart.height, 0, 0, _zoomed.width, _zoomed.height, 0);

    al_set_target_backbuffer(_display);
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

    if (_aircraftLoc.wingSpan > WINGSPAN_SMALL) {
        bmp = _aircraft.bmp;
        halfWidth = _aircraft.halfWidth;
        halfHeight = _aircraft.halfHeight;
    }
    else {
        bmp = _aircraft.smallBmp;
        halfWidth = _aircraft.smallHalfWidth;
        halfHeight = _aircraft.smallHalfHeight;
    }

    if (_zoomed.scale > 0.2) {
        _aircraft.scale = 0.2;
    }
    else {
        _aircraft.scale = 0.14;
    }

    al_draw_scaled_rotated_bitmap(bmp, halfWidth, halfHeight, pos.x, pos.y, _aircraft.scale, _aircraft.scale, _aircraftLoc.heading * DegreesToRadians, 0);

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
        }

        al_draw_bitmap(_aircraftLabelBmpCopy, pos.x, pos.y, 0);
    }
}

void drawOtherAircraft()
{
    if (_aircraftOtherCount == 0) {
        return;
    }

    Position displayPos1;
    displayToChartPos(0, 0, &displayPos1);

    Position displayPos2;
    displayToChartPos(_displayWidth - 1, _displayHeight - 1, &displayPos2);

    // Expand the display slightly so aircraft can be drawn partly off the edge
    double border = 15.0 / _zoomed.scale;
    displayPos1.x -= border;
    displayPos1.y -= border;
    displayPos2.x += border;
    displayPos2.y += border;

    if (_zoomed.scale > 0.2) {
        _aircraft.scale = 0.2;
    }
    else {
        _aircraft.scale = 0.14;
    }

    Position pos;
    for (int i = 0; i < _aircraftOtherCount; i++) {
        // Exclude self
        if (strcmp(_tagText, _otherTag[i].tagText) == 0) {
            continue;
        }

        // Don't draw other aircraft if it is outside the display
        if (drawOtherAircraft(&displayPos1, &displayPos2, &_aircraftOtherLoc[i], &pos)) {
            ALLEGRO_BITMAP* bmp;
            int halfWidth;
            int halfHeight;

            if (_aircraftOtherLoc[i].wingSpan > WINGSPAN_SMALL) {
                bmp = _aircraft.otherBmp;
                halfWidth = _aircraft.halfWidth;
                halfHeight = _aircraft.halfHeight;
            }
            else {
                bmp = _aircraft.smallOtherBmp;
                halfWidth = _aircraft.smallHalfWidth;
                halfHeight = _aircraft.smallHalfHeight;
            }

            al_draw_scaled_rotated_bitmap(bmp, halfWidth, halfHeight, pos.x, pos.y, _aircraft.scale, _aircraft.scale, _aircraftOtherLoc[i].heading * DegreesToRadians, 0);

            if (_otherTag[i].tag.bmp != NULL && _showTags) {
                // Draw tag to right of aircraft
                al_draw_bitmap(_otherTag[i].tag.bmp, pos.x + 1 + halfHeight * _aircraft.scale, pos.y - _otherTag[i].tag.height / 2, 0);
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
    int destX = _mouseData.dragX + _chart.x * _zoomed.scale - _displayWidth / 2;
    int destY = _mouseData.dragY + _chart.y * _zoomed.scale - _displayHeight / 2;
    al_draw_bitmap_region(_zoomed.bmp, destX, destY, _displayWidth, _displayHeight, 0, 0, 0);

    // Draw aircraft
    drawOwnAircraft();

    // Draw other aircraft
    drawOtherAircraft();

    // Draw first marker or both if a message is being displayed
    if (_chartData.state == 1 || (_chartData.state == 2 && (_titleState == -9 || _showCalibration))) {
        int width = _marker.width * _marker.scale;
        int height = _marker.height * _marker.scale;

        for (int i = 0; i < _chartData.state; i++) {
            int x = _chartData.x[i] * _zoomed.scale - destX;
            int y = _chartData.y[i] * _zoomed.scale - destY;
            al_draw_scaled_bitmap(_marker.bmp, 0, 0, _marker.width, _marker.height, x - width / 2, y - height / 2, width, height, 0);
        }
    }

    // Show clicked and clipboard positions when in calibration mode
    if (_chartData.state == 2 && _showCalibration) {
        if (_clickedPos.x != -1) {
            // Show clicked position as a small cross
            int width = _marker.width * _marker.scale / 2.0;
            int height = _marker.height * _marker.scale / 2.0;

            int x = _clickedPos.x * _zoomed.scale - destX;
            int y = _clickedPos.y * _zoomed.scale - destY;
            al_draw_scaled_bitmap(_marker.bmp, 0, 0, _marker.width, _marker.height, x - width / 2, y - height / 2, width, height, 0);
        }

        if (_clipboardPos.x != -1) {
            // Show clipboard position as a circle
            int width = _ring.width * _ring.scale;
            int height = _ring.height * _ring.scale;

            int x = _clipboardPos.x * _zoomed.scale - destX;
            int y = _clipboardPos.y * _zoomed.scale - destY;
            al_draw_scaled_bitmap(_ring.bmp, 0, 0, _ring.width, _ring.height, x - width / 2, y - height / 2, width, height, 0);
        }
    }

    // Draw wind at top centre of display
    int x = _displayWidth / 2;
    int y = 40;
    al_draw_scaled_rotated_bitmap(_arrow.bmp, _arrow.x, _arrow.y, x, y, 0.15, 0.2, (180 + _windDirection) * DegreesToRadians, 0);
    al_draw_bitmap_region(_windInfoCopy.bmp, 0, 0, _windInfoCopy.width, _windInfo.height, x + 26, y - _windInfo.height / 2, 0);
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
    if (_aircraftLoc.lat == MAXINT) {
        return;
    }

    CalibratedData* calib = (CalibratedData*)malloc(sizeof(CalibratedData) * MAX_CHARTS);

    int count;
    findCalibratedCharts(calib, &count);
    if (count == 0) {
        free(calib);
        return;
    }

    CalibratedData* closest = findClosestChart(calib, count, &_aircraftLoc);
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
    if (_chartData.state != 2 || _aircraftLoc.lat == MAXINT) {
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
    createTagText(_aircraftLoc.callsign, _aircraftLoc.model, newTag);

    if (strcmp(_tagText, newTag) != 0) {
        strcpy(_tagText, newTag);
        _titleState = -2;
    }
}

/// <summary>
/// Create a tag bitmap to show to the right of other aircraft.
/// It displays their callsign and aircraft model.
/// </summary>
void createTagBitmap(TagData* otherTag)
{
    otherTag->tag.width = 4 + strlen(otherTag->tagText) * 8;
    otherTag->tag.height = 10;

    otherTag->tag.bmp = al_create_bitmap(otherTag->tag.width, otherTag->tag.height);

    al_set_target_bitmap(otherTag->tag.bmp);

    al_clear_to_color(al_map_rgb(0xe0, 0xe0, 0xe0));
    al_draw_text(_font, al_map_rgb(0x40, 0x40, 0x40), 2, 2, 0, otherTag->tagText);

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
        createTagText(_snapshotOtherLoc[i].callsign, _snapshotOtherLoc[i].model, _otherTag[i].tagText);

        bool tagFound = false;
        for (int j = 0; j < _tagCount; j++) {
            if (strcmp(_otherTag[i].tagText, _oldTag[j].tagText) == 0 && _oldTag[j].tag.bmp != NULL) {
                tagFound = true;
                memcpy(&_otherTag[i].tag, &_oldTag[j].tag, _drawDataSize);
                _oldTag[j].tag.bmp = NULL;
                break;
            }
        }

        if (!tagFound) {
            if (_showTags) {
                createTagBitmap(&_otherTag[i]);
            }
            else {
                // Don't create new tags if tags are currently hidden
                _otherTag[i].tag.bmp = NULL;
            }
        }
    }

    // Cleanup old tags
    for (int i = 0; i < _tagCount; i++) {
        if (_oldTag[i].tag.bmp != NULL) {
            al_destroy_bitmap(_oldTag[i].tag.bmp);
        }
    }

    _tagCount = _snapshotOtherCount;
}

void updateWind()
{
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

    if (_chart.scale < MinScale) {
        _mouseStartZ += MinScale - _chart.scale;
        _chart.scale = MinScale;
    }
    else if (_chart.scale > MaxScale) {
        _mouseStartZ -= _chart.scale - MaxScale;
        _chart.scale = MaxScale;
    }

    // Want zoom to be exponential
    double zoomScale = _chart.scale * _chart.scale / 10000.0;
    if (_zoomed.scale != zoomScale) {
        _zoomed.scale = zoomScale;

        cleanupBitmap(_zoomed.bmp);
        _zoomed.bmp = NULL;
    }

    if (_zoomed.bmp == NULL) {
        initZoomedChart();
    }

    updateOwnAircraft();
    updateWind();

    // Take a new snapshot of the other aircraft
    _snapshotOtherCount = _aircraftOtherCount;
    memcpy(&_snapshotOtherLoc, &_aircraftOtherLoc, _locDataSize * _snapshotOtherCount);

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
    al_get_mouse_state(&_mouse);

    if (event->mouse.button == 1) {
        if (isPress) {
            // Left mouse button pressed
            _mouseData.dragging = true;
            _mouseData.dragStartX = _mouse.x;
            _mouseData.dragStartY = _mouse.y;

            // Show clicked coord in window title if showing calibration
            if (_showCalibration && _chartData.state == 2) {
                displayToChartPos(_mouse.x, _mouse.y, &_clickedPos);
                chartPosToLocation(_clickedPos.x, _clickedPos.y, &_clickedLoc);

                Location loc;
                getClipboardLocation(&loc);
                locationToChartPos(loc.lat, loc.lon, &_clipboardPos);

                _titleState = -2;
            }
        }
        else {
            // Left mouse button released
            if (_mouseData.dragging) {
                _mouseData.dragging = false;
                _chart.x += _mouseData.dragX / _zoomed.scale;
                _chart.y += _mouseData.dragY / _zoomed.scale;
                _mouseData.dragX = 0;
                _mouseData.dragY = 0;
            }
        }
    }
    else if (event->mouse.button == 2) {
        if (isPress) {
            // Right mouse button pressed
            if (_chartData.state == 0 || _chartData.state == 1) {
                _teleport.pos.x = MAXINT;
                Location loc;
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
        }
    }

    cleanup();
}
