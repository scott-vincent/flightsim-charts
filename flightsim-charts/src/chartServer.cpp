#include <WS2tcpip.h>
#include <windows.h>
#include <iostream>
#include "chartServer.h"

// Connects to chart-server (if there is one).
// Comment out next line to disable chart-server.
//#define EXPERIMENTAL


const int Port = 52120;
char serverIp[32];
char data[256];
int response = 0;
bool connected = false;
sockaddr_in sendAddr;
extern char* _chartServer;
extern ChartServerData _chartServerData;
int dataLen = sizeof(ChartServerData);
int skip = 0;


void chartServerInit()
{
#ifdef EXPERIMENTAL

    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0) {
        printf("Chart Server failed to initialise Windows Sockets: %d\n", err);
        return;
    }

    _chartServer = getenv("chart-server");
    if (!_chartServer) {
        return;
    }

    // If it's a hostname need to lookup the IPv4 address
    if (isdigit(*_chartServer)) {
        strcpy(serverIp, _chartServer);
    }
    else {
        struct addrinfo hints, *resp;
        int status;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;  // Only want IPv4 address
        hints.ai_socktype = SOCK_STREAM;

        if ((status = getaddrinfo(_chartServer, NULL, &hints, &resp)) != 0) {
            printf("Failed to get address of chart-server %s: %s\n", _chartServer, gai_strerror(status));
            return;
        }

        struct sockaddr_in* ipv4 = (struct sockaddr_in*)resp->ai_addr;
        void *addr = &(ipv4->sin_addr);
        inet_ntop(resp->ai_family, addr, serverIp, sizeof serverIp);
    }

    printf("chart-server: %s (%s)\n", serverIp, _chartServer);

    sendAddr.sin_family = AF_INET;
    sendAddr.sin_port = htons(Port);
    inet_pton(AF_INET, serverIp, &sendAddr.sin_addr);

    connected = true;
#endif  // EXPERIMENTAL
}

void updateInstrumentHud(LocData* locData)
{
    _chartServerData.lat = locData->loc.lat;
    _chartServerData.lon = locData->loc.lon;
    _chartServerData.heading = locData->heading;
    _chartServerData.headingMag = round(locData->headingMag);
    _chartServerData.altitude = round(locData->alt);
    _chartServerData.speed = round(locData->speed);
    _chartServerData.flaps = round(locData->flaps);
    _chartServerData.trim = round(locData->trim);
    _chartServerData.verticalSpeed = round(locData->verticalSpeed);
    _chartServerData.brake = locData->parkBrake;
    if (_chartServerData.brake == 0 && (locData->leftBrake > 5 || locData->rightBrake > 5)) {
        _chartServerData.brake = -1;
    }

    //printf("hdg: %d, alt: %d, spd: %d, flaps: %d, trim: %d, vs: %d, brake: %d\n",
    //    _chartServerData.headingMag, _chartServerData.altitude, _chartServerData.speed, _chartServerData.flaps,
    //    _chartServerData.trim, _chartServerData.verticalSpeed, _chartServerData.brake);
}

int chartServerSend()
{
#ifdef EXPERIMENTAL
    if (!connected) {
        return 0;
    }

    // Limit data sent
    if (skip > 0) {
        skip--;
        return 0;
    }
    skip = 20;

    SOCKET sockfd;

    // Send data to remote server using a TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Chart Server failed to create TCP socket\n");
        connected = false;
        return 0;
    }
    
    if (connect(sockfd, (sockaddr*)&sendAddr, sizeof(sendAddr)) != 0) {
        printf("Failed to connect to %s port %d\n", serverIp, Port);
        closesocket(sockfd);
        connected = false;
        return 0;
    }

    //printf("Sending %ld bytes to %s\n", dataLen, serverIp);
    int bytes = send(sockfd, (char*)&_chartServerData, dataLen, 0);
    if (bytes <= 0) {
        printf("Chart Server failed to send data\n");
        closesocket(sockfd);
        connected = false;
        return 0;
    }

    // Wait for response
    int timeout = 3000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(int));

    response = 0;
    bytes = recv(sockfd, data, sizeof(data)-1, 0);
    if (bytes > 0) {
        if (bytes == 1) {
            response = data[0];
        }
        else {
            printf("Ignored %ld bytes from chart-server\n", bytes);
        }
    }
    else {
        printf("Timeout receiving response from chart-server\n");
    }

    closesocket(sockfd);
#endif  // EXPERIMENTAL
    return response;
}

void chartServerCleanup()
{
    LocData locData;
    locData.heading = 999;

    int lastHeading = _chartServerData.heading;
    updateInstrumentHud(&locData);

    if (lastHeading != 999) {
        // Let the server know the sim is now disconnected
        chartServerSend();
    }
}
