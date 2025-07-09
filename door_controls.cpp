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
        if (ImGui::Button("Open Door")) {
            serial_door.send_door_command(true);
        } ImGui::SameLine();
        if (ImGui::Button("Close Door")) {
            serial_door.send_door_command(false);
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
void SendDoorCommand(bool open) { serial_door.send_door_command(open); }
int GetObjectLimit() { return object_limit; }
void SetObjectLimit(int value) { object_limit = value; }
int& RefPrevCount() { return prev_count; }
bool IsManualOverride() { return manual_override; }
