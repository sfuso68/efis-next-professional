#pragma once

// FILE GENERATO. Non modificare a mano.
//
// Prodotto da tools/mavlink/generate_message_table.py a partire da
// pymavlink 2.4.49, dialetto ardupilotmega.
//
// Contiene, per ogni messaggio interpretato da EFIS NEXT, il CRC_EXTRA e la
// disposizione dei campi sul filo. Sono dati definiti dal protocollo: qui non
// c'e' logica, e nessuna dipendenza viene introdotta a runtime. Il parser che
// li usa e' scritto nel progetto.
//
// Gli offset seguono l'ordine MAVLink, per dimensione decrescente, non quello
// di dichiarazione.
//
// `length` e' il payload completo, `base_length` la parte definita quando il
// messaggio e' nato. Un frame piu' corto di base_length ha semplicemente gli
// zeri finali rimossi, come il protocollo consente. Un frame che si ferma fra
// base_length e length proviene invece da un trasmettitore che le estensioni
// non le conosce: quei campi sono assenti, non nulli, e leggerli come zero
// significherebbe inventare un dato.

#include "efis/providers/mavlink_types.hpp"

namespace efis::mavlink::generated {

// HEARTBEAT (0), payload 9 byte
inline constexpr FieldDefinition heartbeat_fields[] = {
    {"custom_mode", FieldType::u32, 0, 0},
    {"type", FieldType::u8, 0, 4},
    {"autopilot", FieldType::u8, 0, 5},
    {"base_mode", FieldType::u8, 0, 6},
    {"system_status", FieldType::u8, 0, 7},
    {"mavlink_version", FieldType::u8, 0, 8},
};

// SYS_STATUS (1), payload 31 byte
inline constexpr FieldDefinition sys_status_fields[] = {
    {"onboard_control_sensors_present", FieldType::u32, 0, 0},
    {"onboard_control_sensors_enabled", FieldType::u32, 0, 4},
    {"onboard_control_sensors_health", FieldType::u32, 0, 8},
    {"load", FieldType::u16, 0, 12},
    {"voltage_battery", FieldType::u16, 0, 14},
    {"current_battery", FieldType::i16, 0, 16},
    {"drop_rate_comm", FieldType::u16, 0, 18},
    {"errors_comm", FieldType::u16, 0, 20},
    {"errors_count1", FieldType::u16, 0, 22},
    {"errors_count2", FieldType::u16, 0, 24},
    {"errors_count3", FieldType::u16, 0, 26},
    {"errors_count4", FieldType::u16, 0, 28},
    {"battery_remaining", FieldType::i8, 0, 30},
};

// SYSTEM_TIME (2), payload 12 byte
inline constexpr FieldDefinition system_time_fields[] = {
    {"time_unix_usec", FieldType::u64, 0, 0},
    {"time_boot_ms", FieldType::u32, 0, 8},
};

// POWER_STATUS (125), payload 6 byte
inline constexpr FieldDefinition power_status_fields[] = {
    {"Vcc", FieldType::u16, 0, 0},
    {"Vservo", FieldType::u16, 0, 2},
    {"flags", FieldType::u16, 0, 4},
};

// BATTERY_STATUS (147), payload 54 byte, di cui 18 byte di estensione
inline constexpr FieldDefinition battery_status_fields[] = {
    {"current_consumed", FieldType::i32, 0, 0},
    {"energy_consumed", FieldType::i32, 0, 4},
    {"temperature", FieldType::i16, 0, 8},
    {"voltages", FieldType::u16, 10, 10},
    {"current_battery", FieldType::i16, 0, 30},
    {"id", FieldType::u8, 0, 32},
    {"battery_function", FieldType::u8, 0, 33},
    {"type", FieldType::u8, 0, 34},
    {"battery_remaining", FieldType::i8, 0, 35},
    {"time_remaining", FieldType::i32, 0, 36},  // estensione
    {"charge_state", FieldType::u8, 0, 40},  // estensione
    {"voltages_ext", FieldType::u16, 4, 41},  // estensione
    {"mode", FieldType::u8, 0, 49},  // estensione
    {"fault_bitmask", FieldType::u32, 0, 50},  // estensione
};

// MEMINFO (152), payload 8 byte, di cui 4 byte di estensione
inline constexpr FieldDefinition meminfo_fields[] = {
    {"brkval", FieldType::u16, 0, 0},
    {"freemem", FieldType::u16, 0, 2},
    {"freemem32", FieldType::u32, 0, 4},  // estensione
};

// HWSTATUS (165), payload 3 byte
inline constexpr FieldDefinition hwstatus_fields[] = {
    {"Vcc", FieldType::u16, 0, 0},
    {"I2Cerr", FieldType::u8, 0, 2},
};

// STATUSTEXT (253), payload 54 byte, di cui 3 byte di estensione
inline constexpr FieldDefinition statustext_fields[] = {
    {"severity", FieldType::u8, 0, 0},
    {"text", FieldType::ch, 50, 1},
    {"id", FieldType::u16, 0, 51},  // estensione
    {"chunk_seq", FieldType::u8, 0, 53},  // estensione
};

// EKF_STATUS_REPORT (193), payload 26 byte, di cui 4 byte di estensione
inline constexpr FieldDefinition ekf_status_report_fields[] = {
    {"velocity_variance", FieldType::f32, 0, 0},
    {"pos_horiz_variance", FieldType::f32, 0, 4},
    {"pos_vert_variance", FieldType::f32, 0, 8},
    {"compass_variance", FieldType::f32, 0, 12},
    {"terrain_alt_variance", FieldType::f32, 0, 16},
    {"flags", FieldType::u16, 0, 20},
    {"airspeed_variance", FieldType::f32, 0, 22},  // estensione
};

// VIBRATION (241), payload 32 byte
inline constexpr FieldDefinition vibration_fields[] = {
    {"time_usec", FieldType::u64, 0, 0},
    {"vibration_x", FieldType::f32, 0, 8},
    {"vibration_y", FieldType::f32, 0, 12},
    {"vibration_z", FieldType::f32, 0, 16},
    {"clipping_0", FieldType::u32, 0, 20},
    {"clipping_1", FieldType::u32, 0, 24},
    {"clipping_2", FieldType::u32, 0, 28},
};

// ATTITUDE (30), payload 28 byte
inline constexpr FieldDefinition attitude_fields[] = {
    {"time_boot_ms", FieldType::u32, 0, 0},
    {"roll", FieldType::f32, 0, 4},
    {"pitch", FieldType::f32, 0, 8},
    {"yaw", FieldType::f32, 0, 12},
    {"rollspeed", FieldType::f32, 0, 16},
    {"pitchspeed", FieldType::f32, 0, 20},
    {"yawspeed", FieldType::f32, 0, 24},
};

// ATTITUDE_QUATERNION (31), payload 48 byte, di cui 16 byte di estensione
inline constexpr FieldDefinition attitude_quaternion_fields[] = {
    {"time_boot_ms", FieldType::u32, 0, 0},
    {"q1", FieldType::f32, 0, 4},
    {"q2", FieldType::f32, 0, 8},
    {"q3", FieldType::f32, 0, 12},
    {"q4", FieldType::f32, 0, 16},
    {"rollspeed", FieldType::f32, 0, 20},
    {"pitchspeed", FieldType::f32, 0, 24},
    {"yawspeed", FieldType::f32, 0, 28},
    {"repr_offset_q", FieldType::f32, 4, 32},  // estensione
};

// AHRS (163), payload 28 byte
inline constexpr FieldDefinition ahrs_fields[] = {
    {"omegaIx", FieldType::f32, 0, 0},
    {"omegaIy", FieldType::f32, 0, 4},
    {"omegaIz", FieldType::f32, 0, 8},
    {"accel_weight", FieldType::f32, 0, 12},
    {"renorm_val", FieldType::f32, 0, 16},
    {"error_rp", FieldType::f32, 0, 20},
    {"error_yaw", FieldType::f32, 0, 24},
};

// AHRS2 (178), payload 24 byte
inline constexpr FieldDefinition ahrs2_fields[] = {
    {"roll", FieldType::f32, 0, 0},
    {"pitch", FieldType::f32, 0, 4},
    {"yaw", FieldType::f32, 0, 8},
    {"altitude", FieldType::f32, 0, 12},
    {"lat", FieldType::i32, 0, 16},
    {"lng", FieldType::i32, 0, 20},
};

// GPS_RAW_INT (24), payload 52 byte, di cui 22 byte di estensione
inline constexpr FieldDefinition gps_raw_int_fields[] = {
    {"time_usec", FieldType::u64, 0, 0},
    {"lat", FieldType::i32, 0, 8},
    {"lon", FieldType::i32, 0, 12},
    {"alt", FieldType::i32, 0, 16},
    {"eph", FieldType::u16, 0, 20},
    {"epv", FieldType::u16, 0, 22},
    {"vel", FieldType::u16, 0, 24},
    {"cog", FieldType::u16, 0, 26},
    {"fix_type", FieldType::u8, 0, 28},
    {"satellites_visible", FieldType::u8, 0, 29},
    {"alt_ellipsoid", FieldType::i32, 0, 30},  // estensione
    {"h_acc", FieldType::u32, 0, 34},  // estensione
    {"v_acc", FieldType::u32, 0, 38},  // estensione
    {"vel_acc", FieldType::u32, 0, 42},  // estensione
    {"hdg_acc", FieldType::u32, 0, 46},  // estensione
    {"yaw", FieldType::u16, 0, 50},  // estensione
};

// GPS2_RAW (124), payload 57 byte, di cui 22 byte di estensione
inline constexpr FieldDefinition gps2_raw_fields[] = {
    {"time_usec", FieldType::u64, 0, 0},
    {"lat", FieldType::i32, 0, 8},
    {"lon", FieldType::i32, 0, 12},
    {"alt", FieldType::i32, 0, 16},
    {"dgps_age", FieldType::u32, 0, 20},
    {"eph", FieldType::u16, 0, 24},
    {"epv", FieldType::u16, 0, 26},
    {"vel", FieldType::u16, 0, 28},
    {"cog", FieldType::u16, 0, 30},
    {"fix_type", FieldType::u8, 0, 32},
    {"satellites_visible", FieldType::u8, 0, 33},
    {"dgps_numch", FieldType::u8, 0, 34},
    {"yaw", FieldType::u16, 0, 35},  // estensione
    {"alt_ellipsoid", FieldType::i32, 0, 37},  // estensione
    {"h_acc", FieldType::u32, 0, 41},  // estensione
    {"v_acc", FieldType::u32, 0, 45},  // estensione
    {"vel_acc", FieldType::u32, 0, 49},  // estensione
    {"hdg_acc", FieldType::u32, 0, 53},  // estensione
};

// GLOBAL_POSITION_INT (33), payload 28 byte
inline constexpr FieldDefinition global_position_int_fields[] = {
    {"time_boot_ms", FieldType::u32, 0, 0},
    {"lat", FieldType::i32, 0, 4},
    {"lon", FieldType::i32, 0, 8},
    {"alt", FieldType::i32, 0, 12},
    {"relative_alt", FieldType::i32, 0, 16},
    {"vx", FieldType::i16, 0, 20},
    {"vy", FieldType::i16, 0, 22},
    {"vz", FieldType::i16, 0, 24},
    {"hdg", FieldType::u16, 0, 26},
};

// HOME_POSITION (242), payload 60 byte, di cui 8 byte di estensione
inline constexpr FieldDefinition home_position_fields[] = {
    {"latitude", FieldType::i32, 0, 0},
    {"longitude", FieldType::i32, 0, 4},
    {"altitude", FieldType::i32, 0, 8},
    {"x", FieldType::f32, 0, 12},
    {"y", FieldType::f32, 0, 16},
    {"z", FieldType::f32, 0, 20},
    {"q", FieldType::f32, 4, 24},
    {"approach_x", FieldType::f32, 0, 40},
    {"approach_y", FieldType::f32, 0, 44},
    {"approach_z", FieldType::f32, 0, 48},
    {"time_usec", FieldType::u64, 0, 52},  // estensione
};

// TERRAIN_REPORT (136), payload 22 byte
inline constexpr FieldDefinition terrain_report_fields[] = {
    {"lat", FieldType::i32, 0, 0},
    {"lon", FieldType::i32, 0, 4},
    {"terrain_height", FieldType::f32, 0, 8},
    {"current_height", FieldType::f32, 0, 12},
    {"spacing", FieldType::u16, 0, 16},
    {"pending", FieldType::u16, 0, 18},
    {"loaded", FieldType::u16, 0, 20},
};

// VFR_HUD (74), payload 20 byte
inline constexpr FieldDefinition vfr_hud_fields[] = {
    {"airspeed", FieldType::f32, 0, 0},
    {"groundspeed", FieldType::f32, 0, 4},
    {"alt", FieldType::f32, 0, 8},
    {"climb", FieldType::f32, 0, 12},
    {"heading", FieldType::i16, 0, 16},
    {"throttle", FieldType::u16, 0, 18},
};

// SCALED_PRESSURE (29), payload 16 byte, di cui 2 byte di estensione
inline constexpr FieldDefinition scaled_pressure_fields[] = {
    {"time_boot_ms", FieldType::u32, 0, 0},
    {"press_abs", FieldType::f32, 0, 4},
    {"press_diff", FieldType::f32, 0, 8},
    {"temperature", FieldType::i16, 0, 12},
    {"temperature_press_diff", FieldType::i16, 0, 14},  // estensione
};

// SCALED_PRESSURE2 (137), payload 16 byte, di cui 2 byte di estensione
inline constexpr FieldDefinition scaled_pressure2_fields[] = {
    {"time_boot_ms", FieldType::u32, 0, 0},
    {"press_abs", FieldType::f32, 0, 4},
    {"press_diff", FieldType::f32, 0, 8},
    {"temperature", FieldType::i16, 0, 12},
    {"temperature_press_diff", FieldType::i16, 0, 14},  // estensione
};

// SCALED_PRESSURE3 (143), payload 16 byte, di cui 2 byte di estensione
inline constexpr FieldDefinition scaled_pressure3_fields[] = {
    {"time_boot_ms", FieldType::u32, 0, 0},
    {"press_abs", FieldType::f32, 0, 4},
    {"press_diff", FieldType::f32, 0, 8},
    {"temperature", FieldType::i16, 0, 12},
    {"temperature_press_diff", FieldType::i16, 0, 14},  // estensione
};

// AIRSPEED_AUTOCAL (174), payload 48 byte
inline constexpr FieldDefinition airspeed_autocal_fields[] = {
    {"vx", FieldType::f32, 0, 0},
    {"vy", FieldType::f32, 0, 4},
    {"vz", FieldType::f32, 0, 8},
    {"diff_pressure", FieldType::f32, 0, 12},
    {"EAS2TAS", FieldType::f32, 0, 16},
    {"ratio", FieldType::f32, 0, 20},
    {"state_x", FieldType::f32, 0, 24},
    {"state_y", FieldType::f32, 0, 28},
    {"state_z", FieldType::f32, 0, 32},
    {"Pax", FieldType::f32, 0, 36},
    {"Pby", FieldType::f32, 0, 40},
    {"Pcz", FieldType::f32, 0, 44},
};

// WIND (168), payload 12 byte
inline constexpr FieldDefinition wind_fields[] = {
    {"direction", FieldType::f32, 0, 0},
    {"speed", FieldType::f32, 0, 4},
    {"speed_z", FieldType::f32, 0, 8},
};

// RAW_IMU (27), payload 29 byte, di cui 3 byte di estensione
inline constexpr FieldDefinition raw_imu_fields[] = {
    {"time_usec", FieldType::u64, 0, 0},
    {"xacc", FieldType::i16, 0, 8},
    {"yacc", FieldType::i16, 0, 10},
    {"zacc", FieldType::i16, 0, 12},
    {"xgyro", FieldType::i16, 0, 14},
    {"ygyro", FieldType::i16, 0, 16},
    {"zgyro", FieldType::i16, 0, 18},
    {"xmag", FieldType::i16, 0, 20},
    {"ymag", FieldType::i16, 0, 22},
    {"zmag", FieldType::i16, 0, 24},
    {"id", FieldType::u8, 0, 26},  // estensione
    {"temperature", FieldType::i16, 0, 27},  // estensione
};

// SCALED_IMU2 (116), payload 24 byte, di cui 2 byte di estensione
inline constexpr FieldDefinition scaled_imu2_fields[] = {
    {"time_boot_ms", FieldType::u32, 0, 0},
    {"xacc", FieldType::i16, 0, 4},
    {"yacc", FieldType::i16, 0, 6},
    {"zacc", FieldType::i16, 0, 8},
    {"xgyro", FieldType::i16, 0, 10},
    {"ygyro", FieldType::i16, 0, 12},
    {"zgyro", FieldType::i16, 0, 14},
    {"xmag", FieldType::i16, 0, 16},
    {"ymag", FieldType::i16, 0, 18},
    {"zmag", FieldType::i16, 0, 20},
    {"temperature", FieldType::i16, 0, 22},  // estensione
};

// SCALED_IMU3 (129), payload 24 byte, di cui 2 byte di estensione
inline constexpr FieldDefinition scaled_imu3_fields[] = {
    {"time_boot_ms", FieldType::u32, 0, 0},
    {"xacc", FieldType::i16, 0, 4},
    {"yacc", FieldType::i16, 0, 6},
    {"zacc", FieldType::i16, 0, 8},
    {"xgyro", FieldType::i16, 0, 10},
    {"ygyro", FieldType::i16, 0, 12},
    {"zgyro", FieldType::i16, 0, 14},
    {"xmag", FieldType::i16, 0, 16},
    {"ymag", FieldType::i16, 0, 18},
    {"zmag", FieldType::i16, 0, 20},
    {"temperature", FieldType::i16, 0, 22},  // estensione
};

// SENSOR_OFFSETS (150), payload 42 byte
inline constexpr FieldDefinition sensor_offsets_fields[] = {
    {"mag_declination", FieldType::f32, 0, 0},
    {"raw_press", FieldType::i32, 0, 4},
    {"raw_temp", FieldType::i32, 0, 8},
    {"gyro_cal_x", FieldType::f32, 0, 12},
    {"gyro_cal_y", FieldType::f32, 0, 16},
    {"gyro_cal_z", FieldType::f32, 0, 20},
    {"accel_cal_x", FieldType::f32, 0, 24},
    {"accel_cal_y", FieldType::f32, 0, 28},
    {"accel_cal_z", FieldType::f32, 0, 32},
    {"mag_ofs_x", FieldType::i16, 0, 36},
    {"mag_ofs_y", FieldType::i16, 0, 38},
    {"mag_ofs_z", FieldType::i16, 0, 40},
};

// NAV_CONTROLLER_OUTPUT (62), payload 26 byte
inline constexpr FieldDefinition nav_controller_output_fields[] = {
    {"nav_roll", FieldType::f32, 0, 0},
    {"nav_pitch", FieldType::f32, 0, 4},
    {"alt_error", FieldType::f32, 0, 8},
    {"aspd_error", FieldType::f32, 0, 12},
    {"xtrack_error", FieldType::f32, 0, 16},
    {"nav_bearing", FieldType::i16, 0, 20},
    {"target_bearing", FieldType::i16, 0, 22},
    {"wp_dist", FieldType::u16, 0, 24},
};

// MISSION_CURRENT (42), payload 6 byte, di cui 4 byte di estensione
inline constexpr FieldDefinition mission_current_fields[] = {
    {"seq", FieldType::u16, 0, 0},
    {"total", FieldType::u16, 0, 2},  // estensione
    {"mission_state", FieldType::u8, 0, 4},  // estensione
    {"mission_mode", FieldType::u8, 0, 5},  // estensione
};

// RC_CHANNELS (65), payload 42 byte
inline constexpr FieldDefinition rc_channels_fields[] = {
    {"time_boot_ms", FieldType::u32, 0, 0},
    {"chan1_raw", FieldType::u16, 0, 4},
    {"chan2_raw", FieldType::u16, 0, 6},
    {"chan3_raw", FieldType::u16, 0, 8},
    {"chan4_raw", FieldType::u16, 0, 10},
    {"chan5_raw", FieldType::u16, 0, 12},
    {"chan6_raw", FieldType::u16, 0, 14},
    {"chan7_raw", FieldType::u16, 0, 16},
    {"chan8_raw", FieldType::u16, 0, 18},
    {"chan9_raw", FieldType::u16, 0, 20},
    {"chan10_raw", FieldType::u16, 0, 22},
    {"chan11_raw", FieldType::u16, 0, 24},
    {"chan12_raw", FieldType::u16, 0, 26},
    {"chan13_raw", FieldType::u16, 0, 28},
    {"chan14_raw", FieldType::u16, 0, 30},
    {"chan15_raw", FieldType::u16, 0, 32},
    {"chan16_raw", FieldType::u16, 0, 34},
    {"chan17_raw", FieldType::u16, 0, 36},
    {"chan18_raw", FieldType::u16, 0, 38},
    {"chancount", FieldType::u8, 0, 40},
    {"rssi", FieldType::u8, 0, 41},
};

// SERVO_OUTPUT_RAW (36), payload 37 byte, di cui 16 byte di estensione
inline constexpr FieldDefinition servo_output_raw_fields[] = {
    {"time_usec", FieldType::u32, 0, 0},
    {"servo1_raw", FieldType::u16, 0, 4},
    {"servo2_raw", FieldType::u16, 0, 6},
    {"servo3_raw", FieldType::u16, 0, 8},
    {"servo4_raw", FieldType::u16, 0, 10},
    {"servo5_raw", FieldType::u16, 0, 12},
    {"servo6_raw", FieldType::u16, 0, 14},
    {"servo7_raw", FieldType::u16, 0, 16},
    {"servo8_raw", FieldType::u16, 0, 18},
    {"port", FieldType::u8, 0, 20},
    {"servo9_raw", FieldType::u16, 0, 21},  // estensione
    {"servo10_raw", FieldType::u16, 0, 23},  // estensione
    {"servo11_raw", FieldType::u16, 0, 25},  // estensione
    {"servo12_raw", FieldType::u16, 0, 27},  // estensione
    {"servo13_raw", FieldType::u16, 0, 29},  // estensione
    {"servo14_raw", FieldType::u16, 0, 31},  // estensione
    {"servo15_raw", FieldType::u16, 0, 33},  // estensione
    {"servo16_raw", FieldType::u16, 0, 35},  // estensione
};

// AOA_SSA (11020), payload 16 byte
inline constexpr FieldDefinition aoa_ssa_fields[] = {
    {"time_usec", FieldType::u64, 0, 0},
    {"AOA", FieldType::f32, 0, 8},
    {"SSA", FieldType::f32, 0, 12},
};

// MCU_STATUS (11039), payload 9 byte
inline constexpr FieldDefinition mcu_status_fields[] = {
    {"MCU_temperature", FieldType::i16, 0, 0},
    {"MCU_voltage", FieldType::u16, 0, 2},
    {"MCU_voltage_min", FieldType::u16, 0, 4},
    {"MCU_voltage_max", FieldType::u16, 0, 6},
    {"id", FieldType::u8, 0, 8},
};

// AIRSPEED (295), payload 12 byte, definito nel progetto
inline constexpr FieldDefinition airspeed_fields[] = {
    {"airspeed", FieldType::f32, 0, 0},
    {"raw_press", FieldType::f32, 0, 4},
    {"temperature", FieldType::i16, 0, 8},
    {"id", FieldType::u8, 0, 10},
    {"flags", FieldType::u8, 0, 11},
};

inline constexpr MessageDefinition messages[] = {
    {0, "HEARTBEAT", 50, 9, 9, heartbeat_fields, 6},
    {1, "SYS_STATUS", 124, 31, 31, sys_status_fields, 13},
    {2, "SYSTEM_TIME", 137, 12, 12, system_time_fields, 2},
    {24, "GPS_RAW_INT", 24, 52, 30, gps_raw_int_fields, 16},
    {27, "RAW_IMU", 144, 29, 26, raw_imu_fields, 12},
    {29, "SCALED_PRESSURE", 115, 16, 14, scaled_pressure_fields, 5},
    {30, "ATTITUDE", 39, 28, 28, attitude_fields, 7},
    {31, "ATTITUDE_QUATERNION", 246, 48, 32, attitude_quaternion_fields, 9},
    {33, "GLOBAL_POSITION_INT", 104, 28, 28, global_position_int_fields, 9},
    {36, "SERVO_OUTPUT_RAW", 222, 37, 21, servo_output_raw_fields, 18},
    {42, "MISSION_CURRENT", 28, 6, 2, mission_current_fields, 4},
    {62, "NAV_CONTROLLER_OUTPUT", 183, 26, 26, nav_controller_output_fields, 8},
    {65, "RC_CHANNELS", 118, 42, 42, rc_channels_fields, 21},
    {74, "VFR_HUD", 20, 20, 20, vfr_hud_fields, 6},
    {116, "SCALED_IMU2", 76, 24, 22, scaled_imu2_fields, 11},
    {124, "GPS2_RAW", 87, 57, 35, gps2_raw_fields, 18},
    {125, "POWER_STATUS", 203, 6, 6, power_status_fields, 3},
    {129, "SCALED_IMU3", 46, 24, 22, scaled_imu3_fields, 11},
    {136, "TERRAIN_REPORT", 1, 22, 22, terrain_report_fields, 7},
    {137, "SCALED_PRESSURE2", 195, 16, 14, scaled_pressure2_fields, 5},
    {143, "SCALED_PRESSURE3", 131, 16, 14, scaled_pressure3_fields, 5},
    {147, "BATTERY_STATUS", 154, 54, 36, battery_status_fields, 14},
    {150, "SENSOR_OFFSETS", 134, 42, 42, sensor_offsets_fields, 12},
    {152, "MEMINFO", 208, 8, 4, meminfo_fields, 3},
    {163, "AHRS", 127, 28, 28, ahrs_fields, 7},
    {165, "HWSTATUS", 21, 3, 3, hwstatus_fields, 2},
    {168, "WIND", 1, 12, 12, wind_fields, 3},
    {174, "AIRSPEED_AUTOCAL", 167, 48, 48, airspeed_autocal_fields, 12},
    {178, "AHRS2", 47, 24, 24, ahrs2_fields, 6},
    {193, "EKF_STATUS_REPORT", 71, 26, 22, ekf_status_report_fields, 7},
    {241, "VIBRATION", 90, 32, 32, vibration_fields, 7},
    {242, "HOME_POSITION", 104, 60, 52, home_position_fields, 11},
    {253, "STATUSTEXT", 83, 54, 51, statustext_fields, 4},
    {295, "AIRSPEED", 234, 12, 12, airspeed_fields, 5},
    {11020, "AOA_SSA", 205, 16, 16, aoa_ssa_fields, 3},
    {11039, "MCU_STATUS", 142, 9, 9, mcu_status_fields, 5},
};

}  // namespace efis::mavlink::generated
