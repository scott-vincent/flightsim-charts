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
const char MenuFile[] = "images/menu.png";
const char AircraftFile[] = "images/aircraft.png";
const char AircraftSmallFile[] = "images/aircraft_small.png";
const char AircraftOtherFile[] = "images/aircraft_other.png";
const char AircraftSmallOtherFile[] = "images/aircraft_small_other.png";
const char LabelFile[] = "images/label.png";
const char MarkerFile[] = "images/marker.png";
const char RingFile[] = "images/ring.png";
const char ArrowFile[] = "images/arrow.png";
const int DataRateFps = 6;
const int MinScale = 40;
const int MaxScale = 150;

// Externals
extern bool _quit;
extern LocData _aircraftLoc;
extern WindData _windData;
extern LocData _aircraftOtherLoc[MAX_AIRCRAFT];
extern int _aircraftOtherCount;
extern int _locDataSize;

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
DrawData _menu;
DrawData _menuSelect;
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
ProgramData _programData;
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
bool _menuActive = false;
bool _showTags = true;
bool _showCalibration = false;
Location _clickedLoc;
Position _clickedPos;
Position _clipboardPos;
int _windDirection = -1;
int _windSpeed = -1;


/// <summary>
/// Initialise global variables
/// </summary>
void initVars()
{
    _menu.bmp = NULL;
    _menuSelect.bmp = NULL;
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

    *_programData.chart = '\0';
    *_labelText = '\0';
    *_tagText = '\0';

    // Default window position and size if no settings saved
    _programData.x = 360;
    _programData.y = 100;
    _programData.width = 1200;
    _programData.height = 800;

    strcpy(_chartData.title, ProgramName);
    _chartData.state = -1;
    _titleState = -2;
    _clickedLoc.lat = 99999;
    _clickedPos.x = -1;
    _clipboardPos.x = -1;
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
    if ((_display = al_create_display(_programData.width, _programData.height)) == NULL) {
        printf("Failed to create display\n");
        return false;
    }

    // Make sure window isn't minimised
    _displayWindow = al_get_win_window_handle(_display);
    ShowWindow(_displayWindow, SW_SHOWNORMAL);

    // Position window
    al_set_window_position(_display, _programData.x, _programData.y);

    _displayWidth = al_get_display_width(_display);
    _displayHeight = al_get_display_height(_display);

    al_register_event_source(_eventQueue, al_get_keyboard_event_source());
    al_register_event_source(_eventQueue, al_get_mouse_event_source());
    al_register_event_source(_eventQueue, al_get_display_event_source(_display));

    if (!(_timer = al_create_timer(1.0 / DataRateFps))) {
        printf("Failed to create timer\n");
        return false;
    }

    al_register_event_source(_eventQueue, al_get_timer_event_source(_timer));

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
    cleanupBitmap(_menu.bmp);
    cleanupBitmap(_menuSelect.bmp);
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

    if (_display) {
        al_destroy_display(_display);
        al_inhibit_screensaver(false);
    }
}

/// <summary>
/// Display a message in the window title bar that disappears after about 3 seconds.
/// </summary>
void showMessage(const char* message)
{
    al_set_window_title(_display, message);
    _titleState = -9;
    _titleDelay = 50;
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
/// Use window title to request calibration
/// </summary>
void updateWindowTitle()
{
    const char preCalibrate[] = "THIS CHART NEEDS CALIBRATING: Select 'Re-calibrate' from the right-click menu";
    const char calibrate[] = "Use Little Navmap (More->Copy to clipboard) then right-click on this chart to set POSITION";
    const char cancel[] = "(middle-click to cancel)";

    char title[256];
    char titleStart[256];

    if (*_tagText == '\0') {
        strcpy(titleStart, ProgramName);
    }
    else {
        strcpy(titleStart, _tagText);
    }

    if (*_programData.chart == '\0') {
        strcpy(_chartData.title, titleStart);
    }
    else {
        char* name = strrchr(_programData.chart, '\\');
        if (name) {
            name++;
        }
        else {
            name = _programData.chart;
        }

        char chart[256];
        strcpy(chart, name);
        char* ext = strrchr(chart, '.');
        if (ext) {
            *ext = '\0';
        }

        sprintf(_chartData.title, "%s - %s", titleStart, chart);
    }

    switch (_chartData.state) {
    case -1:
        sprintf(title, "%s - %s", ProgramName, preCalibrate);
        al_set_window_title(_display, title);
        break;
    case 0:
        sprintf(title, "%s - %s 1 %s", ProgramName, calibrate, cancel);
        al_set_window_title(_display, title);
        break;
    case 1:
        sprintf(title, "%s - %s 2 %s", ProgramName, calibrate, cancel);
        al_set_window_title(_display, title);
        break;
    default:
        if (_showCalibration && _clickedLoc.lat != 99999) {
            char str[64];
            locationToString(&_clickedLoc, str);
            sprintf(title, "%s   %s", _chartData.title, str);
            al_set_window_title(_display, title);
        }
        else {
            al_set_window_title(_display, _chartData.title);
        }
        break;
    }

    _titleState = _chartData.state;
}

bool initMenu()
{
    _menu.bmp = al_load_bitmap(MenuFile);
    if (!_menu.bmp) {
        printf("Missing file: %s\n", MenuFile);
        return false;
    }

    _menu.width = al_get_bitmap_width(_menu.bmp);
    _menu.height = al_get_bitmap_height(_menu.bmp);

    _menuSelect.width = _menu.width - 18;
    _menuSelect.height = 22;
    _menuSelect.bmp = al_create_bitmap(_menuSelect.width, _menuSelect.height);

    al_set_target_bitmap(_menuSelect.bmp);
    al_clear_to_color(al_map_rgb(0xbb, 0xbb, 0xbb));

    al_set_target_backbuffer(_display);

    return true;
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
    if (*_programData.chart == '\0') {
        return false;
    }

    cleanupBitmap(_chart.bmp);
    _chart.bmp = NULL;

    cleanupBitmap(_zoomed.bmp);
    _zoomed.bmp = NULL;

    _chart.bmp = al_load_bitmap(_programData.chart);
    if (!_chart.bmp) {
        char msg[256];
        sprintf(msg, "ERROR: Failed to load chart %s\n", _programData.chart);
        showMessage(msg);
        *_programData.chart = '\0';
        return false;
    }

    _chart.width = al_get_bitmap_width(_chart.bmp);
    _chart.height = al_get_bitmap_height(_chart.bmp);

    // Centre map and zoom fully out
    resetMap();

    al_set_target_backbuffer(_display);

    // Load calibration if available
    _titleState = -2;
    loadCalibration();

    return true;
}

void initZoomedChart()
{
    if (*_programData.chart == '\0') {
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
        printf("Missing file: %s\n", AircraftFile);
        return false;
    }

    _aircraft.otherBmp = al_load_bitmap(AircraftOtherFile);
    if (!_aircraft.otherBmp) {
        printf("Missing file: %s\n", AircraftOtherFile);
        return false;
    }

    _aircraft.halfWidth = al_get_bitmap_width(_aircraft.bmp) / 2;
    _aircraft.halfHeight = al_get_bitmap_height(_aircraft.bmp) / 2;

    _aircraft.smallBmp = al_load_bitmap(AircraftSmallFile);
    if (!_aircraft.smallBmp) {
        printf("Missing file: %s\n", AircraftSmallFile);
        return false;
    }

    _aircraft.smallOtherBmp = al_load_bitmap(AircraftSmallOtherFile);
    if (!_aircraft.smallOtherBmp) {
        printf("Missing file: %s\n", AircraftSmallOtherFile);
        return false;
    }

    _aircraft.smallHalfWidth = al_get_bitmap_width(_aircraft.smallBmp) / 2;
    _aircraft.smallHalfHeight = al_get_bitmap_height(_aircraft.smallBmp) / 2;

    // Use this bitmap for the window icon
    al_set_display_icon(_display, _aircraft.bmp);

    // Label for aircraft info when it is off the screen
    _aircraftLabel.bmp = al_load_bitmap(LabelFile);
    if (!_aircraftLabel.bmp) {
        printf("Missing file: %s\n", LabelFile);
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
        printf("Missing file: %s\n", MarkerFile);
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
        printf("Missing file: %s\n", RingFile);
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
        printf("Missing file: %s\n", ArrowFile);
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
    if (_aircraft.x == 99999) {
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
    if (*_programData.chart == '\0') {
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

    // Draw menu
    if (_menuActive) {
        al_draw_bitmap(_menu.bmp, _menu.x, _menu.y, 0);
        // X is used to store the currently selected item number
        if (_menuSelect.x > 0) {
            // Set blender to multiply (shades of grey darken, white has no effect)
            al_set_blender(ALLEGRO_ADD, ALLEGRO_DEST_COLOR, ALLEGRO_ZERO);

            al_draw_bitmap(_menuSelect.bmp, _menu.x + 11, _menuSelect.y, 0);

            // Restore normal blender
            al_set_blender(ALLEGRO_ADD, ALLEGRO_ALPHA, ALLEGRO_INVERSE_ALPHA);
        }
    }
}

/// <summary>
/// Initialise everything
/// </summary>
bool doInit()
{
    if (!initMenu()) {
        return false;
    }

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
        _winCheckDelay = DataRateFps;
        RECT winPos;
        if (GetWindowRect(_displayWindow, &winPos)) {
            if (_programData.x != winPos.left || _programData.y != winPos.top)
            {
                _programData.x = winPos.left;
                _programData.y = winPos.top;
                saveProgramData();
            }
        }
    }
}

/// <summary>
/// See if mouse is over a selectable menu item
/// </summary>
void menuSelect(int mouseY)
{
    _menuSelect.y = _menu.y + 17;

    if (_mouse.y < _menuSelect.y) {
        return;
    }

    int nextItem = 1;
    int itemHeight = 25;

    _menuSelect.x = -1;
    while (_menuSelect.x == -1) {
        switch (nextItem) {
            case 1:     // Load Chart
            case 2:     // Show Tags
            case 5:     // Exit
                if (_mouse.y < _menuSelect.y + itemHeight) {
                    _menuSelect.x = nextItem;
                }
                else {
                    _menuSelect.y += itemHeight;
                }
                break;

            case 3:     // Show Calibration
                if (_mouse.y < _menuSelect.y + itemHeight) {
                    _menuSelect.x = nextItem;
                }
                else {
                    _menuSelect.y += itemHeight + 12;
                    if (_mouse.y < _menuSelect.y) {
                        _menuSelect.x = 0;
                    }
                }
                break;

            case 4:     // Re-calibrate
                if (_mouse.y < _menuSelect.y + itemHeight) {
                    _menuSelect.x = nextItem;
                }
                else {
                    _menuSelect.y += itemHeight + 14;
                    if (_mouse.y < _menuSelect.y) {
                        _menuSelect.x = 0;
                    }
                }
                break;

            default:
                _menuSelect.x = 0;
                break;
        }

        nextItem++;
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
        saveProgramData();
    }
}

/// <summary>
/// When viewing calibration the last point clicked (small cross) and the
/// last location retrieved from the clipboard (circle) may be displayed.
/// </summary>
void clearCustomPoints()
{
    _clickedLoc.lat = 99999;
    _clickedPos.x = -1;
    _clipboardPos.x = -1;
    _titleState = -2;
}

/// <summary>
/// A menu item has been clicked
/// </summary>
void menuAction()
{
    switch (_menuSelect.x) {
        case 1:
            // Load chart
            newChart();
            _showCalibration = false;
            break;

        case 2:
            // Show tags
            if (_chartData.state == 2) {
                _showTags = !_showTags;
            }
            break;

        case 3:
            // Show calibration
            if (_chartData.state == 2) {
                _showCalibration = !_showCalibration;
            }
            clearCustomPoints();
            break;

        case 4:
            // Re-calibrate
            _chartData.state = 0;
            _showCalibration = false;
            // Little Navmap is now the preferred way to calibrate so don't launch OpenStreetMap.
            //launchOpenStreetMap();
            break;

        case 5:
            // Exit
            _quit = true;
            break;
    }
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
    if (_chartData.state != 2 || _aircraftLoc.lat == 99999) {
        _aircraft.x = 99999;
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

    //printf("%s (self) - lat: %f  lon: %f  heading:%f  wingSpan: %f\n", _tagText, _aircraftLoc.lat, _aircraftLoc.lon, _aircraftLoc.heading, _aircraftLoc.wingSpan);
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

        //printf("%d: %s - lat: %f  lon: %f  heading:%f  wingSpan: %f\n", i, _otherTag[i].tagText, _snapshotOtherLoc[i].lat, _snapshotOtherLoc[i].lon, _snapshotOtherLoc[i].heading, _snapshotOtherLoc[i].wingSpan);
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
}

/// <summary>
/// Update everything before the next frame
/// </summary>
void doUpdate()
{
    if (*_programData.chart == '\0') {
        newChart();
    }

    al_get_mouse_state(&_mouse);

    if (_mouseData.dragging) {
        _mouseData.dragX = _mouseData.dragStartX - _mouse.x;
        _mouseData.dragY = _mouseData.dragStartY - _mouse.y;
    }

    if (_menuActive) {
        _menuSelect.x = 0;
        if (_mouse.x >= _menu.x && _mouse.x < _menu.x + _menu.width && _mouse.y >= _menu.y && _mouse.y < _menu.y + _menu.height) {
            menuSelect(_mouse.y);
        }
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
        _menuActive = false;

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
    if (event->mouse.button == 1) {
        if (isPress) {
            // Left mouse button pressed
            if (_menuActive && _mouse.x >= _menu.x && _mouse.x < _menu.x + _menu.width && _mouse.y >= _menu.y && _mouse.y < _menu.y + _menu.height) {
                // X is used to store the item number
                if (_menuSelect.x > 0) {
                    // Menu item has been clicked
                    _menuActive = false;
                    menuAction();
                }
            }
            else {
                _menuActive = false;
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
                Location loc;
                getClipboardLocation(&loc);
                if (loc.lat == 99999) {
                    showMessage("ERROR: Use Little Navmap (right-click/More/Copy to clipboard) or OpenStreetMap (right-click/Centre map here/Copy URL).");
                }
                else {
                    saveCalibration(_mouse.x, _mouse.y, &loc);
                }
            }
            else {
                _menu.x = _mouse.x - 3;
                _menu.y = _mouse.y - 3;
                _menuActive = true;
            }
        }
    }
    else if (event->mouse.button == 3) {
        if (isPress) {
            // Middle mouse button pressed
            _menuActive = false;

            if (_chartData.state == 0 || _chartData.state == 1) {
                // Calibration cancelled
                _chartData.state = -1;
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
    loadProgramData();

    if (!init()) {
        cleanup();
        return;
    }

    if (!doInit()) {
        cleanup();
        return;
    }

    doUpdate();

    bool redraw = true;
    ALLEGRO_EVENT event;

    al_start_timer(_timer);

    while (!_quit) {
        al_wait_for_event(_eventQueue, &event);

        switch (event.type) {
        case ALLEGRO_EVENT_TIMER:
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
            _programData.width = _displayWidth;
            _programData.height = _displayHeight;
            saveProgramData();
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
