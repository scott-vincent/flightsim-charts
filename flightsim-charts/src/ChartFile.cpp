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
extern ProgramData _programData;

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
    strcpy(filename, _programData.chart);

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
void saveProgramData()
{
    FILE* outf = fopen(settingsFile(), "w");
    if (!outf) {
        char msg[256];
        sprintf(msg, "ERROR: Failed to write file %s", settingsFile());
        showMessage(msg);
        return;
    }

    fprintf(outf, "%d,%d,%d,%d\n", _programData.x, _programData.y, _programData.width, _programData.height);
    fprintf(outf, "%s\n", _programData.chart);
    fclose(outf);
}

/// <summary>
/// Load the last window position/size and chart file name if it exists.
/// </summary>
void loadProgramData()
{
    FILE* inf = fopen(settingsFile(), "r");
    if (inf != NULL) {
        char line[256];
        int nextLine = 1;
        int x;
        int y;
        int width;
        int height;

        while (fgets(line, 256, inf) && nextLine < 3) {
            while (strlen(line) > 0 && (line[strlen(line) - 1] == ' '
                || line[strlen(line) - 1] == '\r' || line[strlen(line) - 1] == '\n')) {
                line[strlen(line) - 1] = '\0';
            }

            if (strlen(line) > 0) {
                if (nextLine == 1) {
                    int items = sscanf(line, "%d,%d,%d,%d", &x, &y, &width, &height);
                    if (items == 4) {
                        _programData.x = x;
                        _programData.y = y;
                        _programData.width = width;
                        _programData.height = height;
                    }
                }
                else if (nextLine == 2) {
                    strcpy(_programData.chart, line);
                }

                nextLine++;
            }
        }

        fclose(inf);
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

        if (xDiff < 100) {
            sprintf(msg, "CALIBRATION FAILED: Points are too close together horizontally (%d)", xDiff);
        }
        else if (yDiff < 100) {
            sprintf(msg, "CALIBRATION FAILED: Points are too close together vertically (%d)", yDiff);
        }
        else if (latDiff < 0.0015) {
            sprintf(msg, "CALIBRATION FAILED: Latitudes are too close together (%lf)", latDiff);
        }
        else if (lonDiff < 0.0015) {
            sprintf(msg, "CALIBRATION FAILED: Longitudes are too close together (%lf)", lonDiff);
        }

        if (*msg != '\0') {
            // 2nd point will be requested again
            showMessage(msg);
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
        sprintf(msg, "ERROR: Failed to write file %s", calibrationFile());
        showMessage(msg);
        return;
    }

    for (int i = 0; i < _chartData.state; i++) {
        fprintf(outf, "%d,%d = %lf,%lf\n", _chartData.x[i], _chartData.y[i], _chartData.lat[i], _chartData.lon[i]);
    }

    fclose(outf);

    sprintf(msg, "CALIBRATING: Position %d captured as %lf, %lf", _chartData.state, loc->lat, loc->lon);
    showMessage(msg);
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
    if (inf != NULL) {
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
    strcpy(folder, _programData.chart);
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

    strcpy(_programData.chart, filename);
    return true;
}

/// <summary>
/// Launch OpenStreetMap so user can calibrate chart
/// </summary>
void launchOpenStreetMap()
{
    // Extract airport ICAO from folder name or filename
    char airport[5];
    strcpy(airport, "EGKK");
    bool found = false;

    char filename[256];
    strcpy(filename, _programData.chart);
    char* last = strrchr(filename, '\\');
    if (last) {
        *last = '\0';
        last = strrchr(filename, '\\');
    }

    if (last && strlen(last) == 5) {
        strcpy(airport, &last[1]);
        found = true;
    }
    else if (last && strlen(last) == 3 && last - filename > 2) {
        airport[0] = *(last - 2);
        airport[1] = *(last - 1);
        airport[2] = *(last + 1);
        airport[3] = *(last + 2);
        found = true;
    }
    else {
        // Look for 4 capital letters in filename
        char* name = strrchr(_programData.chart, '\\');
        while (*name != '\0') {
            if (*name >= 'A' && *name <= 'Z' && *(name + 1) >= 'A' && *(name + 1) <= 'Z' &&
                *(name + 2) >= 'A' && *(name + 2) <= 'Z' && *(name + 3) >= 'A' && *(name + 3) <= 'Z')
            {
                strncpy(airport, name, 4);
                found = true;
                break;
            }
            name++;
        }
    }

    if (found) {
        printf("Found airport ICAO: %s in chart path: %s\n", airport, _programData.chart);
    }
    else {
        printf("Defaulting to %s as no airport ICAO found in chart path: %s\n", airport, _programData.chart);
    }

    char url[256];
    sprintf(url, "%s%s", urlQuery, airport);
    ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

/// <summary>
/// Reads coordinates from clipboard. Clipboard can be from Little Navmap or OpenStreetMap.
/// Returns 99999 if no valid location found.
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

    loc->lat = 99999;
    loc->lon = 99999;

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

    if (loc->lon == 99999) {
        loc->lat = 99999;
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

    strcpy(folder, _programData.chart);
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
