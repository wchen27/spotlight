#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "shaman/shaman.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <math.h>

// Settings for our circle
static ImVec2 circle_center = ImVec2(0.5f, 0.5f); // Relative to window size
static float circle_radius = 0.05f; // Relative to window size
static ImVec4 circle_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // RGBA
static bool use_second_monitor = true;

static ImVec2 central_circle_center = ImVec2(0.5f, 0.5f); // In normalized coordinates
static float central_circle_radius = 0.05f;
static ImVec4 central_circle_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

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

// Function to create a borderless window on a specific monitor
GLFWwindow* create_borderless_window(GLFWmonitor* monitor) {
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Spotlight", nullptr, nullptr);
    if (!window) {
        return nullptr;
    }
    
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
    GLFWwindow* control_window = glfwCreateWindow(400, 600, "Spotlight Controls", nullptr, nullptr);
    if (!control_window) {
        std::cerr << "Failed to create control window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(control_window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Setup Platform/Renderer backends (only once for the control window)
    ImGui_ImplGlfw_InitForOpenGL(control_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Create spotlight window on second monitor if available
    GLFWwindow* spotlight_window = nullptr;
    if (has_second_monitor && use_second_monitor) {
        spotlight_window = create_borderless_window(monitors[1]);
        if (!spotlight_window) {
            std::cerr << "Failed to create spotlight window\n";
        }
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
            ImGui::Begin("Spotlight Controls");
            
            if (!has_second_monitor) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "No second monitor detected!");
            } else {
                ImGui::Checkbox("Use Second Monitor", &use_second_monitor);
                
                // ImGui::SliderFloat2("Circle Center", (float*)&circle_center, 0.0f, 1.0f);
                ImGui::SliderFloat("Circle Radius", &circle_radius, 0.01f, 0.5f);
                ImGui::ColorEdit4("Circle Color", (float*)&circle_color);
                
                if (ImGui::Button("Exit")) {
                    glfwSetWindowShouldClose(control_window, true);
                }
            }
            ImGui::SliderFloat("Central Circle Radius", &central_circle_radius, 0.01f, 0.5f);
            ImGui::ColorEdit4("Central Circle Color", (float*)&central_circle_color);
            
            if (ImGui::Button("Reset Central Position")) {
                central_circle_center = ImVec2(0.5f, 0.5f); // Recenter
            }

            ImGui::End();
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

        // Render spotlight window if available
        if (has_second_monitor && use_second_monitor && spotlight_window && !glfwWindowShouldClose(spotlight_window)) {
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

            // Actual central circle position in pixels
            ImVec2 central_pixel_pos = ImVec2(
                central_circle_center.x * width,
                central_circle_center.y * height
            );
            float central_pixel_radius = central_circle_radius * std::min(width, height);

            // Accumulated push vector
            ImVec2 total_push = ImVec2(0, 0);

            if (reader.pop(boxes)) {
                std::cout << "received " << boxes.size() << " vectors\n";
                for (const auto& obj : boxes) {
                    float cx = (obj.rect.x - 690) / 1831 * height + (width - height) / 2;
                    float cy = (obj.rect.y - 129) / 1848 * height;
                    float radius = obj.rect.width * 0.01f * circle_radius * std::min(width, height);

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

                    draw_filled_circle(cx, cy, radius, circle_color);
                }
            }

            // Apply push to central circle's position (in pixel space)
            central_pixel_pos.x += total_push.x;
            central_pixel_pos.y += total_push.y;

            // Clamp to window bounds
            central_pixel_pos.x = std::max(central_pixel_radius, std::min((float)width - central_pixel_radius, central_pixel_pos.x));
            central_pixel_pos.y = std::max(central_pixel_radius, std::min((float)height - central_pixel_radius, central_pixel_pos.y));

            // Convert back to normalized coordinates
            central_circle_center.x = central_pixel_pos.x / width;
            central_circle_center.y = central_pixel_pos.y / height;

            // Draw central circle
            draw_filled_circle(central_pixel_pos.x, central_pixel_pos.y, central_pixel_radius, central_circle_color);


            glfwSwapBuffers(spotlight_window);
        }
    }

    // Cleanup
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