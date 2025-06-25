#include "grating_controls.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <GL/gl.h>

namespace {
static bool show_grating = false;
static int is_vertical = 1; 
static float speed = 100.0f;
static float bar_length = 40.0f; 
static ImVec2 center = ImVec2(0.5f, 0.5f); 
static float box_width = 0.3f; 
static float box_height = 0.3f; 
static ImVec4 bar_color = ImVec4(1,1,1,1);
static ImVec4 bg_color = ImVec4(0,0,0,1);
}

void RenderGratingControls() {
    ImGui::Begin("Grating Controls");
    ImGui::Checkbox("Show Grating", &show_grating);
    ImGui::RadioButton("Vertical Bars", &is_vertical, 1); ImGui::SameLine();
    ImGui::RadioButton("Horizontal Bars", &is_vertical, 0);
    ImGui::SliderFloat("Speed (px/s)", &speed, -500.0f, 500.0f, "%.1f");
    ImGui::SliderFloat("Bar Length (px)", &bar_length, 5.0f, 200.0f, "%.1f");
    ImGui::SliderFloat2("Center (norm)", (float*)&center, 0.0f, 1.0f);
    ImGui::SliderFloat("Box Width (norm)", &box_width, 0.05f, 1.0f);
    ImGui::SliderFloat("Box Height (norm)", &box_height, 0.05f, 1.0f);
    ImGui::ColorEdit4("Bar Color", (float*)&bar_color);
    ImGui::ColorEdit4("BG Color", (float*)&bg_color);
    ImGui::End();
}

void DrawMovingGratings(int width, int height, double time) {
    if (!show_grating) return;
    float cx = center.x * width;
    float cy = center.y * height;
    float w = box_width * width;
    float h = box_height * height;
    float left = cx - w/2, right = cx + w/2;
    float top = cy - h/2, bottom = cy + h/2;

    glColor4f(bg_color.x, bg_color.y, bg_color.z, bg_color.w);
    glBegin(GL_QUADS);
    glVertex3f(left, top, -1.0f);
    glVertex3f(right, top, -1.0f);
    glVertex3f(right, bottom, -1.0f);
    glVertex3f(left, bottom, -1.0f);
    glEnd();

    glColor4f(bar_color.x, bar_color.y, bar_color.z, bar_color.w);
    float period = 2 * bar_length;
    float offset = std::fmod(time * speed, period);
    if (is_vertical == 1) {
        for (float x = left - period; x < right + period; x += period) {
            float bar_x = x + offset;
            float bar_x2 = bar_x + bar_length;
            if (bar_x2 < left || bar_x > right) continue;
            float bx1 = std::max(bar_x, left);
            float bx2 = std::min(bar_x2, right);
            glBegin(GL_QUADS);
            glVertex3f(bx1, top, -1.0f);
            glVertex3f(bx2, top, -1.0f);
            glVertex3f(bx2, bottom, -1.0f);
            glVertex3f(bx1, bottom, -1.0f);
            glEnd();
        }
    } else {
        for (float y = top - period; y < bottom + period; y += period) {
            float bar_y = y + offset;
            float bar_y2 = bar_y + bar_length;
            if (bar_y2 < top || bar_y > bottom) continue;
            float by1 = std::max(bar_y, top);
            float by2 = std::min(bar_y2, bottom);
            glBegin(GL_QUADS);
            glVertex3f(left, by1, -1.0f);
            glVertex3f(right, by1, -1.0f);
            glVertex3f(right, by2, -1.0f);
            glVertex3f(left, by2, -1.0f);
            glEnd();
        }
    }
}
