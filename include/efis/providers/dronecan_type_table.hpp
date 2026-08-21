#pragma once

// FILE GENERATO. Non modificare a mano.
//
// Prodotto da tools/dronecan/generate_type_table.py a partire da
// dronecan 1.0.27.
//
// Contiene, per ogni tipo interpretato, la signature che entra nel CRC del
// trasferimento e la disposizione dei campi in bit. Sono dati definiti dal
// protocollo: qui non c'e' logica, e nessuna dipendenza viene introdotta a
// runtime. Il decodificatore che li usa e' scritto nel progetto.
//
// Il DSDL non allinea i campi ai byte: le ampiezze sono in bit, e vanno lette
// nell'ordine in cui compaiono.

#include "efis/providers/dronecan_types.hpp"

namespace efis::dronecan::generated {

// uavcan.equipment.ice.reciprocating.Status
inline constexpr FieldDescriptor uavcan_equipment_ice_reciprocating_status_fields[] = {
    {"state", FieldShape::unsigned_integer, 2},
    {"flags", FieldShape::unsigned_integer, 30},
    {"padding", FieldShape::padding, 16},
    {"engine_load_percent", FieldShape::unsigned_integer, 7},
    {"engine_speed_rpm", FieldShape::unsigned_integer, 17},
    {"spark_dwell_time_ms", FieldShape::floating, 16},
    {"atmospheric_pressure_kpa", FieldShape::floating, 16},
    {"intake_manifold_pressure_kpa", FieldShape::floating, 16},
    {"intake_manifold_temperature", FieldShape::floating, 16},
    {"coolant_temperature", FieldShape::floating, 16},
    {"oil_pressure", FieldShape::floating, 16},
    {"oil_temperature", FieldShape::floating, 16},
    {"fuel_pressure", FieldShape::floating, 16},
    {"fuel_consumption_rate_cm3pm", FieldShape::floating, 32},
    {"estimated_consumed_fuel_volume_cm3", FieldShape::floating, 32},
    {"throttle_position_percent", FieldShape::unsigned_integer, 7},
    {"ecu_index", FieldShape::unsigned_integer, 6},
    {"spark_plug_usage", FieldShape::unsigned_integer, 3},
    {"cylinder_status", FieldShape::array, 0, 16, true},
};

// uavcan.equipment.ice.reciprocating.CylinderStatus
inline constexpr FieldDescriptor uavcan_equipment_ice_reciprocating_cylinderstatus_fields[] = {
    {"ignition_timing_deg", FieldShape::floating, 16},
    {"injection_time_ms", FieldShape::floating, 16},
    {"cylinder_head_temperature", FieldShape::floating, 16},
    {"exhaust_gas_temperature", FieldShape::floating, 16},
    {"lambda_coefficient", FieldShape::floating, 16},
};

// uavcan.protocol.NodeStatus
inline constexpr FieldDescriptor uavcan_protocol_nodestatus_fields[] = {
    {"uptime_sec", FieldShape::unsigned_integer, 32},
    {"health", FieldShape::unsigned_integer, 2},
    {"mode", FieldShape::unsigned_integer, 3},
    {"sub_mode", FieldShape::unsigned_integer, 3},
    {"vendor_specific_status_code", FieldShape::unsigned_integer, 16},
};

inline constexpr TypeDescriptor types[] = {
    {341, "uavcan.protocol.NodeStatus", 0x0F0868D0C1A7C6F1ULL, uavcan_protocol_nodestatus_fields, 5},
    {1120, "uavcan.equipment.ice.reciprocating.Status", 0xD38AA3EE75537EC6ULL, uavcan_equipment_ice_reciprocating_status_fields, 19},
    {0, "uavcan.equipment.ice.reciprocating.CylinderStatus", 0xD68AC83A89D5B36BULL, uavcan_equipment_ice_reciprocating_cylinderstatus_fields, 5},
};

}  // namespace efis::dronecan::generated
