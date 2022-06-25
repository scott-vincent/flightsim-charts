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

const char* IFR_Aircraft = "Airbus A320 Neo Asobo";
const char* VFR_Aircraft = "DA40-NG Asobo";

extern HANDLE hSimConnect;
extern bool _connected;
extern bool _listening;
extern SOCKET _sockfd;
extern SOCKET _sendSockfd;
extern sockaddr_in _sendAddr;
extern char* _listenerData;
extern char _listenerBounds[256];
extern int _snapshotDataSize;
extern int _aiAircraftCount;
extern AI_Aircraft _aiAircraft[Max_AI_Aircraft];
extern int _aiFixedCount;
extern AI_Fixed _aiFixed[Max_AI_Fixed];


void listenerInit()
{
    char* remoteIp = getenv("fr24server");
    if (!remoteIp) {
        printf("No fr24server\n");
        return;
    }

    printf("fr24server: %s\n", remoteIp);

    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0) {
        printf("Listener failed to initialise Windows Sockets: %d\n", err);
        return;
    }

    // Create a UDP socket
    if ((_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        printf("Listener failed to create UDP socket\n");
        return;
    }

    int opt = 1;
    setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(Port);

    if (bind(_sockfd, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Listener failed to bind to localhost port %d: %ld\n", Port, WSAGetLastError());
        closesocket(_sockfd);
        return;
    }

    // Create a UDP send socket
    if ((_sendSockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        printf("Listener failed to create UDP send socket\n");
        closesocket(_sockfd);
        return;
    }

    setsockopt(_sendSockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    _sendAddr.sin_family = AF_INET;
    _sendAddr.sin_port = htons(Port);
    inet_pton(AF_INET, remoteIp, &_sendAddr.sin_addr);

    _listenerData = (char*)malloc(MaxDataSize);
    if (_listenerData == NULL) {
        printf("Listener failed to allocate memory");
        closesocket(_sockfd);
        closesocket(_sendSockfd);
        return;
    }

    printf("Listening on port %d\n", Port);
    _listening = true;
}

void listenerCleanup()
{
    if (!_listening) {
        return;
    }

    free(_listenerData);

    closesocket(_sockfd);
    closesocket(_sendSockfd);
    printf("Listener stopped\n");
}

const char* getAircraftType(AI_Aircraft aircraft)
{
    if (strcmp(aircraft.model, "GRND") == 0) {
        // No vehicle model currently
        return VFR_Aircraft;
    }
    else if (strcmp(aircraft.model, "GLID") == 0) {
        // No glider model currently
        return VFR_Aircraft;
    }
    else if (strcmp(aircraft.airline, "N/A") == 0) {
        return VFR_Aircraft;
    }

    return IFR_Aircraft;
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

                if (_aiFixedCount < Max_AI_Fixed) {
                    memcpy(&_aiFixed[_aiFixedCount], &ai, _snapshotDataSize);
                    strcpy(_aiFixed[_aiFixedCount].model, ai.model);
                    strcpy(_aiFixed[_aiFixedCount].tagData.tagText, ai.callsign);
                    // Tag will be created later by single-threaded drawing thread
                    _aiFixed[_aiFixedCount].tagData.tag.bmp = NULL;
                    _aiFixedCount++;
                }
                continue;
            }

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
                            if (SimConnect_AICreateNonATCAircraft(hSimConnect, getAircraftType(ai),
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
                    if (SimConnect_AICreateNonATCAircraft(hSimConnect, getAircraftType(ai),
                        ai.callsign, getAircraftPos(ai), REQ_AI_AIRCRAFT) != 0) {
                        printf("Failed to create AI aircraft: %s\n", ai.callsign);
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

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(_sockfd, &fds);

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 250000;

    sockaddr_in senderAddr;
    int addrSize = sizeof(senderAddr);

    // Remote server now only sends data when poked
    int bytes = sendto(_sendSockfd, request, strlen(request), 0, (SOCKADDR*)&_sendAddr, addrSize);
    if (bytes <= 0) {
        printf("Failed to request remote data\n");
        Sleep(waitMillis);
        return false;
    }

    // Wait for data
    bool success = false;
    int sel = select(FD_SETSIZE, &fds, 0, 0, &timeout);
    if (sel > 0) {
        int bytes = recvfrom(_sockfd, _listenerData, MaxDataSize - 1, 0, (SOCKADDR*)&senderAddr, &addrSize);
        if (bytes > 0) {
            // printf("Received %ld bytes from %s\n", bytes, inet_ntoa(senderAddr.sin_addr));
            _listenerData[bytes] = '\0';
            processData();
            success = true;
        }
    }

    removeStale();
    return success;
}
