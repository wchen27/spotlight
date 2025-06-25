#pragma once

void RenderDoorControls();

// Door state interface for main loop
bool IsDoorSerialOpen();
void SendDoorCommand(bool open);
int GetObjectLimit();
void SetObjectLimit(int value);
int& RefPrevCount();
