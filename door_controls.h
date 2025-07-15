#pragma once
#include <string>

void RenderDoorControls();

// Door state interface for main loop
bool IsDoorSerialOpen();
void SendDoorCommand(const std::string& command);
int GetObjectLimit();
void SetObjectLimit(int value);
int& RefPrevCount();
bool IsManualOverride();
