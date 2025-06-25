#include "spotlight_controls.h"
#include "imgui.h"
#include <algorithm>

namespace {
static bool use_second_monitor = true;
static ImVec2 circle_center = ImVec2(0.5f, 0.5f);
static float circle_radius = 0.12f;
static ImVec4 circle_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
static ImVec4 alternate_circle_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
static int circle_segments = 12;
static float inner_radius = 0.75;
static bool randomize_rotation_direction = true;
static int rotation_direction = 1;
static float theta_rotation = 0.0f;
static bool random_rotation = true;
static float min_rotation = 6.0f;
static float max_rotation = 12.0f;
static float min_rotation_time = 0.5f;
static float max_rotation_time = 5.0f;
static float rotation_time = 1.0f;
static bool randomize_rotation_time = true;
static bool randomize_rotation_delay = true;
static float rotation_delay = 0.0f;
static float min_rotation_delay = 0.0f;
static float max_rotation_delay = 5.0f;
static bool rotation_running = false;
static double rotation_start_time = -1.0;
static float target_theta = 0.0f;
static float start_theta = 0.0f;
static float actual_rotation_time = 1.0f;
static float actual_rotation_delay = 0.0f;
static bool in_rotation_phase = false;
static bool in_delay_phase = false;
static ImVec2 central_circle_center = ImVec2(0.5f, 0.5f);
static float central_circle_radius = 0.1f;
static ImVec4 central_circle_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
static ImVec4 alternate_central_circle_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
static int central_circle_segments = 64;
static float drift_speed = 0.1f;
static float dynamic_circle_radius = 0.0f;
static float dynamic_circle_max_duration = 3.0f;
static float dynamic_circle_max_radius = 0.2f;
static float dynamic_circle_linger_duration = 1.0f;
static double dynamic_circle_start_time = -1.0;
static bool collision_enabled = true;
static bool calibrating = false;
static bool dynamic_circle = false;
}

void RenderSpotlightControls(bool has_second_monitor) {
    ImGui::Begin("Spotlight Controls");
    if (!has_second_monitor) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "No second monitor detected!");
    } else {
        ImGui::Checkbox("Use Second Monitor", &use_second_monitor);
        ImGui::SliderFloat("Circle Radius", &circle_radius, 0.01f, 0.5f);
        ImGui::SliderFloat("Inner Circle Radius", &inner_radius, 0.01f, 1.0f);
        ImGui::ColorEdit4("Circle Color", (float*)&circle_color);
        ImGui::Checkbox("Random Rotation", &random_rotation);
        ImGui::Checkbox("Randomize Rotation Time", &randomize_rotation_time);
        ImGui::Checkbox("Randomize Rotation Delay", &randomize_rotation_delay);
        if (random_rotation) {
            ImGui::SliderFloat("Min Rotation (magnitude)", &min_rotation, 0.0f, 60.0f);
            ImGui::SliderFloat("Max Rotation (magnitude)", &max_rotation, 0.0f, 60.0f);
            ImGui::SliderFloat("Min Rotation Time", &min_rotation_time, 0.0f, 10.0f);
            ImGui::SliderFloat("Max Rotation Time", &max_rotation_time, 0.0f, 10.0f);
            ImGui::Checkbox("Randomize Direction", &randomize_rotation_direction);
            if (!randomize_rotation_direction) {
                ImGui::RadioButton("CW", &rotation_direction, 1); ImGui::SameLine();
                ImGui::RadioButton("CCW", &rotation_direction, -1);
            }
            if (randomize_rotation_delay) {
                ImGui::SliderFloat("Min Rotation Delay", &min_rotation_delay, 0.0f, 10.0f);
                ImGui::SliderFloat("Max Rotation Delay", &max_rotation_delay, 0.0f, 10.0f);
            } else {
                ImGui::SliderFloat("Rotation Delay", &rotation_delay, 0.0f, 10.0f);
            }
        } else {
            ImGui::SliderFloat("Theta Rotation", &theta_rotation, 0.0f, 6.28319f);
        }
        if (rotation_running) {
            if (ImGui::Button("Stop Rotation")) {
                rotation_running = false;
                in_rotation_phase = false;
                in_delay_phase = false;
            }
        } else {
            if (ImGui::Button("Start Rotation")) {
                rotation_running = true;
                rotation_start_time = ImGui::GetTime();
            }
        }
        ImGui::ColorEdit4("Alternate Circle Color", (float*)&alternate_circle_color);
        ImGui::SliderInt("Circle Segments", &circle_segments, 3, 128);
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SliderFloat("Central Circle Radius", &central_circle_radius, 0.01f, 1.0f);
    ImGui::ColorEdit4("Central Circle Color", (float*)&central_circle_color);
    ImGui::SliderInt("Central Circle Segments", &central_circle_segments, 3, 128);
    if (ImGui::Button("Reset Central Position")) {
        central_circle_center = ImVec2(0.5f, 0.5f);
    }
    ImGui::SliderFloat("Drift Speed", &drift_speed, 0.005f, 0.2f);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SliderFloat("Dynamic Circle Duration", &dynamic_circle_max_duration, 0.5f, 10.0f, "%.1f s");
    ImGui::SliderFloat("Dynamic Circle Max Radius", &dynamic_circle_max_radius, 0.01f, 0.5f, "%.2f");
    ImGui::SliderFloat("Dynamic Circle Linger Duration", &dynamic_circle_linger_duration, 1.0f, 60.0f, "%.1f s");
    if (ImGui::Checkbox("Dynamic Circle", &dynamic_circle)) {
        if (dynamic_circle) {
            dynamic_circle_start_time = -999;
            dynamic_circle_radius = 0.00f;
        }
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Checkbox("Collision Enabled", &collision_enabled);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Checkbox("Calibration Mode", &calibrating);
    ImGui::End();
}

// Interface implementations
float GetCircleRadius() { return circle_radius; }
float GetInnerRadius() { return inner_radius; }
ImVec4 GetCircleColor() { return circle_color; }
ImVec4 GetAlternateCircleColor() { return alternate_circle_color; }
int GetCircleSegments() { return circle_segments; }
ImVec2& RefCentralCircleCenter() { return central_circle_center; }
float GetCentralCircleRadius() { return central_circle_radius; }
ImVec4 GetCentralCircleColor() { return central_circle_color; }
ImVec4 GetAlternateCentralCircleColor() { return alternate_central_circle_color; }
int GetCentralCircleSegments() { return central_circle_segments; }
float GetDriftSpeed() { return drift_speed; }
bool GetCollisionEnabled() { return collision_enabled; }
bool GetDynamicCircle() { return dynamic_circle; }
double& RefDynamicCircleStartTime() { return dynamic_circle_start_time; }
float& RefDynamicCircleRadius() { return dynamic_circle_radius; }
bool& RefCalibrating() { return calibrating; }
bool GetUseSecondMonitor() { return use_second_monitor; }
float GetDynamicCircleMaxDuration() { return dynamic_circle_max_duration; }
float GetDynamicCircleMaxRadius() { return dynamic_circle_max_radius; }
float GetDynamicCircleLingerDuration() { return dynamic_circle_linger_duration; }
bool GetRotationRunning() { return rotation_running; }
bool& RefInRotationPhase() { return in_rotation_phase; }
bool& RefInDelayPhase() { return in_delay_phase; }
float& RefStartTheta() { return start_theta; }
float& RefThetaRotation() { return theta_rotation; }
bool GetRandomRotation() { return random_rotation; }
float GetMinRotation() { return min_rotation; }
float GetMaxRotation() { return max_rotation; }
bool GetRandomizeRotationDirection() { return randomize_rotation_direction; }
int GetRotationDirection() { return rotation_direction; }
float& RefTargetTheta() { return target_theta; }
bool GetRandomizeRotationTime() { return randomize_rotation_time; }
float GetMinRotationTime() { return min_rotation_time; }
float GetMaxRotationTime() { return max_rotation_time; }
float& RefActualRotationTime() { return actual_rotation_time; }
float GetRotationTime() { return rotation_time; }
bool GetRandomizeRotationDelay() { return randomize_rotation_delay; }
float GetMinRotationDelay() { return min_rotation_delay; }
float GetMaxRotationDelay() { return max_rotation_delay; }
float& RefActualRotationDelay() { return actual_rotation_delay; }
float GetRotationDelay() { return rotation_delay; }
double& RefRotationStartTime() { return rotation_start_time; }
