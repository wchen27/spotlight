#include "pump_controls.h"
#include "serial/serial.h"
#include "imgui.h"
#include <vector>
#include <string>
#include <ctime>
#include <iostream>

namespace {
// Pump control state (was static in spotlight.cpp)
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
        static std::vector<std::string> config_files = list_json_files_in_folder();
        static int selected_config_index = 0;
        static std::string current_config_file = config_files.empty() ? "" : config_files[0];

        std::size_t pos = current_config_file.find_last_of("/\\");
        std::string current_filename = (pos == std::string::npos) ? current_config_file : current_config_file.substr(pos + 1);

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
                // TODO: call config/init functions as needed
            }
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
                        } else {
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
                    // TODO: load config and initialize pump state
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Reload Config")) {
            config_files = list_json_files_in_folder();
            if (!config_files.empty() && selected_config_index < config_files.size()) {
                current_config_file = config_files[selected_config_index];
                // TODO: reload config and initialize pump state
            }
        }
    }
    ImGui::End();
}
