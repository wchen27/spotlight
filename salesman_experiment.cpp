#include "salesman_experiment.h"
#include "serial/serial.h"
#include "pump_controls.h"
#include "spotlight_controls.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>

struct SalesmanCircle {
    ImVec2 center; // normalized [0,1]
    float radius;  // in pixels
    bool collected = false;
    double intersect_start = 0.0; // time when intersection started
    bool intersecting = false;
};

static std::vector<SalesmanCircle> circles;
static int num_circles = 5;
static float circle_radius = 70.0f;
static int intersection_time_ms = 500;
static bool pump_check[3] = {false, false, false};
static bool experiment_running = false;
static double experiment_start_time = 0.0;
static unsigned int user_seed = 0; // User-configurable seed (0 = auto-generate)
static unsigned int current_seed = 0; // The actual seed used for current experiment
static bool reuse_last_seed = false; // Checkbox to reuse the last seed
static std::mt19937 rng;

// User-configurable color and segments for salesman circles
static ImVec4 salesman_circle_color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
static int salesman_circle_segments = 5;
ImVec4& RefSalesmanCircleColor() { return salesman_circle_color; }
int& RefSalesmanCircleSegments() { return salesman_circle_segments; }

void RestartSalesmanExperiment() {
    circles.clear();
    
    // Set the seed for this experiment
    if (reuse_last_seed && current_seed != 0) {
        // Reuse the last seed
        rng.seed(current_seed);
    } else if (user_seed == 0) {
        // Auto-generate seed from current time
        current_seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
        rng.seed(current_seed);
    } else {
        // Use user-specified seed
        current_seed = user_seed;
        rng.seed(current_seed);
    }
    
    std::uniform_real_distribution<float> xdist(0.1f, 0.9f);
    std::uniform_real_distribution<float> ydist(0.1f, 0.9f);
    
    for (int i = 0; i < num_circles; ++i) {
        SalesmanCircle c;
        c.radius = circle_radius;
        c.collected = false;
        c.intersect_start = 0.0;
        c.intersecting = false;
        
        // Try to place circle without overlapping existing ones
        bool placed = false;
        int attempts = 0;
        const int max_attempts = 100; // Prevent infinite loop
        
        while (!placed && attempts < max_attempts) {
            c.center = ImVec2(xdist(rng), ydist(rng));
            
            // Check if this position overlaps with any existing circle
            bool overlaps = false;
            for (const auto& existing : circles) {
                float dx = c.center.x - existing.center.x;
                float dy = c.center.y - existing.center.y;
                float distance = sqrtf(dx * dx + dy * dy);
                
                // Add some padding between circles (1.2x radius instead of just touching)
                float min_distance = (c.radius + existing.radius) * 1.2f / 1000.0f; // Convert to normalized space
                
                if (distance < min_distance) {
                    overlaps = true;
                    break;
                }
            }
            
            if (!overlaps) {
                placed = true;
            }
            attempts++;
        }
        
        // If we couldn't place without overlap after many attempts, place anyway
        // This handles cases where there's not enough space for all circles
        circles.push_back(c);
    }
    experiment_running = true;
    experiment_start_time = ImGui::GetTime();
}

bool IsSalesmanExperimentRunning() {
    return experiment_running;
}

void RenderSalesmanExperimentControls() {
    if (ImGui::Begin("Salesman Experiment")) {
        ImGui::SliderInt("Number of Circles", &num_circles, 1, 20);
        ImGui::SliderFloat("Circle Radius (px)", &circle_radius, 5.0f, 200.0f);
        ImGui::SliderInt("Intersection Time (ms)", &intersection_time_ms, 100, 5000);
        ImGui::ColorEdit4("Circle Color", (float*)&salesman_circle_color);
        ImGui::SliderInt("Circle Segments", &salesman_circle_segments, 8, 128);
        
        // Seed controls
        ImGui::Separator();
        ImGui::Text("Random Seed Controls:");
        ImGui::InputInt("Seed (0 = auto)", (int*)&user_seed);
        if (user_seed < 0) user_seed = 0; // Ensure non-negative
        ImGui::Checkbox("Reuse last seed", &reuse_last_seed);
        ImGui::Text("Current experiment seed: %u", current_seed);
        
        ImGui::Separator();
        ImGui::Checkbox("Pump X", &pump_check[0]); ImGui::SameLine();
        ImGui::Checkbox("Pump Y", &pump_check[1]); ImGui::SameLine();
        ImGui::Checkbox("Pump Z", &pump_check[2]);
        if (ImGui::Button("Restart Experiment")) {
            RestartSalesmanExperiment();
        }
        if (!experiment_running) {
            if (ImGui::Button("Start Experiment")) {
                RestartSalesmanExperiment();
            }
        }
        ImGui::Text("Circles remaining: %d", (int)std::count_if(circles.begin(), circles.end(), [](const SalesmanCircle& c){return !c.collected;}));
    }
    ImGui::End();
}

void DrawSalesmanExperiment(int width, int height, double /*time*/) {
    if (!experiment_running) return;
    for (const auto& c : circles) {
        if (c.collected) continue;
        float px = c.center.x * width;
        float py = c.center.y * height;
        glColor4f(salesman_circle_color.x, salesman_circle_color.y, salesman_circle_color.z, salesman_circle_color.w);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(px, py);
        for (int j = 0; j <= salesman_circle_segments; ++j) {
            float theta = 2.0f * 3.1415926f * j / salesman_circle_segments;
            glVertex2f(px + cosf(theta) * c.radius, py + sinf(theta) * c.radius);
        }
        glEnd();
    }
}

void UpdateSalesmanExperiment(int width, int height, double time, const std::vector<std::pair<ImVec2, float>>& ring_list, SerialPort& serial, const char* pump_ids) {
    if (!experiment_running) return;
    for (auto& c : circles) {
        if (c.collected) continue;
        float px = c.center.x * width;
        float py = c.center.y * height;
        bool any_intersect = false;
        for (const auto& ring : ring_list) {
            float dx = px - ring.first.x * width;
            float dy = py - ring.first.y * height;
            float dist = sqrtf(dx*dx + dy*dy);
            if (fabs(dist - ring.second) < c.radius) {
                any_intersect = true;
                break;
            }
        }
        if (any_intersect) {
            if (!c.intersecting) {
                c.intersecting = true;
                c.intersect_start = time;
            } else if ((time - c.intersect_start) * 1000.0 >= intersection_time_ms) {
                c.collected = true;
            }
        } else {
            c.intersecting = false;
        }
    }
    // If all collected, trigger pumps
    if (std::all_of(circles.begin(), circles.end(), [](const SalesmanCircle& c){return c.collected;})) {
        experiment_running = false;
        for (int i = 0; i < 3; ++i) {
            if (pump_check[i]) {
                if (get_pump_control_mode(i) == 0) {
                    serial.send_pump_command(pump_ids[i], get_pump_is_push(i), get_pump_microliters(i), get_pump_delivery_ms(i));
                } else {
                    serial.send_pump_command(pump_ids[i], get_pump_is_push(i), get_pump_cycles(i), get_pump_delays(i));
                }
                // Use the exact same logic as pump_controls.cpp for dynamic circle
                if (GetDynamicCircle()) {
                    RefDynamicCircleStartTime() = ImGui::GetTime();
                    RefDynamicCircleRadius() = 0.00f;
                }
            }
        }
    }
}
