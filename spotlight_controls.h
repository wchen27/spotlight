#pragma once
#include "imgui.h"

void RenderSpotlightControls(bool has_second_monitor);

// Spotlight state interface for main loop
float GetCircleRadius();
float GetInnerRadius();
ImVec4 GetCircleColor();
ImVec4 GetAlternateCircleColor();
int GetCircleSegments();
ImVec2& RefCentralCircleCenter();
float GetCentralCircleRadius();
ImVec4 GetCentralCircleColor();
ImVec4 GetAlternateCentralCircleColor();
int GetCentralCircleSegments();
float GetDriftSpeed();
bool GetCollisionEnabled();
bool GetDynamicCircle();
double& RefDynamicCircleStartTime();
float& RefDynamicCircleRadius();
bool& RefCalibrating();
bool GetUseSecondMonitor();
float GetDynamicCircleMaxDuration();
float GetDynamicCircleMaxRadius();
float GetDynamicCircleLingerDuration();
bool GetRotationRunning();
bool& RefInRotationPhase();
bool& RefInDelayPhase();
float& RefStartTheta();
float& RefThetaRotation();
bool GetRandomRotation();
float GetMinRotation();
float GetMaxRotation();
bool GetRandomizeRotationDirection();
int GetRotationDirection();
float& RefTargetTheta();
bool GetRandomizeRotationTime();
float GetMinRotationTime();
float GetMaxRotationTime();
float& RefActualRotationTime();
float GetRotationTime();
bool GetRandomizeRotationDelay();
float GetMinRotationDelay();
float GetMaxRotationDelay();
float& RefActualRotationDelay();
float GetRotationDelay();
double& RefRotationStartTime();
// ...add more as needed for main loop rendering...
