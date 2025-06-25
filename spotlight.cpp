#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "shaman/shaman.h"
#include "serial/serial.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <math.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

#include "pump_controls.h"
#include "door_controls.h"
#include "spotlight_controls.h"
#include "grating_controls.h"
#include "concentric_circles_controls.h"

// Function to get monitor information
std::vector<GLFWmonitor*> get_monitors() {
    std::vector<GLFWmonitor*> monitors;
    int count;
    GLFWmonitor** m = glfwGetMonitors(&count);
    for (int i = 0; i < count; i++) {
        monitors.push_back(m[i]);
    }
    return monitors;
}

ImVec2 lerp(const ImVec2& a, const ImVec2& b, float t) {
    return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

// Function to create a borderless window on a specific monitor
GLFWwindow* create_borderless_window(GLFWmonitor* monitor, GLFWwindow* shared_context = nullptr) {
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

    // Share OpenGL context with control window
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Spotlight", nullptr, shared_context);

    if (!window) return nullptr;

    int xpos, ypos;
    glfwGetMonitorPos(monitor, &xpos, &ypos);
    glfwSetWindowPos(window, xpos, ypos);

    return window;
}

void draw_filled_circle(float cx, float cy, float r, ImVec4 color, int segments = 64) {
    glColor4f(color.x, color.y, color.z, color.w);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy); // center
    for (int i = 0; i <= segments; ++i) {
        float theta = (2.0f * 3.1415926f * float(i)) / float(segments);
        float x = r * cosf(theta);
        float y = r * sinf(theta);
        glVertex2f(cx + x, cy + y);
    }
    glEnd();
}

void draw_filled_circle(float cx, float cy, float r, ImVec4 color1, ImVec4 color2, int segments = 64) {
    // draw filled circle with alternating colors
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; ++i) {
        float theta = (2.0f * 3.1415926f * float(i)) / float(segments);
        float x = r * cosf(theta);
        float y = r * sinf(theta);
        
        if (i % 64 < 32) {
            glColor4f(color1.x, color1.y, color1.z, color1.w);
        } else {
            glColor4f(color2.x, color2.y, color2.z, color2.w);
        }
        glVertex2f(cx + x, cy + y);
    }
    glEnd();
}

void draw_filled_ring(float cx, float cy, float r_inner, float r_outer,
                      ImVec4 color1, ImVec4 color2, int segments, float theta_rotation) {
    float angle_step = 2.0f * 3.1415926f / segments;

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float theta = i * angle_step + theta_rotation; // Apply rotation
        float cos_theta = cosf(theta);
        float sin_theta = sinf(theta);

        // Alternate colors
        ImVec4 color = (i % 2 == 0) ? color1 : color2;
        glColor4f(color.x, color.y, color.z, color.w);

        // Outer vertex
        glVertex2f(cx + cos_theta * r_outer, cy + sin_theta * r_outer);
        // Inner vertex
        glVertex2f(cx + cos_theta * r_inner, cy + sin_theta * r_inner);
    }
    glEnd();
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    shaman::SharedBoxQueue reader(false);
    std::vector<shaman::Object> boxes;

    // Decide GL+GLSL versions
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Get monitor information
    auto monitors = get_monitors();
    bool has_second_monitor = monitors.size() > 1;
    
    // Create control window on primary monitor
    GLFWwindow* control_window = glfwCreateWindow(1000, 1000, "Spotlight Controls", nullptr, nullptr);
    if (!control_window) {
        std::cerr << "Failed to create control window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(control_window);
    glfwSwapInterval(1); // enable vsync

    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Setup Platform/Renderer backends (only once for the control window)
    ImGui_ImplGlfw_InitForOpenGL(control_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Create spotlight window on second monitor if available
    GLFWwindow* spotlight_window = nullptr;
    if (has_second_monitor && GetUseSecondMonitor()) {
        spotlight_window = create_borderless_window(monitors[1], control_window);
        if (!spotlight_window) {
            std::cerr << "Failed to create spotlight window\n";
        }
    }

    std::vector<shaman::Object> latest_boxes;
    std::mutex box_mutex;

    std::atomic<bool> running{true};
    
    uint64_t writer_timestamp = 0;

    std::thread reader_thread;
    {
        auto thread_func = [&]() {
            std::vector<shaman::Object> temp;
            while (running) {
                // writer_timestamp = 0;
                while (reader.pop(temp, writer_timestamp)) {


                    std::lock_guard<std::mutex> lock(box_mutex);
                    latest_boxes = temp;

                    uint64_t now = get_time_us();
                    // std::cout << "write - read latency: " << (now - writer_timestamp) / 1000.0 << std::endl;
                }
            }
        };
        reader_thread = std::thread(thread_func);
    }
    // Main loop
    while (!glfwWindowShouldClose(control_window)) {
        // Poll and handle events (inputs, window resize, etc.)
        glfwPollEvents();

        // Start the Dear ImGui frame for control window
        glfwMakeContextCurrent(control_window);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Control window UI
        {
            RenderPumpControls();
            RenderDoorControls();
            RenderSpotlightControls(has_second_monitor);
            RenderGratingControls();
            RenderConcentricRingsControls();
        }

        // Render control window
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(control_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(control_window);

        // get start time for rendering spotlight
        uint64_t spotlight_timestamp = get_time_us();

        // Render spotlight window if available
        if (has_second_monitor && GetUseSecondMonitor() && spotlight_window && !glfwWindowShouldClose(spotlight_window)) {
            glfwMakeContextCurrent(spotlight_window);

            int width, height;
            glfwGetFramebufferSize(spotlight_window, &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, width, height, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            // Draw gratings FIRST so they appear beneath everything else
            DrawMovingGratings(width, height, ImGui::GetTime());
            DrawConcentricRings(width, height, ImGui::GetTime());

            // Actual central circle position in pixels
            ImVec2 central_pixel_pos = ImVec2(
                RefCentralCircleCenter().x * width,
                RefCentralCircleCenter().y * height
            );
            float central_pixel_radius = GetCentralCircleRadius() * std::min(width, height);

            // Accumulated push vector
            ImVec2 total_push = ImVec2(0, 0);

            {
                std::lock_guard<std::mutex> lock(box_mutex);
                boxes = latest_boxes;
             }
            
            if (boxes.size() != RefPrevCount() && boxes.size() >= GetObjectLimit() && IsDoorSerialOpen()) {
                std::cout << "Sending door command to close door" << std::endl;
                SendDoorCommand(false);
            } else if (boxes.size() != RefPrevCount() && boxes.size() < GetObjectLimit() && IsDoorSerialOpen()) {
                std::cout << "Sending door command to open door" << std::endl;
                SendDoorCommand(true);
            }
            RefPrevCount() = boxes.size();
            
            float xcenter, ycenter;
            for (const auto& obj : boxes) {
                xcenter = obj.rect.x + obj.rect.width / 2.0f;
                ycenter = obj.rect.y - obj.rect.height / 2.0f;
                float cx = (xcenter - 1020) / 1172 * height + (width - height) / 2;
                cx = width - cx; // reflect so projection shows up correctly
                float cy = (ycenter - 465) / 1172 * height;
                float radius = GetCircleRadius() * height;
                if (GetCollisionEnabled()) {
                    // Calculate vector between centers
                    float dx = central_pixel_pos.x - cx;
                    float dy = central_pixel_pos.y - cy;
                    float dist_sq = dx * dx + dy * dy;
                    float min_dist = central_pixel_radius + radius;

                    if (dist_sq < min_dist * min_dist && dist_sq > 0.0f) {
                        float dist = std::sqrt(dist_sq);
                        float push_strength = (min_dist - dist) * 0.5f; // Push half the overlap
                        total_push.x += (dx / dist) * push_strength;
                        total_push.y += (dy / dist) * push_strength;
                    }

                }
                
                draw_filled_ring(cx, cy, radius, radius * GetInnerRadius(), GetCircleColor(), GetAlternateCircleColor(), GetCircleSegments(), RefThetaRotation());
            }
            

            // Apply push to central circle's position (in pixel space)
            central_pixel_pos.x += total_push.x;
            central_pixel_pos.y += total_push.y;

            // Clamp to window bounds
            central_pixel_pos.x = std::max(central_pixel_radius, std::min((float)width - central_pixel_radius, central_pixel_pos.x));
            central_pixel_pos.y = std::max(central_pixel_radius, std::min((float)height - central_pixel_radius, central_pixel_pos.y));

            // drift back to center
            const ImVec2 center_normalized(0.5f, 0.5f);
            const float push_magnitude = std::sqrt(total_push.x * total_push.x + total_push.y * total_push.y);
            if (push_magnitude < 0.05f) {
                // Drift in normalized space
                RefCentralCircleCenter() = lerp(RefCentralCircleCenter(), center_normalized, GetDriftSpeed());
                central_pixel_pos.x = RefCentralCircleCenter().x * width;
                central_pixel_pos.y = RefCentralCircleCenter().y * height;
            }

            // Convert back to normalized coordinates
            RefCentralCircleCenter().x = central_pixel_pos.x / width;
            RefCentralCircleCenter().y = central_pixel_pos.y / height;

            if (!GetDynamicCircle()) {
                draw_filled_circle(central_pixel_pos.x, central_pixel_pos.y, central_pixel_radius, GetCentralCircleColor(), GetAlternateCentralCircleColor(), GetCentralCircleSegments());
            } else {
                double elapsed = ImGui::GetTime() - RefDynamicCircleStartTime();
                if (elapsed <= GetDynamicCircleLingerDuration() + GetDynamicCircleMaxDuration()) {
                    RefDynamicCircleRadius() = std::min(
                        GetDynamicCircleMaxRadius(),
                        ((float) elapsed / GetDynamicCircleMaxDuration()) * GetDynamicCircleMaxRadius()
                    );
                    draw_filled_circle(central_pixel_pos.x, central_pixel_pos.y,
                                    RefDynamicCircleRadius() * std::min(width, height),
                                    GetCentralCircleColor(), GetCentralCircleSegments());
                } else { 
                    RefDynamicCircleRadius() = 0.0f;
                }
            }

            // rotate circles if enabled
            if (GetRotationRunning()) {
                double now = ImGui::GetTime();
                if (!RefInRotationPhase() && !RefInDelayPhase()) {
                    RefStartTheta() = RefThetaRotation();
                    if (GetRandomRotation()) {
                        float magnitude = GetMinRotation() + static_cast<float>(rand()) / RAND_MAX * (GetMaxRotation() - GetMinRotation());
                        int dir = GetRandomizeRotationDirection() ? (rand() % 2 == 0 ? 1 : -1) : GetRotationDirection();
                        RefTargetTheta() = RefThetaRotation() + dir * magnitude;
                    }
                    if (GetRandomizeRotationTime()) {
                        RefActualRotationTime() = GetMinRotationTime() + static_cast<float>(rand()) / RAND_MAX * (GetMaxRotationTime() - GetMinRotationTime());
                    } else {
                        RefActualRotationTime() = GetRotationTime();
                    }
                    if (GetRandomizeRotationDelay()) {
                        RefActualRotationDelay() = GetMinRotationDelay() + static_cast<float>(rand()) / RAND_MAX * (GetMaxRotationDelay() - GetMinRotationDelay());
                    } else {
                        RefActualRotationDelay() = GetRotationDelay();
                    }
                    // rotation_start_time = now; (move to module if needed)
                    RefInRotationPhase() = true;
                }
                if (RefInRotationPhase()) {
                    float t = static_cast<float>((now - RefRotationStartTime()) / RefActualRotationTime());
                    if (t >= 1.0f) {
                        RefThetaRotation() = RefTargetTheta();
                        RefInRotationPhase() = false;
                        RefInDelayPhase() = true;
                        // rotation_start_time = now; (move to module if needed)
                    } else {
                        RefThetaRotation() = RefStartTheta() + t * (RefTargetTheta() - RefStartTheta());
                    }
                } else if (RefInDelayPhase()) {
                    if ((now - RefRotationStartTime()) >= RefActualRotationDelay()) {
                        RefInDelayPhase() = false;
                    }
                }
            }

            // Draw central circle
            

            if (RefCalibrating()) {
                draw_filled_circle(width / 2.0 - height / 2.0, 0, 20, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                draw_filled_circle(width / 2.0 + height / 2.0, 0, 20, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                draw_filled_circle(width / 2.0 - height / 2.0, height, 20, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                draw_filled_circle(width / 2.0 + height / 2.0, height, 20, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            }

            uint64_t now = get_time_us();
            // format latency in seconds
            uint64_t latency = now - spotlight_timestamp;

            std::string latency_str = "render latency: " + std::to_string(latency / 1000.0) + " ms";

            if (latency > 100000) {
                std::cout << "high latency: " << latency_str << std::endl;
            }

            glfwSwapBuffers(spotlight_window);
        }
    }

    // Cleanup
    running = false;
    reader_thread.join();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (spotlight_window) {
        glfwDestroyWindow(spotlight_window);
    }
    glfwDestroyWindow(control_window);
    glfwTerminate();

    return 0;
}