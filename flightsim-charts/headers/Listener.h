#pragma once

void listenerInit();
void listenerCleanup();
bool listenerRead(const char* request, int waitMillis, bool immediate);
