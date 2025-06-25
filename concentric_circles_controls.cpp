#include "concentric_circles_controls.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <algorithm>
#include <deque>

static bool rings_enabled = false;
static ImVec2 center = ImVec2(0.5f, 0.5f); 
static float box_width = 0.8f;
static int num_rings = 5;
static float ring_radius = 0.04f; 
static float ring_gap = 0.02f;
static ImVec4 ring_color = ImVec4(1,1,1,1);
static float thickness = 0.01f;
static float shrink_speed = 80.0f;


typedef struct {
    float radius; 
} RingState;

static std::deque<RingState> rings;
static double last_time = 0.0;

static void ResetRings(float box_size, int n, float start_radius, float gap, float thick) {
    rings.clear();
    float r = start_radius + (n-1) * (gap + thick);
    for (int i = n-1; i >= 0; --i) {
        rings.push_front({r});
        r -= (gap + thick);
    }
}

void RenderConcentricRingsControls() {
    if (ImGui::Begin("Concentric Rings Controls")) {
        ImGui::Checkbox("Enable Concentric Rings", &rings_enabled);
        ImGui::SliderFloat2("Center (X,Y)", (float*)&center, 0.0f, 1.0f);
        ImGui::SliderFloat("Box Width", &box_width, 0.1f, 1.0f);
        ImGui::SliderInt("Number of Rings", &num_rings, 1, 20);
        ImGui::SliderFloat("Ring Radius", &ring_radius, 0.01f, 0.5f);
        ImGui::SliderFloat("Gap Between Rings", &ring_gap, 0.0f, 0.2f);
        ImGui::SliderFloat("Ring Thickness", &thickness, 0.001f, 0.1f);
        ImGui::SliderFloat("Convergence Speed (px/s)", &shrink_speed, 1.0f, 500.0f);
        ImGui::ColorEdit4("Ring Color", (float*)&ring_color);
        if (ImGui::Button("Reset Rings")) {
            float box_size = box_width * std::min(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
            float start_r = ring_radius * box_size;
            float gap = ring_gap * box_size;
            float thick = thickness * box_size;
            ResetRings(box_size, num_rings, start_r, gap, thick);
        }
    }
    ImGui::End();
}

bool GetConcentricRingsEnabled() {
    return rings_enabled;
}

void DrawConcentricRings(int width, int height, double time) {
    if (!rings_enabled) return;
    float box_size = box_width * std::min(width, height);
    float cx = center.x * width;
    float cy = center.y * height;
    float start_radius = ring_radius * box_size;
    float gap = ring_gap * box_size;
    float thick = thickness * box_size;
    if (rings.empty() || num_rings != (int)rings.size()) {
        ResetRings(box_size, num_rings, start_radius, gap, thick);
        last_time = time;
    }
    float dt = (float)(time - last_time);
    last_time = time;
    // Shrink all rings
    for (auto& ring : rings) {
        ring.radius -= shrink_speed * dt;
    }
    while (!rings.empty() && rings.front().radius < thick * 0.5f) {
        rings.pop_front();
    }
    
    while ((int)rings.size() < num_rings) {
        float outermost = rings.empty() ? start_radius : rings.back().radius;
        float new_radius = outermost + gap + thick;
        rings.push_back({new_radius});
    }

    glColor4f(ring_color.x, ring_color.y, ring_color.z, ring_color.w);
    for (const auto& ring : rings) {
        float inner = ring.radius - thick * 0.5f;
        float outer = ring.radius + thick * 0.5f;
        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= 128; ++j) {
            float theta = 2.0f * 3.1415926f * j / 128.0f;
            float x = cosf(theta);
            float y = sinf(theta);
            glVertex2f(cx + x * outer, cy + y * outer);
            glVertex2f(cx + x * inner, cy + y * inner);
        }
        glEnd();
    }
}
