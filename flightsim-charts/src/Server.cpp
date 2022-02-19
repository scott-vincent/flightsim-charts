#include <windows.h>
#include <iostream>
#include <thread>
#include "flightsim-charts.h"
#include "simconnect.h"

enum DEFINITION_ID {
    DEF_SELF,
    DEF_ALL,
    DEF_TELEPORT,
    DEF_SNAPSHOT
};

enum REQUEST_ID {
    REQ_SELF,
    REQ_ALL,
    REQ_SNAPSHOT
};

// Externals
extern bool _quit;

// Variables
LocData _locData;
WindData _windData;
LocData _aircraftLoc;
LocData _aircraftOtherLoc[MAX_AIRCRAFT];
LocData _newAircraftOtherLoc[MAX_AIRCRAFT];
int _aircraftOtherCount = 0;
int _locDataSize = sizeof(LocData);
int _windDataSize = sizeof(WindData);
bool _pendingRequest = false;
HANDLE hSimConnect = NULL;
bool _connected = false;
bool _shownMaxExceeded = false;
int _range = AIRCRAFT_RANGE;
TeleportData _teleport;
SnapshotData _snapshot;


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
                printf("Error: SimConnect expected %d bytes but received %d bytes\n", _locDataSize + _windDataSize, dataSize);
                return;
            }

            memcpy(&_locData, &pObjData->dwData, _locDataSize);
            memcpy(&_windData, (char*)&pObjData->dwData + _locDataSize, _windDataSize);

            // Check for fake data that FS2020 sends before you have selected an airport
            if (_locData.lat > -0.02 && _locData.lat < 0.02 && _locData.lon > -0.02 && _locData.lon < 0.02 && (_locData.heading > 359.9 || _locData.heading < 0.1)) {
                _aircraftLoc.lat = MAXINT;
            }
            else {
                memcpy(&_aircraftLoc, &_locData, _locDataSize);
            }

            if (_teleport.inProgress) {
                if (SimConnect_SetDataOnSimObject(hSimConnect, DEF_TELEPORT, SIMCONNECT_OBJECT_ID_USER, 0, 0, _teleport.dataSize, &_teleport) != 0) {
                    printf("Failed to teleport aircraft\n");
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
                if (SimConnect_SetDataOnSimObject(hSimConnect, DEF_SNAPSHOT, SIMCONNECT_OBJECT_ID_USER, 0, 0, _snapshot.dataSize, &_snapshot) != 0) {
                    printf("Failed to restore snapshot data\n");
                }
                _snapshot.restore = false;
            }
            else if (!_pendingRequest) {
                // Request data of aircraft within range
                if (SimConnect_RequestDataOnSimObjectType(hSimConnect, REQ_ALL, DEF_ALL, _range, SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT) != 0) {
                    printf("Failed to request all aircraft data\n");
                }
                else {
                    _pendingRequest = true;
                }
            }

            break;
        }

        case REQ_SNAPSHOT:
        {
            int dataSize = pObjData->dwSize - ((int)(&pObjData->dwData) - (int)pData);
            if (dataSize != _snapshot.dataSize) {
                printf("Error: SimConnect expected %d bytes but received %d bytes\n", _snapshot.dataSize, dataSize);
                return;
            }

            memcpy(&_snapshot, &pObjData->dwData, _snapshot.dataSize);
            _snapshot.save = false;
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
            if (dataSize != _locDataSize) {
                printf("Error: SimConnect expected %d bytes but received %d bytes\n", _locDataSize, dataSize);
                return;
            }

            memcpy(&_locData, &pObjData->dwData, _locDataSize);
            int i = pObjData->dwentrynumber - 1;

            if (i < MAX_AIRCRAFT) {
                //printf("%d: %s - lat: %f  lon: %f  heading:%f  wingSpan: %f\n", i, _locData.callsign, _locData.lat, _locData.lon, _locData.heading, _locData.wingSpan);
                memcpy(&_newAircraftOtherLoc[i], &_locData, _locDataSize);
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

                // Overwrite old locations with new locations
                memcpy(&_aircraftOtherLoc, &_newAircraftOtherLoc, _locDataSize * count);
                _aircraftOtherCount = count;
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
    // SimConnect variables
    addDataDef(DEF_SELF, "Plane Latitude", "Degrees");
    addDataDef(DEF_SELF, "Plane Longitude", "Degrees");
    addDataDef(DEF_SELF, "Plane Heading Degrees True", "Degrees");
    addDataDef(DEF_SELF, "Wing Span", "Feet");
    addDataDef(DEF_SELF, "Atc Id", "string");
    addDataDef(DEF_SELF, "Atc Model", "string");
    addDataDef(DEF_SELF, "Ambient Wind Direction", "Degrees");
    addDataDef(DEF_SELF, "Ambient Wind Velocity", "Knots");

    addDataDef(DEF_ALL, "Plane Latitude", "Degrees");
    addDataDef(DEF_ALL, "Plane Longitude", "Degrees");
    addDataDef(DEF_ALL, "Plane Heading Degrees True", "Degrees");
    addDataDef(DEF_ALL, "Wing Span", "Feet");
    addDataDef(DEF_ALL, "Atc Id", "string");
    addDataDef(DEF_ALL, "Atc Model", "string");

    addDataDef(DEF_TELEPORT, "Plane Latitude", "Degrees");
    addDataDef(DEF_TELEPORT, "Plane Longitude", "Degrees");
    addDataDef(DEF_TELEPORT, "Plane Heading Degrees True", "Degrees");
    addDataDef(DEF_TELEPORT, "Plane Bank Degrees", "Degrees");
    addDataDef(DEF_TELEPORT, "Plane Pitch Degrees", "Degrees");
    _teleport.dataSize = 5 * sizeof(double);

    // Constant teleport values
    _teleport.bank = 0;
    _teleport.pitch = 0;

    addDataDef(DEF_SNAPSHOT, "Plane Latitude", "Degrees");
    addDataDef(DEF_SNAPSHOT, "Plane Longitude", "Degrees");
    addDataDef(DEF_SNAPSHOT, "Plane Heading Degrees True", "Degrees");
    addDataDef(DEF_SNAPSHOT, "Plane Bank Degrees", "Degrees");
    addDataDef(DEF_SNAPSHOT, "Plane Pitch Degrees", "Degrees");
    addDataDef(DEF_SNAPSHOT, "Plane Altitude", "Feet");
    addDataDef(DEF_SNAPSHOT, "Airspeed Indicated", "Knots");
    _snapshot.dataSize = 7 * sizeof(double);

    // Start requesting data for our aircraft
    if (SimConnect_RequestDataOnSimObject(hSimConnect, REQ_SELF, DEF_SELF, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_VISUAL_FRAME, 0, 0, 0, 0) != 0) {
        printf("Failed to start requesting own aircraft data\n");
    }
}

void cleanUp()
{
    if (hSimConnect) {
        if (SimConnect_RequestDataOnSimObject(hSimConnect, REQ_SELF, DEF_SELF, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_NEVER, 0, 0, 0, 0) != 0) {
            printf("Failed to stop requesting data\n");
        }

        printf("Disconnecting from MS FS2020\n");
        SimConnect_Close(hSimConnect);
    }

    if (!_quit) {
        printf("Stopping server\n");
        _quit = true;
    }

    printf("Finished\n");
}

void server()
{
    const char WaitMsg[] = "Waiting for local MS FS2020...\n";

    printf(WaitMsg);
    _connected = false;
    _aircraftLoc.lat = MAXINT;
    _aircraftOtherCount = 0;
    _teleport.inProgress = false;
    _snapshot.loc.lat = MAXINT;
    _snapshot.save = false;
    _snapshot.restore = false;

    HRESULT result;

    int loopMillis = 10;
    int retryDelay = 0;
    while (!_quit)
    {
        if (_connected) {
            result = SimConnect_CallDispatch(hSimConnect, MyDispatchProc, NULL);
            if (result != 0) {
                printf("Disconnected from MS FS2020\n");
                _connected = false;
                _aircraftLoc.lat = MAXINT;
                _aircraftOtherCount = 0;
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

        Sleep(loopMillis);
    }

    cleanUp();
}
