#pragma once
#include <imgui.h>
#include <vector> 
class SerialPort;

// User-configurable salesman experiment appearance
ImVec4& RefSalesmanCircleColor();
int& RefSalesmanCircleSegments();

void DrawSalesmanExperiment(int width, int height, double time);
void RenderSalesmanExperimentControls();
void RestartSalesmanExperiment();
bool IsSalesmanExperimentRunning();
void UpdateSalesmanExperiment(int width, int height, double time, const std::vector<std::pair<ImVec2, float>>& ring_list, SerialPort& serial, const char* pump_ids);
