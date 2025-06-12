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

// serial settings
static SerialPort serial;
static std::vector<std::string> port_list;
static int selected_port = -1;
static char send_buffer[128] = "";
static std::string recv_data;

// pump control settings
const char pump_ids[] = { 'x', 'y', 'z' };
static int cycles[3] = {1000, 1000, 1000};
static int delays[3] = {50, 50, 50};
static int push_directions[3] = {1, 1, 1};
static int control_mode[3] = {0, 0, 0};
static float microliters[3] = {2.0f, 2.0f, 2.0f};
static int delivery_ms[3] = {100, 100, 100};
static bool repeat[3] = {false, false, false};
static bool randomize[3] = {false, false, false};
static int repeat_delay[3] = {10, 10, 10};
static int random_min_delay[3] = {5, 5, 5};
static int random_max_delay[3] = {100, 100, 100};
static double last_sent_time[3] = {0.0, 0.0, 0.0};
static bool curr_running[3] = {false, false, false};

// object circle settings
static ImVec2 circle_center = ImVec2(0.5f, 0.5f); // Relative to window size
static float circle_radius = 0.2f; // Relative to window size
static ImVec4 circle_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // RGBA
static ImVec4 alternate_circle_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
static int circle_segments = 64; // Number of segments for the circle
static float inner_radius = 0.75;
static float theta_rotation = 0.0f; // Rotation angle in radians
static bool use_second_monitor = true;

static ImVec2 central_circle_center = ImVec2(0.5f, 0.5f); // In normalized coordinates
static float central_circle_radius = 0.1f;
static ImVec4 central_circle_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
static ImVec4 alternate_central_circle_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
static int central_circle_segments = 64; // Number of segments for the central circle
static float drift_speed = 0.1f;

static float dynamic_circle_radius = 0.0f; // Radius that increases over time
static float dynamic_circle_max_duration = 3.0f;  // in seconds
static float dynamic_circle_max_radius = 0.2f; // Maximum radius for the dynamic circle
static float dynamic_circle_linger_duration = 1.0f; // Duration to linger at max radius
static double dynamic_circle_start_time = -1.0;  

static bool collision_enabled = true;
static bool calibrating = false;
static bool dynamic_circle = false;

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
                      ImVec4 color1, ImVec4 color2, int segments = 64) {
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
    GLFWwindow* control_window = glfwCreateWindow(1000, 800, "Spotlight Controls", nullptr, nullptr);
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
    if (has_second_monitor && use_second_monitor) {
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
            if (ImGui::Begin("Syringe Control")) {
                // window for serial communication functions
                
                static std::vector<std::string> config_files = list_json_files_in_folder();
                static int selected_config_index = 0;
                static std::string current_config_file = config_files.empty() ? "" : config_files[0];
    
                std::size_t pos = current_config_file.find_last_of("/\\");
                std::string current_filename = (pos == std::string::npos) ? current_config_file 
                                                    : current_config_file.substr(pos + 1);
                
                if (ImGui::Button("Scan Ports")) {
                    port_list = SerialPort::list_available_ports();
                    selected_port = -1;
                }
    
                for (int i = 0; i < port_list.size(); i++) {
                    if (ImGui::RadioButton(port_list[i].c_str(), selected_port == i)) {
                        selected_port = i;
                    }
                }
    
                if (!serial.is_open() && selected_port >= 0) {
                    if (ImGui::Button("Open Port")) {
                        serial.open(port_list[selected_port]);
                        const auto& configs = get_loaded_pump_configs(config_files[0]);
                        initialize_pump_state_from_config(configs, pump_ids, microliters, delivery_ms, cycles, delays, push_directions, control_mode, repeat, repeat_delay);  
                    }
                } else if (serial.is_open()) {
                    ImGui::Text("Port Open");
                    if (ImGui::Button("Close Port")) {
                        serial.close();
                    }
                }
    
                for (int i = 0; i < 3; ++i) {
                    ImGui::PushID(i);  // Make widgets unique
            
                    std::string label = std::string("Pump ") + (char)toupper(pump_ids[i]);
                    ImGui::Text("%s", label.c_str());
            
                    ImGui::RadioButton("Push", &push_directions[i], 1); ImGui::SameLine();
                    ImGui::RadioButton("Pull", &push_directions[i], 0);
            
                    ImGui::RadioButton("µL", &control_mode[i], 0); ImGui::SameLine();
                    ImGui::RadioButton("Cycles/Delay", &control_mode[i], 1);
    
                    // µL or Cycles/Delay inputs
                    if (control_mode[i] == 0) {
                        ImGui::SliderFloat("Target µL", &microliters[i], 1.0f, 20.0f, "%.1f");
                        ImGui::SliderInt("Delivery Time", &delivery_ms[i], 10, 1000);
                    } else {
                        ImGui::SliderInt("Cycles", &cycles[i], 100, 30000);
                        ImGui::SliderInt("Delay", &delays[i], 10, 100);
                    }
            
                    if (serial.is_open()) {
                        if (curr_running[i]) {
                            if (ImGui::Button("Stop Command")) {
                                curr_running[i] = false;
                                last_sent_time[i] = 0.0;
                            } ImGui::SameLine();
                            
                        } else {
                            if (ImGui::Button("Send Command")) {
                                if (repeat[i]) {
                                    curr_running[i] = true;
                                } else {
                                    bool is_push = (push_directions[i] == 1);
                                    if (control_mode[i] == 0) {
                                        serial.send_pump_command(pump_ids[i], is_push, microliters[i], delivery_ms[i]);
                                    }
                                    else {
                                        serial.send_pump_command(pump_ids[i], is_push, cycles[i], delays[i]);
                                    }
                                }
                                std::time_t dispense_time = std::time(nullptr);
                                char ts[64];
                                std::strftime(ts, sizeof(ts), "[%Y-%m-%d %H:%M:%S] ", std::localtime(&dispense_time));
                                std::cout << ts << "Dispensing pump " << pump_ids[i] << std::endl;
                                dynamic_circle_start_time = ImGui::GetTime();
                                dynamic_circle_radius = 0.00f;
                            } ImGui::SameLine();
                        }
                        
    
                        ImGui::Checkbox("Repeat", &repeat[i]); 
                        ImGui::SameLine();
                        ImGui::Checkbox("Randomize", &randomize[i]);
                        if (repeat[i] && !randomize[i]) {
                            ImGui::SliderInt("Interval", &repeat_delay[i], 5, 5000); // TODO: change
                        }
                        if (repeat[i] && randomize[i]) {
                            // two sliders for min and max delay
                            ImGui::SliderInt("Min Delay", &random_min_delay[i], 5, 6000);
                            ImGui::SliderInt("Max Delay", &random_max_delay[i], 5, 6000);
                        }
    
                        double now = ImGui::GetTime();
                        
                        if (repeat[i] && (now - last_sent_time[i] >= repeat_delay[i]) && curr_running[i]) {
                            bool is_push = (push_directions[i] == 1);
                            if (control_mode[i] == 0) {
                                serial.send_pump_command(pump_ids[i], is_push, microliters[i], delivery_ms[i]);
                            }
                            else {
                                serial.send_pump_command(pump_ids[i], is_push, cycles[i], delays[i]);
                            }
                            if (randomize[i]) {
                                // Randomize the repeat delay
                                repeat_delay[i] = random_min_delay[i] + (rand() % (random_max_delay[i] - random_min_delay[i] + 1));
                            }
                            last_sent_time[i] = now;
                            std::time_t dispense_time = std::time(nullptr);
                            char ts[64];
                            std::strftime(ts, sizeof(ts), "[%Y-%m-%d %H:%M:%S] ", std::localtime(&dispense_time));
                            std::cout << ts << "Dispensing pump " << pump_ids[i] << std::endl;
                            dynamic_circle_start_time = ImGui::GetTime();
                            dynamic_circle_radius = 0.00f;
                            
                        }
                    }
            
                    ImGui::Separator();
                    ImGui::PopID();
                }
    
                if (serial.is_open()) {
                    if (ImGui::Button("Send All Commands")) {
                        for (int i = 0; i < 3; ++i) {
                            if (repeat[i]) {
                                curr_running[i] = true;
                            } else {
                                bool is_push = (push_directions[i] == 1);
                                if (control_mode[i] == 0) {
                                    serial.send_pump_command(pump_ids[i], is_push, microliters[i], delivery_ms[i]);
                                }
                                else {
                                    serial.send_pump_command(pump_ids[i], is_push, cycles[i], delays[i]);
                                }
                            }
                            std::time_t dispense_time = std::time(nullptr);
                            char ts[64];
                            std::strftime(ts, sizeof(ts), "[%Y-%m-%d %H:%M:%S] ", std::localtime(&dispense_time));
                            std::cout << ts << "Dispensing pump " << pump_ids[i] << std::endl;
                        }
                    } ImGui::SameLine();
                    if (ImGui::Button("Stop All Commands")) {
                        for (int i = 0; i < 3; ++i) {
                            curr_running[i] = false;
                            last_sent_time[i] = 0.0;
                        }
                    }
                } else {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Serial port not open");
                }
    
                
    
                if (ImGui::BeginCombo("Select Config", current_filename.c_str())) {
                    for (int i = 0; i < config_files.size(); ++i) {
                        std::string full_path = config_files[i];
                        std::size_t pos = full_path.find_last_of("/\\");
                        std::string filename = (pos == std::string::npos) ? full_path : full_path.substr(pos + 1);
                        bool is_selected = (selected_config_index == i);
                        if (ImGui::Selectable(filename.c_str(), is_selected)) {
                            selected_config_index = i;
                            current_config_file = config_files[i];
                
                            if (load_pump_config(current_config_file, cfg)) {
                                const auto& configs = get_loaded_pump_configs(current_config_file);
                            
                                initialize_pump_state_from_config(configs, pump_ids, microliters, delivery_ms, cycles, delays, push_directions, control_mode, repeat, repeat_delay);      
                            }
                        }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
    
                if (ImGui::Button("Reload Config")) {
                    config_files = list_json_files_in_folder();  // refresh file list
                    if (!config_files.empty() && selected_config_index < config_files.size()) {
                        current_config_file = config_files[selected_config_index];
                        if (load_pump_config(current_config_file, cfg)) {
                            const auto& configs = get_loaded_pump_configs(current_config_file);
                
                            initialize_pump_state_from_config(configs, pump_ids, microliters, delivery_ms, cycles, delays, push_directions, control_mode, repeat, repeat_delay);    
                        }
                    }
                }
            }
            ImGui::End();

            ImGui::Begin("Spotlight Controls");
            
            if (!has_second_monitor) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "No second monitor detected!");
            } else {
                ImGui::Checkbox("Use Second Monitor", &use_second_monitor);
                
                // ImGui::SliderFloat2("Circle Center", (float*)&circle_center, 0.0f, 1.0f);
                ImGui::SliderFloat("Circle Radius", &circle_radius, 0.01f, 0.5f);
                ImGui::SliderFloat("Inner Circle Radius", &inner_radius, 0.01f, 1.0f);
                ImGui::ColorEdit4("Circle Color", (float*)&circle_color);
                ImGui::SliderFloat("Circle Rotation", &theta_rotation, 0.0f, 6.28319f);
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
                central_circle_center = ImVec2(0.5f, 0.5f); // Recenter
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
                    dynamic_circle_start_time = -999;  // Start timer
                    dynamic_circle_radius = 0.00f; // Reset initial radius
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

            {
                std::lock_guard<std::mutex> lock(box_mutex);
                boxes = latest_boxes;
            }

            float xcenter, ycenter;
            for (const auto& obj : boxes) {
                xcenter = obj.rect.x + obj.rect.width * 4 / 5.0f;
                ycenter = obj.rect.y - obj.rect.height * 1 / 5.0f;
                float cx = (xcenter - 1107) / 1020 * height + (width - height) / 2;
                cx = width - cx; // reflect so projection shows up correctly
                float cy = (ycenter - 543) / 1020 * height;
                float radius = circle_radius * height;
                
                if (collision_enabled) {
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
                
                draw_filled_ring(cx, cy, radius, radius * inner_radius, circle_color, alternate_circle_color, circle_segments); 
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
                central_circle_center = lerp(central_circle_center, center_normalized, drift_speed); 
                central_pixel_pos.x = central_circle_center.x * width;
                central_pixel_pos.y = central_circle_center.y * height;
            }

            // Convert back to normalized coordinates
            central_circle_center.x = central_pixel_pos.x / width;
            central_circle_center.y = central_pixel_pos.y / height;

            if (!dynamic_circle) {
                draw_filled_circle(central_pixel_pos.x, central_pixel_pos.y, central_pixel_radius, central_circle_color,  central_circle_segments);
            } else {
                double elapsed = ImGui::GetTime() - dynamic_circle_start_time;

                // Only update and draw if within duration
                if (elapsed <= dynamic_circle_linger_duration + dynamic_circle_max_duration) {
                    dynamic_circle_radius = std::min(
                        dynamic_circle_max_radius,
                        ((float) elapsed / dynamic_circle_max_duration) * dynamic_circle_max_radius
                    );
                    draw_filled_circle(central_pixel_pos.x, central_pixel_pos.y,
                                    dynamic_circle_radius * std::min(width, height),
                                    central_circle_color, alternate_central_circle_color, central_circle_segments);
                } else { 
                    dynamic_circle_radius = 0.0f; // Reset radius
                }
            }
            // Draw central circle
            

            if (calibrating) {
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