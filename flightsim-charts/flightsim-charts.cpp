#include <windows.h>
#include <iostream>
#include <thread>

const char* versionString = "v0.9.4";
bool _quit = false;

void server();
void showChart();

int main(int argc, char **argv)
{
    printf("FlightSim Charts %s Copyright (c) 2022 Scott Vincent\n", versionString);

    // Start a thread to connect to FS2020 via SimConnect
    std::thread serverThread(server);

    // Yield so server can start
    Sleep(100);

    showChart();

    // Wait for server to exit
    serverThread.join();

    return 0;
}
