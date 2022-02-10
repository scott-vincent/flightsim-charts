#include <windows.h>
#include <iostream>
#include "flightsim-charts.h"
#include "ChartCoords.h"

// Constants
const char SettingsExt[] = ".settings";
const char CalibrationExt[] = ".calibration";
const char urlQuery[] = "https://www.openstreetmap.org/search?query=";

// Externals
extern int _displayWidth;
extern int _displayHeight;
extern ChartData _chartData;
extern Settings _settings;


/// <summary>
/// Returns the full pathname of the settings file
/// </summary>
char* settingsFile()
{
    static char filename[256];
    GetModuleFileName(NULL, filename, 256);

    char* last = strrchr(filename, '\\');
    if (last) {
        char* ext = strrchr(last, '.');
        if (ext) {
            *ext = '\0';
        }
    }
    strcat(filename, SettingsExt);

    return filename;
}

/// <summary>
/// Returns the full pathname of the chart calibration file
/// </summary>
char* calibrationFile()
{
    static char filename[256];
    strcpy(filename, _settings.chart);

    char* last = strrchr(filename, '\\');
    if (last) {
        char* ext = strrchr(last, '.');
        if (ext) {
            *ext = '\0';
        }
    }
    strcat(filename, CalibrationExt);

    return filename;
}

/// <summary>
/// Save the latest window position/size and chart file name
/// </summary>
void saveSettings()
{
    FILE* outf = fopen(settingsFile(), "w");
    if (!outf) {
        char msg[256];
        sprintf(msg, "Failed to write file %s", settingsFile());
        showMessage(msg, true);
        return;
    }

    fprintf(outf, "%d,%d,%d,%d\n", _settings.x, _settings.y, _settings.width, _settings.height);
    fprintf(outf, "%s\n", _settings.chart);
    fprintf(outf, "FramesPerSec = %d\n", _settings.framesPerSec);

    fclose(outf);
}

/// <summary>
/// Load the last window position/size and chart file name if it exists.
/// </summary>
void loadSettings()
{
    FILE* inf = fopen(settingsFile(), "r");
    if (inf != NULL) {
        char line[256];
        int nextLine = 1;
        int x;
        int y;
        int width;
        int height;

        while (fgets(line, 256, inf) && nextLine < 4) {
            while (strlen(line) > 0 && (line[strlen(line) - 1] == ' '
                || line[strlen(line) - 1] == '\r' || line[strlen(line) - 1] == '\n')) {
                line[strlen(line) - 1] = '\0';
            }

            if (strlen(line) == 0) {
                continue;
            }

            switch (nextLine) {
            case 1:
            {
                int items = sscanf(line, "%d,%d,%d,%d", &x, &y, &width, &height);
                if (items == 4) {
                    _settings.x = x;
                    _settings.y = y;
                    _settings.width = width;
                    _settings.height = height;
                }
                break;
            }
            case 2:
            {
                strcpy(_settings.chart, line);
                break;
            }
            case 3:
            {
                char* sep = strchr(line, '=');
                if (sep) {
                    _settings.framesPerSec = atoi(sep + 1);
                }
                break;
            }
            }

            nextLine++;
        }

        fclose(inf);

        if (_settings.framesPerSec < 1) {
            _settings.framesPerSec = DefaultFPS;
            saveSettings();
        }
    }
    else {
        // No settings file so use defaults
        _settings.framesPerSec = DefaultFPS;
        GetModuleFileName(NULL, _settings.chart, 256);

        char* last = strrchr(_settings.chart, '\\');
        if (last) {
            last++;
            *last = '\0';
            strcat(_settings.chart, DefaultChart);
        }
    }
}

/// <summary>
/// Add position to config and save/update chart calibration file
/// </summary>
void saveCalibration(int x, int y, Location* loc)
{
    Position chartPos;
    displayToChartPos(x, y, &chartPos);

    // Make sure points are far enough apart (pixels and coords)
    if (_chartData.state == 1) {
        int xDiff = abs(_chartData.x[0] - chartPos.x);
        int yDiff = abs(_chartData.y[0] - chartPos.y);
        double latDiff = abs(_chartData.lat[0] - loc->lat);
        double lonDiff = abs(_chartData.lon[0] - loc->lon);

        char msg[256];
        *msg = '\0';

        if (xDiff < 50) {
            sprintf(msg, "Calibration Failed\n\nPoints are too close together horizontally (%d).\n", xDiff);
        }
        else if (yDiff < 50) {
            sprintf(msg, "Calibration Failed\n\nPoints are too close together vertically (%d).\n", yDiff);
        }
        else if (latDiff < 0.001) {
            sprintf(msg, "Calibration Failed\n\nLatitudes are too close together (%lf).\n", latDiff);
        }
        else if (lonDiff < 0.001) {
            sprintf(msg, "Calibration Failed\n\nLongitudes are too close together (%lf).\n", lonDiff);
        }

        if (*msg != '\0') {
            strcat(msg, "Please calibrate the 2nd point again.");
            showMessage(msg, true);
            return;
        }
    }

    _chartData.x[_chartData.state] = chartPos.x;
    _chartData.y[_chartData.state] = chartPos.y;
    _chartData.lat[_chartData.state] = loc->lat;
    _chartData.lon[_chartData.state] = loc->lon;
    _chartData.state++;

    char msg[256];

    FILE* outf = fopen(calibrationFile(), "w");
    if (!outf) {
        sprintf(msg, "Failed to write file %s", calibrationFile());
        showMessage(msg, true);
        return;
    }

    for (int i = 0; i < _chartData.state; i++) {
        fprintf(outf, "%d,%d = %lf,%lf\n", _chartData.x[i], _chartData.y[i], _chartData.lat[i], _chartData.lon[i]);
    }

    fclose(outf);
}

/// <summary>
/// Load calibration data if it exists.
/// If no filename supplied assume currently loaded chart
/// </summary>
void loadCalibrationData(ChartData* chartData, char* filename = NULL)
{
    if (filename == NULL) {
        filename = calibrationFile();
    }

    FILE* inf = fopen(filename, "r");
    if (inf == NULL) {
        return;
    }

    char line[256];
    int x;
    int y;
    double lat;
    double lon;

    chartData->state = 0;
    while (fgets(line, 256, inf) && chartData->state < 2) {
        while (strlen(line) > 0 && (line[strlen(line) - 1] == ' '
            || line[strlen(line) - 1] == '\r' || line[strlen(line) - 1] == '\n')) {
            line[strlen(line) - 1] = '\0';
        }

        if (strlen(line) > 0) {
            int items = sscanf(line, "%d,%d = %lf,%lf", &x, &y, &lat, &lon);
            if (items == 4) {
                chartData->x[chartData->state] = x;
                chartData->y[chartData->state] = y;
                chartData->lat[chartData->state] = lat;
                chartData->lon[chartData->state] = lon;
                chartData->state++;
            }
        }
    }

    fclose(inf);
}

/// <summary>
/// Windows Common File Picker Dialog.
/// Returns false if cancelled.
/// </summary>
bool fileSelectorDialog(HWND displayHwnd)
{
    bool validFolder = false;
    char filename[260];
    char folder[260];
    OPENFILENAME ofn;

    // Set initial folder
    strcpy(folder, _settings.chart);
    char* sep = strrchr(folder, '\\');
    if (sep) {
        *sep = '\0';
    }

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = displayHwnd;

    ofn.lpstrFile = filename;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = ".png or .jpg\0*.png;*.jpg\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = folder;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (!GetOpenFileName(&ofn)) {
        return false;
    }

    strcpy(_settings.chart, filename);
    return true;
}

/// <summary>
/// Reads coordinates from clipboard. Clipboard can be from Little Navmap, Google Maps or OpenStreetMap.
/// Returns MAXINT if no valid location found.
/// </summary>
void getClipboardLocation(Location* loc)
{
    // Make sure required data is in clipboard
    char text[256];
    *text = '\0';

    if (OpenClipboard(NULL)) {
        HANDLE clipboard = GetClipboardData(CF_TEXT);

        if (clipboard != NULL) {
            strncpy(text, (char*)clipboard, 255);
            text[255] = '\0';
            //EmptyClipboard();
        }
        CloseClipboard();
    }

    loc->lat = MAXINT;
    loc->lon = MAXINT;

    if (strchr(text, '°')) {
        // Little Navmap format, e.g. 51° 28' 29.60" N 0° 29' 5.19" W
        int deg;
        int min;
        double sec;
        char dirn;

        int count = sscanf(text, "%d° %d' %lf\" %c", &deg, &min, &sec, &dirn);
        char* next = strchr(text, dirn);
        if (count == 4 && next) {
            loc->lat = deg + (min / 60.0) + (sec / 3600.0);
            if (dirn == 'S') {
                loc->lat = -loc->lat;
            }
            next++;
            count = sscanf(next, "%d° %d' %lf\" %c", &deg, &min, &sec, &dirn);
            if (count == 4) {
                loc->lon = deg + (min / 60.0) + (sec / 3600.0);
                if (dirn == 'W') {
                    loc->lon = -loc->lon;
                }
            }
        }
    }
    else if (strchr(text, '#')) {
        // OpenStreetMap format, e.g. https://www.openstreetmap.org/#map=14/51.4706/-0.4540
        char* last = strrchr(text, '/');
        if (last) {
            loc->lon = strtod(last + 1, NULL);
            *last = '\0';
        }

        last = strrchr(text, '/');
        if (last) {
            loc->lat = strtod(last + 1, NULL);
        }
    }
    else {
        // Google Maps format, e.g. 51.471327034054205, -0.4534810854350456
        double lat;
        double lon;
        int count = sscanf(text, "%lf, %lf", &lat, &lon);
        if (count == 2) {
            loc->lat = lat;
            loc->lon = lon;
        }
    }

    if (loc->lon == MAXINT) {
        loc->lat = MAXINT;
    }
}

/// <summary>
/// Recursively find all calibration files in a parent folder
/// </summary>
void findAllFiles(char *folder, CalibratedData* calib, int* count)
{
    char searchPath[256];
    sprintf(searchPath, "%s\\*", folder);

    WIN32_FIND_DATA fileData;
    HANDLE hFind = FindFirstFile(searchPath, &fileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    char filePath[512];
    do
    {
        if (*fileData.cFileName == '.') {
            continue;
        }

        sprintf(filePath, "%s\\%s", folder, fileData.cFileName);

        if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            findAllFiles(filePath, calib, count);
        }
        else {
            char* ext = strrchr(filePath, '.');
            if (ext && strcmp(ext, CalibrationExt) == 0) {
                CalibratedData* nextCalib = calib + *count;
                strcpy(nextCalib->filename, filePath);
                nextCalib->data.state = -1;
                loadCalibrationData(&nextCalib->data, nextCalib->filename);

                if (*count < MAX_CHARTS) {
                    (*count)++;
                }
            }
        }
    } while (FindNextFile(hFind, &fileData) != 0);

    FindClose(hFind);
}

/// <summary>
/// Use current chart file to find parent folder.
/// Pathname must include airport code, e.g. either /EGKK/ or /EG/KK/
/// Find all calibration files within parent folder.
/// </summary>
void findCalibratedCharts(CalibratedData* calib, int* count)
{
    char folder[256];
    *count = 0;

    strcpy(folder, _settings.chart);
    char* sep = strrchr(folder, '\\');
    if (!sep) {
        return;
    }

    // Remove filename to get folder name
    *sep = '\0';
    sep = strrchr(folder, '\\');
    if (!sep) {
        return;
    }

    // If folder ends with /xx go back another level
    if (strlen(sep) == 3) {
        *sep = '\0';
        sep = strrchr(folder, '\\');
        if (!sep) {
            return;
        }
    }

    // Expect folder to end with /xx or /xxxx
    if (strlen(sep) != 3 && strlen(sep) != 5) {
        return;
    }
    *sep = '\0';

    // Recurse parent folder to find all calibration files
    findAllFiles(folder, calib, count);
}
