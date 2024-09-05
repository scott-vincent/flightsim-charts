#include <windows.h>
#include <iostream>
#include <thread>

const char* versionString = "v2.4.2";
bool _quit = false;
bool _showAi = false;
bool _noConnect = false;

void server();
void showChart();

int main(int argc, char **argv)
{
    printf("FlightSim Charts %s Copyright (c) 2024 Scott Vincent\n", versionString);

    for (int i = 1; i < argc; i++) {
        if (_stricmp(argv[i], "showai") == 0) {
            _showAi = true;
        }
        if (_stricmp(argv[i], "noconnect") == 0) {
            _noConnect = true;
            _showAi = true;
        }
        if (_stricmp(argv[i], "view") == 0) {
            // View mode to just view charts with no aircraft
            _noConnect = true;
            _showAi = false;
        }
    }

    // Start a thread to connect to FS2020 via SimConnect
    std::thread serverThread(server);

    // Yield so server can start
    Sleep(100);

    showChart();

    // Wait for server to exit
    serverThread.join();

    return 0;
}
