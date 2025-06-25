#include "pump_controls.h"
#include "serial/serial.h"
#include "imgui.h"
#include "spotlight_controls.h"
#include <vector>
#include <string>
#include <ctime>
#include <iostream>

namespace {
static SerialPort serial;
static std::vector<std::string> port_list;
static int selected_port = -1;
static char send_buffer[128] = "";
static std::string recv_data;

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
}

void RenderPumpControls() {
    if (ImGui::Begin("Serial Control")) {
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
                        // Use setter for dynamic circle if needed
                        RefDynamicCircleStartTime() = ImGui::GetTime();
                        RefDynamicCircleRadius() = 0.00f;
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
                    RefDynamicCircleStartTime() = ImGui::GetTime();
                    RefDynamicCircleRadius() = 0.00f;
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
}
