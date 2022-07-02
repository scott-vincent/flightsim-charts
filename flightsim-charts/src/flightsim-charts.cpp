#include <windows.h>
#include <iostream>
#include <thread>

const char* versionString = "v1.6.2";
bool _quit = false;
bool _showAi = false;

void server();
void showChart();

int main(int argc, char **argv)
{
    printf("FlightSim Charts %s Copyright (c) 2022 Scott Vincent\n", versionString);

    if (argc > 1 && _stricmp(argv[1], "showai") == 0) {
        _showAi = true;
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
