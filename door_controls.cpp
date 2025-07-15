#include "door_controls.h"
#include "serial/serial.h"
#include "imgui.h"
#include <vector>
#include <string>

namespace {
static SerialPort serial_door;
static int selected_door_port = -1;
static std::vector<std::string> door_port_list;
static int object_limit = 3;
static int prev_count = -1;
static bool manual_override = false; // Manual override flag
static bool door_selected[3] = {false, false, false}; // Selection state for doors 1, 2, 3
}

void RenderDoorControls() {
    ImGui::Begin("Door Control");
    if (ImGui::Button("Scan Ports")) {
        door_port_list = SerialPort::list_available_ports();
        selected_door_port = -1;
    }
    for (int i = 0; i < door_port_list.size(); i++) {
        if (ImGui::RadioButton(door_port_list[i].c_str(), selected_door_port == i)) {
            selected_door_port = i;
        }
    }
    if (selected_door_port >= 0 && !serial_door.is_open()) {
        if (ImGui::Button("Open Door Port")) {
            serial_door.open(door_port_list[selected_door_port]);
        }
    }
    if (serial_door.is_open()) {
        ImGui::Text("Door Port Open");
        if (ImGui::Button("Close Door Port")) {
            serial_door.close();
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Door port not open");
    }
    
    if (serial_door.is_open()) {
        ImGui::Spacing();
        ImGui::Text("Individual Door Controls:");
        
        // Door selection checkboxes
        ImGui::Checkbox("Door 1", &door_selected[0]); ImGui::SameLine();
        ImGui::Checkbox("Door 2", &door_selected[1]); ImGui::SameLine();
        ImGui::Checkbox("Door 3", &door_selected[2]);
        
        ImGui::Spacing();
        
        // Individual door buttons
        if (ImGui::Button("Open Door 1")) {
            serial_door.send_door_command("o1");
        } ImGui::SameLine();
        if (ImGui::Button("Close Door 1")) {
            serial_door.send_door_command("c1");
        }
        
        if (ImGui::Button("Open Door 2")) {
            serial_door.send_door_command("o2");
        } ImGui::SameLine();
        if (ImGui::Button("Close Door 2")) {
            serial_door.send_door_command("c2");
        }
        
        if (ImGui::Button("Open Door 3")) {
            serial_door.send_door_command("o3");
        } ImGui::SameLine();
        if (ImGui::Button("Close Door 3")) {
            serial_door.send_door_command("c3");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Multiple Door Controls:");
        
        // Multiple door controls
        if (ImGui::Button("Open Selected")) {
            std::string command;
            for (int i = 0; i < 3; i++) {
                if (door_selected[i]) {
                    command += "o" + std::to_string(i + 1);
                }
            }
            if (!command.empty()) {
                serial_door.send_door_command(command);
            }
        } ImGui::SameLine();
        
        if (ImGui::Button("Close Selected")) {
            std::string command;
            for (int i = 0; i < 3; i++) {
                if (door_selected[i]) {
                    command += "c" + std::to_string(i + 1);
                }
            }
            if (!command.empty()) {
                serial_door.send_door_command(command);
            }
        }
        
        ImGui::Spacing();
        if (ImGui::Button("Open All")) {
            serial_door.send_door_command("o1o2o3");
        } ImGui::SameLine();
        if (ImGui::Button("Close All")) {
            serial_door.send_door_command("c1c2c3");
        }
        
        ImGui::Spacing();
        ImGui::Checkbox("Manual Override", &manual_override);
        if (manual_override) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Automatic door control disabled");
        } else {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Automatic door control enabled");
        }
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SliderInt("Object Limit", &object_limit, 1, 10);
    ImGui::End();
}

// Interface implementations
bool IsDoorSerialOpen() { return serial_door.is_open(); }
void SendDoorCommand(const std::string& command) { serial_door.send_door_command(command); }
int GetObjectLimit() { return object_limit; }
void SetObjectLimit(int value) { object_limit = value; }
int& RefPrevCount() { return prev_count; }
bool IsManualOverride() { return manual_override; }
