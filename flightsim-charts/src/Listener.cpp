#include <WS2tcpip.h>
#include <windows.h>
#include <iostream>
#include "Server.h"
#include "flightsim-charts.h"
#include "simconnect.h"

/// Read aircraft data from an external source passed to our port
/// and inject it into FS2020. This is optional functionality in case
/// you want to display additional aircraft on the chart.

const int Port = 52025;
const int MaxDataSize = 65536;
const int IntervalSecs = 6;
const int StaleSecs = 20;

const char* IFR_Default = "Airbus A320 Neo Asobo";
const char* VFR_Default = "DA40-NG Asobo";

extern HANDLE hSimConnect;
extern bool _connected;
extern bool _listening;
extern char* _remoteIp;
extern SOCKET _sockfd;
extern sockaddr_in _sendAddr;
extern char* _listenerData;
extern char _listenerBounds[256];
extern int _snapshotDataSize;
extern int _aiAircraftCount;
extern AI_Aircraft _aiAircraft[Max_AI_Aircraft];
extern int _aiFixedCount;
extern AI_Fixed _aiFixed[Max_AI_Fixed];
extern int _aiModelMatchCount;
extern AI_ModelMatch _aiModelMatch[Max_AI_ModelMatch];


void getModelMatch(const char* modelMatchFile)
{
    FILE* inf = fopen(modelMatchFile, "r");
    if (inf == NULL) {
        printf("Failed to open model match file: %s\n", modelMatchFile);
        return;
    }

    int linenum = 0;
    char line[16384];
    char prefix[7];
    char model[5];

    while (fgets(line, sizeof(line), inf)) {
        linenum++;
        char* pos = line;
        while (*pos == ' ') {
            pos++;
        }
        if (_strnicmp(pos, "<ModelMatchRule ", 16) == 0) {
            pos += 16;
            if (_strnicmp(pos, "CallsignPrefix=\"", 16) != 0) {
                printf("Bad model match CallsignPrefix at line %d\n", linenum);
                continue;
            }
            pos += 16;
            char* endPos = strchr(pos, '\"');
            if (!endPos || endPos - pos > 6) {
                printf("Bad model match CallsignPrefix value at line %d\n", linenum);
                continue;
            }
            *endPos = '\0';
            strcpy(prefix, pos);
            pos = endPos + 2;

            if (_strnicmp(pos, "TypeCode=\"", 10) != 0) {
                printf("Bad model match TypeCode at line %d\n", linenum);
                continue;
            }
            pos += 10;
            endPos = strchr(pos, '\"');
            if (!endPos || endPos - pos > 4) {
                printf("Bad model match TypeCode value at line %d\n", linenum);
                continue;
            }
            *endPos = '\0';
            strcpy(model, pos);
            pos = endPos + 2;

            if (_strnicmp(pos, "ModelName=\"", 11) != 0) {
                printf("Bad model match ModelName at line %d\n", linenum);
                continue;
            }
            pos += 11;
            endPos = strchr(pos, '\"');
            if (!endPos) {
                printf("Bad model match ModelName value at line %d\n", linenum);
                continue;
            }
            *endPos = '\0';

            if (_aiModelMatchCount == Max_AI_ModelMatch) {
                printf("Maxmimum number of model matches exceeded\n");
                break;
            }

            _aiModelMatchCount++;

            // Create a space in correct pos (sorted on key)
            int i = _aiModelMatchCount - 1;
            for (; i > 0; i--) {
                int prefixCmp = strcmp(prefix, _aiModelMatch[i - 1].prefix);
                if (prefixCmp > 0) {
                    break;
                }
                if (prefixCmp == 0 && strcmp(model, _aiModelMatch[i - 1].model) >= 0) {
                    break;
                }
                memcpy(&_aiModelMatch[i], &_aiModelMatch[i-1], sizeof(AI_ModelMatch));
            }

            strcpy(_aiModelMatch[i].prefix, prefix);
            strcpy(_aiModelMatch[i].model, model);
            _aiModelMatch[i].modelNames = (char*)malloc(strlen(pos) + 1);
            if (_aiModelMatch[i].modelNames) {
                strcpy(_aiModelMatch[i].modelNames, pos);
            }
            else {
                printf("Failed to allocate memory for models\n");
            }
        }
    }

    fclose(inf);
}

void listenerInit()
{
    srand(time(NULL));

    _remoteIp = getenv("fr24server");
    if (!_remoteIp) {
        printf("No fr24server\n");
        return;
    }

    printf("fr24server: %s\n", _remoteIp);

    char* modelMatchFile = getenv("fr24modelmatch");
    if (modelMatchFile) {
        printf("fr24modelmatch: %s\n", modelMatchFile);
        getModelMatch(modelMatchFile);
    }

    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0) {
        printf("Listener failed to initialise Windows Sockets: %d\n", err);
        return;
    }

    _sendAddr.sin_family = AF_INET;
    _sendAddr.sin_port = htons(Port);
    inet_pton(AF_INET, _remoteIp, &_sendAddr.sin_addr);

    _listenerData = (char*)malloc(MaxDataSize);
    if (_listenerData == NULL) {
        printf("Listener failed to allocate memory");
        closesocket(_sockfd);
        return;
    }

    _listening = true;
}

void listenerCleanup()
{
    if (!_listening) {
        return;
    }

    free(_listenerData);

    for (int i = 0; i < _aiModelMatchCount; i++) {
        if (_aiModelMatch[i].modelNames) {
            free(_aiModelMatch[i].modelNames);
        }
    }

    closesocket(_sockfd);
    printf("Listener stopped\n");
}

char* getModelName(AI_Aircraft aircraft)
{
    static char modelName[1024];

    if (strcmp(aircraft.model, "GRND") == 0 || strcmp(aircraft.model, "GLID") == 0) {
        strcpy(modelName, VFR_Default);
        return modelName;
    }

    if (_aiModelMatchCount == 0) {
        if (strcmp(aircraft.airline, "N/A") == 0) {
            strcpy(modelName, VFR_Default);
            return modelName;
        }
        else {
            strcpy(modelName, IFR_Default);
            return modelName;
        }
    }

    // Model Match file is sorted so binary chop
    int lowPos = 0;
    int highPos = _aiModelMatchCount - 1;
    int foundPos;

    while (true) {
        foundPos = (lowPos + highPos) / 2;
        int prefixCmp = strncmp(aircraft.callsign, _aiModelMatch[foundPos].prefix, strlen(_aiModelMatch[foundPos].prefix));
        int modelCmp = strcmp(aircraft.model, _aiModelMatch[foundPos].model);

        if (prefixCmp == 0 && modelCmp == 0) {
            break;
        }

        if (prefixCmp < 0 || (prefixCmp == 0 && modelCmp < 0)) {
            if (foundPos == lowPos) {
                strcpy(modelName, VFR_Default);
                return modelName;
            }
            highPos = foundPos - 1;
        }
        else {
            if (foundPos == highPos) {
                strcpy(modelName, VFR_Default);
                return modelName;
            }
            lowPos = foundPos + 1;
        }
    }

    // If multiple liveries choose one at random
    char *startPos[1024];
    startPos[0] = _aiModelMatch[foundPos].modelNames;
    int count = 0;

    while (true) {
        count++;
        startPos[count] = strstr(startPos[count - 1], "//");
        if (startPos[count]) {
            startPos[count] += 2;
        }
        else {
            startPos[count] = startPos[count-1] + strlen(startPos[count-1]) + 2;
            break;
        }
    }

    int choice = rand() % count;
    int len = (startPos[choice + 1] - startPos[choice]) - 2;
    strncpy(modelName, startPos[choice], len);
    modelName[len] = '\0';

    return modelName;
}

SIMCONNECT_DATA_INITPOSITION getAircraftPos(AI_Aircraft aircraft)
{
    SIMCONNECT_DATA_INITPOSITION pos;

    pos.Latitude = aircraft.loc.lat;
    pos.Longitude = aircraft.loc.lon;
    pos.Heading = aircraft.heading;
    pos.Altitude = aircraft.alt;
    pos.Airspeed = aircraft.speed;
    pos.Pitch = 0;
    pos.Bank = 0;

    return pos;
}

void processData()
{
    // Only need to add fixed once as the same data is sent every time
    char* data = _listenerData;

    while (*data != '\0') {
        char* line = data;
        char* endLine = strchr(line, '\n');
        if (!endLine) {
            return;
        }
        *endLine = '\0';
        data += strlen(line) + 1;

        AI_Aircraft ai;
        int cols = sscanf(line, "%15[^,],%15[^,],%15[^,],%lf,%lf,%lf,%lf,%lf",
            ai.callsign, ai.airline, ai.model, &ai.loc.lat, &ai.loc.lon, &ai.heading, &ai.alt, &ai.speed);

        if (cols != 8) {
            printf("Listener bad data ignored: %s (%d)\n", line, cols);
        }
        else {
            if (strcmp(ai.model, "N/A") == 0) {
                strcpy(ai.model, "GRND");
            }

            ai.bank = 0;
            ai.pitch = 0;

            if (strcmp(ai.model, "AIRP") == 0 || strcmp(ai.model, "WAYP") == 0) {
                if (strcmp(ai.callsign, "Home") == 0) {
                    char* home = getenv("fr24home");
                    if (home) {
                        double lat, lon;
                        cols = sscanf(home, "%lf,%lf", &lat, &lon);
                        if (cols == 2) {
                            ai.loc.lat = lat;
                            ai.loc.lon = lon;
                        }
                    }
                    printf("fr24home: %lf.%lf\n", ai.loc.lat, ai.loc.lon);

                    // Change bounds when home changed
                    char* bounds = getenv("fr24bounds");
                    if (bounds) {
                        strcpy(_listenerBounds, bounds);
                    }
                    else {
                        sprintf(_listenerBounds, "%lf,%lf,%lf,%lf",
                            ai.loc.lat + 1.4, ai.loc.lat - 1.4, ai.loc.lon - 3.2, ai.loc.lon + 3.2);
                    }
                    printf("fr24bounds: %s\n", _listenerBounds);
                }

                int i = 0;
                for (; i < _aiFixedCount; i++) {
                    if (strcmp(ai.callsign, _aiFixed[i].tagData.tagText) == 0) {
                        // Update waypoint
                        memcpy(&_aiFixed[_aiFixedCount], &ai, _snapshotDataSize);
                        strcpy(_aiFixed[_aiFixedCount].model, ai.model);
                        break;
                    }
                }

                if (i == _aiFixedCount && _aiFixedCount < Max_AI_Fixed) {
                    // Create waypoint
                    memcpy(&_aiFixed[_aiFixedCount], &ai, _snapshotDataSize);
                    strcpy(_aiFixed[_aiFixedCount].tagData.tagText, ai.callsign);
                    strcpy(_aiFixed[_aiFixedCount].model, ai.model);
                    // Tag will be created later by single-threaded drawing thread
                    _aiFixed[_aiFixedCount].tagData.tag.bmp = NULL;
                    _aiFixedCount++;
                }
            }
            else {
                int i = 0;
                for (; i < _aiAircraftCount; i++) {
                    if (strcmp(ai.callsign, _aiAircraft[i].callsign) == 0) {
                        // Update aircraft
                        memcpy(&_aiAircraft[i], &ai, _snapshotDataSize);
                        strcpy(_aiAircraft[i].airline, ai.airline);
                        strcpy(_aiAircraft[i].model, ai.model);
                        time(&_aiAircraft[i].lastUpdated);

                        if (_connected && _aiAircraft[i].objectId != -1) {
                            if (SimConnect_SetDataOnSimObject(hSimConnect, DEF_SNAPSHOT, _aiAircraft[i].objectId, 0, 0, _snapshotDataSize, &_aiAircraft[i]) != 0) {
                                if (SimConnect_AICreateNonATCAircraft(hSimConnect, getModelName(ai),
                                    ai.callsign, getAircraftPos(ai), REQ_AI_AIRCRAFT) != 0) {
                                    printf("Failed to create/update AI aircraft: %s\n", ai.callsign);
                                }
                            }
                        }
                        break;
                    }
                }

                if (i == _aiAircraftCount && _aiAircraftCount < Max_AI_Aircraft) {
                    // Add aircraft
                    memcpy(&_aiAircraft[i], &ai, _snapshotDataSize);
                    strcpy(_aiAircraft[i].callsign, ai.callsign);
                    strcpy(_aiAircraft[i].airline, ai.airline);
                    strcpy(_aiAircraft[i].model, ai.model);
                    time(&_aiAircraft[i].lastUpdated);
                    _aiAircraft[i].objectId = -1;

                    // Create a tag so we can still draw the AI aircraft if FS2020 is disconnected
                    createTagText(ai.callsign, ai.model, _aiAircraft[i].tagData.tagText);
                    // Tag will be created later by single-threaded drawing thread
                    _aiAircraft[i].tagData.tag.bmp = NULL;

                    _aiAircraftCount++;

                    if (_connected) {
                        if (SimConnect_AICreateNonATCAircraft(hSimConnect, getModelName(ai),
                            ai.callsign, getAircraftPos(ai), REQ_AI_AIRCRAFT) != 0) {
                            printf("Failed to create AI aircraft: %s\n", ai.callsign);
                        }
                    }
                }
            }
        }
    }
}

void removeStale()
{
    // Delete stale aircraft (no update in the last 30 seconds)
    time_t now;
    time(&now);

    for (int i = 0; i < _aiAircraftCount; i++) {
        if (now - _aiAircraft[i].lastUpdated > StaleSecs) {
            // Remove aircraft
            cleanupBitmap(_aiAircraft[i].tagData.tag.bmp);

            if (_aiAircraft[i].objectId != -1) {
                if (SimConnect_AIRemoveObject(hSimConnect, _aiAircraft[i].objectId, REQ_AI_AIRCRAFT) != 0) {
                    printf("Failed to remove AI aircraft: %s\n", _aiAircraft[i].callsign);
                }
            }

            if (i < _aiAircraftCount - 1) {
                memcpy(&_aiAircraft[i], &_aiAircraft[i + 1], sizeof(AI_Aircraft) * (_aiAircraftCount - i));
            }
            _aiAircraftCount--;
        }
    }
}

/// <summary>
/// If there is any data on the port, read and process it.
/// 
/// </summary>
bool listenerRead(const char* request, int waitMillis)
{
    static time_t lastRequest = 0;

    if (!_listening) {
        Sleep(waitMillis);
        return false;
    }

    time_t now;
    time(&now);

    if (waitMillis > 0 && now - lastRequest < IntervalSecs) {
        Sleep(waitMillis);
        return false;
    }

    time(&lastRequest);

    // Request data from remote server using a TCP socket
    if ((_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Listener failed to create TCP socket\n");
        closesocket(_sockfd);
        return false;
    }

    if (connect(_sockfd, (sockaddr*)&_sendAddr, sizeof(_sendAddr)) != 0) {
        printf("Failed to connect to %s port %d\n", _remoteIp, Port);
        closesocket(_sockfd);
        Sleep(waitMillis);
        return false;
    }

    int bytes = send(_sockfd, request, strlen(request), 0);
    if (bytes <= 0) {
        printf("Failed to request remote data\n");
        closesocket(_sockfd);
        Sleep(waitMillis);
        return false;
    }

    // Wait for data
    bool success = false;
    bytes = recv(_sockfd, _listenerData, MaxDataSize - 1, 0);
    if (bytes > 0) {
        // printf("Received %ld bytes from %s\n", bytes, _remoteIp);
        _listenerData[bytes] = '\0';
        processData();
        success = true;
    }

    closesocket(_sockfd);
    removeStale();
    return success;
}
