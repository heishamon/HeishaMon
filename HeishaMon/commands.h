#define LWIP_INTERNAL

#include <ArduinoJson.h>

#define DATASIZE 203

#define INITIALQUERYSIZE 7
extern byte initialQuery[INITIALQUERYSIZE];
#define PANASONICQUERYSIZE 110
extern byte panasonicQuery[PANASONICQUERYSIZE];


#define OPTDATASIZE 20
#define OPTIONALPCBQUERYTIME 1000 //send optional pcb query each second
#define OPTIONALPCBQUERYSIZE 19
#define OPTIONALPCBSAVETIME 300 //save each 5 minutes the current optional pcb state into flash to have valid values during reboot
extern byte optionalPCBQuery[OPTIONALPCBQUERYSIZE];


extern const char* mqtt_topic_values;
extern const char* mqtt_topic_xvalues;
extern const char* mqtt_topic_commands;
extern const char* mqtt_topic_pcbvalues;
extern const char* mqtt_topic_1wire;
extern const char* mqtt_topic_s0;
extern const char* mqtt_topic_pcb;
extern const char* mqtt_logtopic;
extern const char* mqtt_willtopic;
extern const char* mqtt_iptopic;
extern const char* mqtt_send_raw_value_topic;

unsigned int set_heatpump_state(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_pump(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_max_pump_duty(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_quiet_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_z1_heat_request_temperature(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_z1_cool_request_temperature(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_z2_heat_request_temperature(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_z2_cool_request_temperature(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_force_DHW(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_force_defrost(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sterilization_max_time(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sterilization_temp(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_force_sterilization(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_force_heater(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_holiday_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_powerful_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_operation_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_DHW_temp(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_curves(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_zones(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_floor_heat_delta(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_floor_cool_delta(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_dhw_heat_delta(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_reset(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_heater_delay_time(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_heater_start_delta(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_heater_stop_delta(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_main_schedule(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_alt_external_sensor(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_external_pad_heater(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_buffer_delta(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_buffer(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_heatingoffoutdoortemp(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_bivalent_control(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_bivalent_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_bivalent_start_temp(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_bivalent_ap_start_temp(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_bivalent_ap_stop_temp(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_external_control(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_external_error(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_heatingcontrol(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_smart_dhw(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_quiet_mode_priority(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_pump_flowrate_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_dhw_sensor_selection(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_dhw_heater_state(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_room_heater_state(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_heater_on_outdoor_temp(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_heat_to_cool_temp(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_cool_to_heat_temp(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_room_operation_max_time(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_dhw_heatup_max_time(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_quiet_mode_schedule(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_anti_freeze_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_pool_delta(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_demand_control_enable(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_dhw_defrost(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_static_pressure(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_heating_on_outdoor_temp(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_heating_on_delay(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_dhw_booster_delay(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_efficiency_stage1_time(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_efficiency_stage2_time(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_efficiency_stage3_time(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sensor_offset_remote_th1(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sensor_offset_outdoor(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_bivalent_ap_heat(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_bivalent_operating_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_bivalent_external_pump(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_bivalent_ap_start_delay(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_bivalent_ap_stop_delay(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_extra_pump(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_extra_pump_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_internal_heater_capacity(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_solar_on_delta(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_solar_off_delta(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_solar_frost_protection(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_room_holiday_shift_temp(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_zone_type(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_heating_water_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_cooling_water_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_defrost_signal(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sensor_offset_dhw_tank(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sensor_offset_flow(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_liquid_type(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_dhw_heater_type(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_dhw_installed(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_solar_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_solar_high_limit(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sg_ready_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sg_ready_dhw_power_1(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sg_ready_heat_power_1(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sg_ready_cool_power_1(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sg_ready_dhw_power_2(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sg_ready_heat_power_2(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sg_ready_cool_power_2(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sg_ready_stop_power(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sg_ready_cool_capacity_1(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sg_ready_cool_capacity_2(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_sensor_offset_return(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_z1_sensor_settings(char *msg, unsigned char *cmd, char *log_msg);

//optional pcb commands
unsigned int set_heat_cool_mode(char *msg, char *log_msg);
unsigned int set_compressor_state(char *msg, char *log_msg);
unsigned int set_smart_grid_mode(char *msg, char *log_msg);
unsigned int set_external_thermostat_1_state(char *msg, char *log_msg);
unsigned int set_external_thermostat_2_state(char *msg, char *log_msg);
unsigned int set_demand_control(char *msg, char *log_msg);
unsigned int set_pool_temp(char *msg, char *log_msg);
unsigned int set_buffer_temp(char *msg, char *log_msg);
unsigned int set_z1_room_temp(char *msg, char *log_msg);
unsigned int set_z1_water_temp(char *msg, char *log_msg);
unsigned int set_z2_room_temp(char *msg, char *log_msg);
unsigned int set_z2_water_temp(char *msg, char *log_msg);
unsigned int set_solar_temp(char *msg, char *log_msg);
unsigned int set_byte_9(char *msg, char *log_msg);
unsigned int set_external_compressor_control(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_external_heat_cool_control(char *msg, unsigned char *cmd, char *log_msg);

struct cmdStruct {
  char name[29];
  unsigned int (*func)(char *msg, unsigned char *cmd, char *log_msg);
};

const cmdStruct commands[] PROGMEM = {
  // set heatpump state to on by sending 1
  { "SetHeatpump", set_heatpump_state },
  // set pump state to on by sending 1
  { "SetPump", set_pump },
  // set max pump duty
  { "SetMaxPumpDuty", set_max_pump_duty },
  // set 0 for Off mode, set 1 for Quiet mode 1, set 2 for Quiet mode 2, set 3 for Quiet mode 3
  { "SetQuietMode", set_quiet_mode },
  // z1 heat request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  { "SetZ1HeatRequestTemperature", set_z1_heat_request_temperature },
  // z1 cool request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  { "SetZ1CoolRequestTemperature", set_z1_cool_request_temperature },
  // z2 heat request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  { "SetZ2HeatRequestTemperature", set_z2_heat_request_temperature },
  // z2 cool request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  { "SetZ2CoolRequestTemperature", set_z2_cool_request_temperature },
  // set mode to force DHW by sending 1
  { "SetForceDHW", set_force_DHW },
  // set mode to force defrost  by sending 1
  { "SetForceDefrost", set_force_defrost },
  // set mode to force sterilization by sending 1
  { "SetForceSterilization", set_force_sterilization },
  // set sterilization target temperature in whole degrees Celsius (55-75)
  { "SetSterilizationTemp", set_sterilization_temp },
  // set sterilization holding time in 5-minute steps (5-60)
  { "SetSterilizationMaxTime", set_sterilization_max_time },
  // set mode to force heater (emergency heating) by sending 1, off will be 0
  { "SetForceHeater", set_force_heater },
  // set Holiday mode by sending 1, off will be 0
  { "SetHolidayMode", set_holiday_mode },
  // set Powerful mode by sending 0 = off, 1 for 30min, 2 for 60min, 3 for 90 min
  { "SetPowerfulMode", set_powerful_mode },
  // set Heat pump operation mode  3 = DHW only, 0 = heat only, 1 = cool only, 2 = Auto, 4 = Heat+DHW, 5 = Cool+DHW, 6 = Auto + DHW
  { "SetOperationMode", set_operation_mode },
  // set DHW temperature by sending desired temperature between 40C-75C
  { "SetDHWTemp", set_DHW_temp },
  // set heat/cool curves on z1 and z2 using a json input
  { "SetCurves", set_curves },
  // set zones to active
  { "SetZones", set_zones },
  { "SetFloorHeatDelta", set_floor_heat_delta },
  { "SetFloorCoolDelta", set_floor_cool_delta },
  { "SetDHWHeatDelta", set_dhw_heat_delta },
  { "SetReset", set_reset },
  { "SetHeaterDelayTime", set_heater_delay_time },
  { "SetHeaterStartDelta", set_heater_start_delta },
  { "SetHeaterStopDelta", set_heater_stop_delta },
  { "SetMainSchedule", set_main_schedule },
  { "SetAltExternalSensor", set_alt_external_sensor },
  { "SetExternalPadHeater", set_external_pad_heater },
  { "SetBufferDelta", set_buffer_delta },
  { "SetBuffer", set_buffer },
  // set Outdoor Temperature to stop heating 5-35
  { "SetHeatingOffOutdoorTemp", set_heatingoffoutdoortemp },
  { "SetExternalControl", set_external_control },
  { "SetExternalError", set_external_error },
  { "SetExternalCompressorControl", set_external_compressor_control },
  { "SetExternalHeatCoolControl", set_external_heat_cool_control },
  // set Bivalent control mode  0 = off, 1 = on
  { "SetBivalentControl", set_bivalent_control },
  // set Bivalent operation mode  0 = Alternativ, 1 = Parallel, 2 = Advanced Parallel
  { "SetBivalentMode", set_bivalent_mode },
  // bivalent start temp -  set from -15C to 35C 
  { "SetBivalentStartTemp", set_bivalent_start_temp },
  // bivalent AP start temp -  set from -15C to 35C 
  { "SetBivalentAPStartTemp", set_bivalent_ap_start_temp },
  // bivalent AP stop temp -  set from -15C to 35C
  { "SetBivalentAPStopTemp", set_bivalent_ap_stop_temp },
  { "SetHeatingControl", set_heatingcontrol },
  { "SetSmartDHW", set_smart_dhw },
  { "SetQuietModePriority", set_quiet_mode_priority },
  { "SetPumpFlowrateMode", set_pump_flowrate_mode },
  { "SetDHWSensorSelection", set_dhw_sensor_selection },
  { "SetDHWHeaterState", set_dhw_heater_state },
  { "SetRoomHeaterState", set_room_heater_state },
  { "SetHeaterOnOutdoorTemp", set_heater_on_outdoor_temp },
  { "SetHeatToCoolTemp", set_heat_to_cool_temp },
  { "SetCoolToHeatTemp", set_cool_to_heat_temp },
  { "SetRoomOperationMaxTime", set_room_operation_max_time },
  { "SetDHWHeatupMaxTime", set_dhw_heatup_max_time },
  { "SetQuietModeSchedule", set_quiet_mode_schedule },
  { "SetAntiFreezeMode", set_anti_freeze_mode },
  { "SetPoolDelta", set_pool_delta },
  { "SetDemandControlEnable", set_demand_control_enable },
  { "SetDHWDefrost", set_dhw_defrost },
  { "SetStaticPressure", set_static_pressure },
  { "SetHeatingOnOutdoorTemp", set_heating_on_outdoor_temp },
  { "SetHeatingOnDelay", set_heating_on_delay },
  { "SetDHWBoosterDelay", set_dhw_booster_delay },
  { "SetEfficiencyStage1Time", set_efficiency_stage1_time },
  { "SetEfficiencyStage2Time", set_efficiency_stage2_time },
  { "SetEfficiencyStage3Time", set_efficiency_stage3_time },
  { "SetSensorOffsetRemoteTh1", set_sensor_offset_remote_th1 },
  { "SetSensorOffsetOutdoor", set_sensor_offset_outdoor },
  { "SetBivalentAPHeat", set_bivalent_ap_heat },
  { "SetBivalentOperatingMode", set_bivalent_operating_mode },
  { "SetBivalentExternalPump", set_bivalent_external_pump },
  { "SetBivalentAPStartDelay", set_bivalent_ap_start_delay },
  { "SetBivalentAPStopDelay", set_bivalent_ap_stop_delay },
  { "SetExtraPump", set_extra_pump },
  { "SetExtraPumpMode", set_extra_pump_mode },
  { "SetInternalHeaterCapacity", set_internal_heater_capacity },
  { "SetSolarOnDelta", set_solar_on_delta },
  { "SetSolarOffDelta", set_solar_off_delta },
  { "SetSolarFrostProtection", set_solar_frost_protection },
  { "SetRoomHolidayShiftTemp", set_room_holiday_shift_temp },
  { "SetZoneType", set_zone_type },
  { "SetHeatingWaterMode", set_heating_water_mode },
  { "SetCoolingWaterMode", set_cooling_water_mode },
  { "SetLiquidType", set_liquid_type },
  { "SetDHWHeaterType", set_dhw_heater_type },
  { "SetDHWInstalled", set_dhw_installed },
  { "SetSolarMode", set_solar_mode },
  { "SetSolarHighLimit", set_solar_high_limit },
  { "SetSGReadyMode", set_sg_ready_mode },
  { "SetSGReadyDHWPower1", set_sg_ready_dhw_power_1 },
  { "SetSGReadyHeatPower1", set_sg_ready_heat_power_1 },
  { "SetSGReadyCoolPower1", set_sg_ready_cool_power_1 },
  { "SetSGReadyDHWPower2", set_sg_ready_dhw_power_2 },
  { "SetSGReadyHeatPower2", set_sg_ready_heat_power_2 },
  { "SetSGReadyCoolPower2", set_sg_ready_cool_power_2 },
  { "SetSGReadyStopPower", set_sg_ready_stop_power },
  { "SetSGReadyCoolCapacity1", set_sg_ready_cool_capacity_1 },
  { "SetSGReadyCoolCapacity2", set_sg_ready_cool_capacity_2 },
  { "SetSensorOffsetReturn", set_sensor_offset_return },
  { "SetZ1SensorSettings", set_z1_sensor_settings },
  { "SetDefrostSignal", set_defrost_signal },
  { "SetSensorOffsetDhwTank", set_sensor_offset_dhw_tank },
  { "SetSensorOffsetFlow", set_sensor_offset_flow },
};

struct optCmdStruct{
  char name[28];
  unsigned int (*func)(char *msg, char *log_msg);
};

const optCmdStruct optionalCommands[] PROGMEM = {
  // optional PCB
  { "SetHeatCoolMode", set_heat_cool_mode },
  { "SetCompressorState", set_compressor_state },
  { "SetSmartGridMode", set_smart_grid_mode },
  { "SetExternalThermostat1State", set_external_thermostat_1_state },
  { "SetExternalThermostat2State", set_external_thermostat_2_state },
  { "SetDemandControl", set_demand_control },
  { "SetPoolTemp", set_pool_temp },
  { "SetBufferTemp", set_buffer_temp },
  { "SetZ1RoomTemp", set_z1_room_temp },
  { "SetZ1WaterTemp", set_z1_water_temp },
  { "SetZ2RoomTemp", set_z2_room_temp },
  { "SetZ2WaterTemp", set_z2_water_temp },
  { "SetSolarTemp", set_solar_temp },
  { "SetOptPCBByte9", set_byte_9 }
};

void send_heatpump_command(char* topic, char *msg, bool (*send_command)(byte*, int), void (*log_message)(char*), bool optionalPCB);
bool saveOptionalPCB(byte* command, int length);
bool loadOptionalPCB(byte* command, int length);
