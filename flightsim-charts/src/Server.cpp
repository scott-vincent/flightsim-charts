#include <windows.h>
#include <iostream>
#include <thread>
#include "flightsim-charts.h"
#include "ChartCoords.h"
#include "Server.h"
#include "Listener.h"
#include "simconnect.h"

// Externals
extern bool _quit;
extern bool _showAi;


// Variables
LocData _locData;
WindData _windData;
LocData _aircraftData;
OtherData _otherData;
OtherData _otherAircraftData[MAX_AIRCRAFT];
OtherData _newOtherAircraftData[MAX_AIRCRAFT];
int _otherAircraftCount = 0;
int _locDataSize = sizeof(LocData);
int _windDataSize = sizeof(WindData);
int _otherDataSize = sizeof(OtherData);
int _snapshotDataSize = 7 * sizeof(double);
bool _pendingRequest = false;
HANDLE hSimConnect = NULL;
bool _connected = false;
bool _shownMaxExceeded = false;
int _range;
bool _maxRange = true;
TeleportData _teleport;
SnapshotData _snapshot;
FollowData _follow;
bool _listening = false;
char* _remoteIp;
SOCKET _sockfd;
sockaddr_in _sendAddr;
char* _listenerData;
char* _listenerHome;
bool _listenerInitFetch = false;
int _aiAircraftCount = 0;
AI_Aircraft _aiAircraft[Max_AI_Aircraft];
int _aiFixedCount = 0;
AI_Fixed _aiFixed[Max_AI_Fixed];
int _aiModelMatchCount = 0;
AI_ModelMatch _aiModelMatch[Max_AI_ModelMatch];
AI_Trail _aiTrail;
char _watchCallsign[16];
bool _watchInProgress = false;


void stopFollowing(bool force = false)
{
    if (!force && *_follow.callsign == '\0') {
        return;
    }

    if (SimConnect_RequestDataOnSimObject(hSimConnect, REQ_SELF, DEF_SELF, _follow.aircraftId, SIMCONNECT_PERIOD_NEVER, 0, 0, 0, 0) != 0) {
        printf("Failed to stop requesting followed aircraft data\n");
    }

    if (SimConnect_RequestDataOnSimObject(hSimConnect, REQ_SELF, DEF_SELF, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_VISUAL_FRAME, 0, 0, 0, 0) != 0) {
        printf("Failed to start requesting own aircraft data\n");
    }

    *_follow.callsign = '\0';
    _follow.inProgress = false;
}

void CALLBACK MyDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext)
{
    switch (pData->dwID)
    {
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
    {
        auto pObjData = static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(pData);

        switch (pObjData->dwRequestID)
        {
        case REQ_SELF:
        {
            int dataSize = pObjData->dwSize - ((int)(&pObjData->dwData) - (int)pData);
            if (dataSize != _locDataSize + _windDataSize) {
                printf("Error: SimConnect self expected %d bytes but received %d bytes\n", _locDataSize + _windDataSize, dataSize);
                return;
            }

            memcpy(&_locData, &pObjData->dwData, _locDataSize);
            memcpy(&_windData, (char*)&pObjData->dwData + _locDataSize, _windDataSize);

            // Check for fake data that FS2020 sends before you have selected an airport
            if (_locData.loc.lat > -0.02 && _locData.loc.lat < 0.02 && _locData.loc.lon > -0.02 && _locData.loc.lon < 0.02 && (_locData.heading > 359.9 || _locData.heading < 0.1)) {
                _aircraftData.loc.lat = MAXINT;
            }
            else {
                memcpy(&_aircraftData, &_locData, _locDataSize);
            }

            if (_teleport.inProgress) {
                stopFollowing();
                if (_teleport.setAltSpeed) {
                    // Include alt and speed
                    if (SimConnect_SetDataOnSimObject(hSimConnect, DEF_SNAPSHOT, SIMCONNECT_OBJECT_ID_USER, 0, 0, _snapshot.dataSize, &_teleport) != 0) {
                        printf("Failed to teleport aircraft\n");
                    }
                }
                else {
                    // Don't include alt and speed (want them to stay the same)
                    if (SimConnect_SetDataOnSimObject(hSimConnect, DEF_TELEPORT, SIMCONNECT_OBJECT_ID_USER, 0, 0, _teleport.dataSize, &_teleport) != 0) {
                        printf("Failed to teleport aircraft\n");
                    }
                }
                _teleport.inProgress = false;
            }
            else if (_snapshot.save) {
                if (SimConnect_RequestDataOnSimObject(hSimConnect, REQ_SNAPSHOT, DEF_SNAPSHOT, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE, 0, 0, 0, 0) != 0) {
                    printf("Failed to request snapshot data\n");
                    _snapshot.save = false;
                }
                // Save not complete until data received
            }
            else if (_snapshot.restore) {
                stopFollowing();
                if (SimConnect_SetDataOnSimObject(hSimConnect, DEF_SNAPSHOT, SIMCONNECT_OBJECT_ID_USER, 0, 0, _snapshot.dataSize, &_snapshot) != 0) {
                    printf("Failed to restore snapshot data\n");
                }
                _snapshot.restore = false;
            }
            else if (_follow.inProgress && *_follow.callsign == '\0') {
                stopFollowing(true);
            }

            if (!_follow.inProgress && *_follow.callsign != '\0') {
                // Apply followed aircraft data to own aircraft.
                // Need to make adjustments so we get a good cockpit view.
                adjustFollowLocation(&_locData, _follow.ownWingSpan);
                if (SimConnect_SetDataOnSimObject(hSimConnect, DEF_SNAPSHOT, SIMCONNECT_OBJECT_ID_USER, 0, 0, _snapshot.dataSize, &_locData) != 0) {
                    printf("Failed to apply follow data\n");
                }
            }

            break;
        }

        case REQ_SNAPSHOT:
        {
            int dataSize = pObjData->dwSize - ((int)(&pObjData->dwData) - (int)pData);
            if (dataSize != _snapshot.dataSize) {
                printf("Error: SimConnect snapshot expected %d bytes but received %d bytes\n", _snapshot.dataSize, dataSize);
                return;
            }

            memcpy(&_snapshot, &pObjData->dwData, _snapshot.dataSize);
            _snapshot.save = false;
            break;
        }

        case REQ_AI_AIRCRAFT:
        {
            printf("Received AI aircraft\n");
            break;
        }

        default:
        {
            printf("SimConnect unknown request id: %ld\n", pObjData->dwRequestID);
            break;
        }
        }
        break;
    }

    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE:
    {
        auto pObjData = static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(pData);

        switch (pObjData->dwRequestID)
        {
        case REQ_ALL:
        {
            int dataSize = pObjData->dwSize - ((int)(&pObjData->dwData) - (int)pData);
            if (dataSize != _otherDataSize) {
                printf("Error: SimConnect other expected %d bytes but received %d bytes\n", _otherDataSize, dataSize);
                _pendingRequest = false;
                return;
            }

            memcpy(&_otherData, &pObjData->dwData, _otherDataSize);
            int i = pObjData->dwentrynumber - 1;

            // Use the AI model (not matched model) if it's an AI aircraft
            for (int j = 0; j < _aiAircraftCount; j++) {
                if (strcmp(_otherData.callsign, _aiAircraft[j].callsign) == 0) {
                    strcpy(_otherData.model, _aiAircraft[j].model);
                    _aiAircraft[j].objectId = pObjData->dwObjectID;
                    break;
                }
            }

            // Make sure followed aircraft still exists
            if (*_follow.callsign != '\0' && strcmp(_otherData.callsign, _follow.callsign) == 0) {
                _follow.aircraftId = pObjData->dwObjectID;
            }

            if (_follow.inProgress && *_follow.callsign != '\0' && strcmp(_otherData.callsign, _follow.callsign) == 0) {
                // Start following
                if (SimConnect_RequestDataOnSimObject(hSimConnect, REQ_SELF, DEF_SELF, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_NEVER, 0, 0, 0, 0) != 0) {
                    printf("Failed to stop requesting own aircraft data\n");
                }
                if (SimConnect_RequestDataOnSimObject(hSimConnect, REQ_SELF, DEF_SELF, _follow.aircraftId, SIMCONNECT_PERIOD_VISUAL_FRAME, 0, 0, 0, 0) != 0) {
                    printf("Failed to start requesting followed aircraft data\n");
                }
                _follow.inProgress = false;
            }

            // Exclude static aircraft
            if (i < MAX_AIRCRAFT && strcmp(_otherData.callsign, "ASXGSA") != 0 && strcmp(_otherData.callsign, "AS-MTP2") != 0) {
                //printf("%d: %s - lat: %f  lon: %f  heading:%f  wingSpan: %f\n", i, _locData.callsign, _locData.lat, _locData.lon, _locData.heading, _locData.wingSpan);
                memcpy(&_newOtherAircraftData[i], &_otherData, _otherDataSize);
            }
            else if (!_shownMaxExceeded) {
                _shownMaxExceeded = true;
                printf("Max limit of %d aircraft in range exceeded. Actual: %d\n",
                    MAX_AIRCRAFT, pObjData->dwoutof);
            }

            // Once all aircraft received, update global data
            if (pObjData->dwentrynumber == pObjData->dwoutof) {
                int count = pObjData->dwoutof;
                if (count > MAX_AIRCRAFT) {
                    count = MAX_AIRCRAFT;
                }

                // If followed aircraft has disappeared, stop following it
                if (*_follow.callsign != '\0' && _follow.aircraftId == MAXINT) {
                    stopFollowing();
                }

                // Overwrite old locations with new locations
                memcpy(&_otherAircraftData, &_newOtherAircraftData, _otherDataSize * count);
                _otherAircraftCount = count;
                _pendingRequest = false;
            }

            break;
        }

        default:
        {
            printf("SimConnect unknown request id: %ld\n", pObjData->dwRequestID);
            break;
        }
        }
        break;
    }

    case SIMCONNECT_RECV_ID_QUIT:
    {
        // Comment out next line to stay running when FS2020 quits
        //_quit = true;
        printf("SimConnect Quit\n");
        break;
    }

    }
}

void getAllAircract() {
    if (_pendingRequest || _aircraftData.loc.lat == MAXINT) {
        return;
    }

    // Request data of aircraft within range
    if (SimConnect_RequestDataOnSimObjectType(hSimConnect, REQ_ALL, DEF_ALL, _range, SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT) != 0) {
        printf("Failed to request all aircraft data\n");
        return;
    }

    _follow.aircraftId = MAXINT;
    _pendingRequest = true;
}

void addDataDef(SIMCONNECT_DATA_DEFINITION_ID defId, const char *var, const char *units)
{
    HRESULT result;
    
    if (strcmp(units, "string") == 0) {
        result = SimConnect_AddToDataDefinition(hSimConnect, defId, var, NULL, SIMCONNECT_DATATYPE_STRING32);
    }
    else {
        result = SimConnect_AddToDataDefinition(hSimConnect, defId, var, units);
    }

    if (result != 0) {
        printf("SimConnect Data definition failed: %s (%s)\n", var, units);
    }
}

void init()
{
    addDataDef(DEF_TELEPORT, "Plane Latitude", "Degrees");
    addDataDef(DEF_TELEPORT, "Plane Longitude", "Degrees");
    addDataDef(DEF_TELEPORT, "Plane Heading Degrees True", "Degrees");
    addDataDef(DEF_TELEPORT, "Plane Bank Degrees", "Degrees");
    addDataDef(DEF_TELEPORT, "Plane Pitch Degrees", "Degrees");
    _teleport.dataSize = 5 * sizeof(double);

    // First part of SNAPSHOT must match teleport
    addDataDef(DEF_SNAPSHOT, "Plane Latitude", "Degrees");
    addDataDef(DEF_SNAPSHOT, "Plane Longitude", "Degrees");
    addDataDef(DEF_SNAPSHOT, "Plane Heading Degrees True", "Degrees");
    addDataDef(DEF_SNAPSHOT, "Plane Bank Degrees", "Degrees");
    addDataDef(DEF_SNAPSHOT, "Plane Pitch Degrees", "Degrees");
    // Rest of SNAPSHOT data after teleport
    addDataDef(DEF_SNAPSHOT, "Plane Alt Above Ground", "Feet");
    addDataDef(DEF_SNAPSHOT, "Airspeed Indicated", "Knots");
    _snapshot.dataSize = _snapshotDataSize;

    // First part of SELF must match snapshot
    addDataDef(DEF_SELF, "Plane Latitude", "Degrees");
    addDataDef(DEF_SELF, "Plane Longitude", "Degrees");
    addDataDef(DEF_SELF, "Plane Heading Degrees True", "Degrees");
    addDataDef(DEF_SELF, "Plane Bank Degrees", "Degrees");
    addDataDef(DEF_SELF, "Plane Pitch Degrees", "Degrees");
    addDataDef(DEF_SELF, "Plane Alt Above Ground", "Feet");
    addDataDef(DEF_SELF, "Airspeed Indicated", "Knots");
    // Rest of SELF data after snapshot
    addDataDef(DEF_SELF, "Wing Span", "Feet");
    addDataDef(DEF_SELF, "Atc Id", "string");
    addDataDef(DEF_SELF, "Atc Model", "string");
    addDataDef(DEF_SELF, "Ambient Wind Direction", "Degrees");
    addDataDef(DEF_SELF, "Ambient Wind Velocity", "Knots");

    addDataDef(DEF_ALL, "Plane Latitude", "Degrees");
    addDataDef(DEF_ALL, "Plane Longitude", "Degrees");
    addDataDef(DEF_ALL, "Plane Heading Degrees True", "Degrees");
    addDataDef(DEF_ALL, "Plane Alt Above Ground", "Feet");
    addDataDef(DEF_ALL, "Airspeed Indicated", "Knots");
    addDataDef(DEF_ALL, "Wing Span", "Feet");
    addDataDef(DEF_ALL, "Atc Id", "string");
    addDataDef(DEF_ALL, "Atc Model", "string");

    // Constant teleport values
    _teleport.bank = 0;
    _teleport.pitch = 0;
    _teleport.alt = 6;
    _teleport.speed = 0;

    // Start requesting data for our aircraft
    if (SimConnect_RequestDataOnSimObject(hSimConnect, REQ_SELF, DEF_SELF, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_VISUAL_FRAME, 0, 0, 0, 0) != 0) {
        printf("Failed to start requesting own aircraft data\n");
    }
}

void cleanUp()
{
    if (hSimConnect) {
        if (*_follow.callsign != '\0') {
            if (SimConnect_RequestDataOnSimObject(hSimConnect, REQ_SELF, DEF_SELF, _follow.aircraftId, SIMCONNECT_PERIOD_NEVER, 0, 0, 0, 0) != 0) {
                printf("Failed to stop requesting followed aircraft data\n");
            }
        }
        else {
            if (SimConnect_RequestDataOnSimObject(hSimConnect, REQ_SELF, DEF_SELF, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_NEVER, 0, 0, 0, 0) != 0) {
                printf("Failed to stop requesting data\n");
            }
        }

        printf("Disconnecting from MS FS2020\n");
        SimConnect_Close(hSimConnect);
    }

    if (!_quit) {
        printf("Stopping server\n");
        _quit = true;
    }

    listenerCleanup();

    printf("Finished\n");
}

void server()
{
    const char WaitMsg[] = "Waiting for local MS FS2020...\n";

    printf(WaitMsg);
    _connected = false;
    _aircraftData.loc.lat = MAXINT;
    _otherAircraftCount = 0;
    _teleport.inProgress = false;
    _snapshot.loc.lat = MAXINT;
    _snapshot.save = false;
    _snapshot.restore = false;
    *_follow.callsign = '\0';
    _follow.inProgress = false;
    _range = _maxRange ? MAX_RANGE : AIRCRAFT_RANGE;

    HRESULT result;

    if (_showAi) {
        listenerInit();
    }

    int loopMillis = 10;
    int retryDelay = 0;
    while (!_quit)
    {
        if (_connected) {
            result = SimConnect_CallDispatch(hSimConnect, MyDispatchProc, NULL);
            if (result == 0) {
                getAllAircract();
            }
            else {
                printf("Disconnected from MS FS2020\n");
                _connected = false;
                _pendingRequest = false;
                *_follow.callsign = '\0';
                _aircraftData.loc.lat = MAXINT;
                _otherAircraftCount = 0;
                printf(WaitMsg);
            }
        }
        else if (retryDelay > 0) {
            retryDelay--;
        }
        else {
            result = SimConnect_Open(&hSimConnect, "FlightSim Charts", NULL, 0, 0, 0);
            if (result == 0) {
                printf("Connected to MS FS2020\n");
                init();
                _connected = true;
            }
            else {
                retryDelay = 200;
            }
        }

        if (_showAi) {
            char request[256];
            bool immediate = false;

            if (_watchInProgress) {
                sprintf(request, "watch,%s", _watchCallsign);
                immediate = true;
            }
            else if (_listenerInitFetch) {
                if (_listenerHome) {
                    sprintf(request, "home,%s", _listenerHome);
                }
                else {
                    strcpy(request, "wayp");
                }
                immediate = true;
            }
            else {
                strcpy(request, "fr24");
            }

            if (listenerRead(request, loopMillis, immediate) && _listenerInitFetch) {
                if (_listenerHome) {
                    _listenerHome = NULL;
                }
                else if (strcmp(request, "wayp") == 0) {
                    _listenerInitFetch = false;
                }
            }

            // Don't retry a watch. User must click again.
            if (strncmp(request, "watch", 5) == 0) {
                _watchInProgress = false;
            }
        }
        else {
            Sleep(loopMillis);
        }
    }

    cleanUp();
}
