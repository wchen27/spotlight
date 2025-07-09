#pragma once

void RenderPumpControls();

// Accessors for salesman experiment
class SerialPort;
SerialPort& get_serial();
const char* get_pump_ids();
float get_pump_microliters(int idx);
int get_pump_delivery_ms(int idx);
int get_pump_cycles(int idx);
int get_pump_delays(int idx);
bool get_pump_is_push(int idx);
int get_pump_control_mode(int idx);
