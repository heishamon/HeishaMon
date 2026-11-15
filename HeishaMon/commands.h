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

// Main commands
//
// Almost every command writes one number into one field of the command datagram. Those commands are a row of
// settings in the table below; heatpump_command() turns the payload into the field. A command only gets its own
// function when it can't be described that way (CMD_FUNC).
//
//   field = ((value / divisor + offset) & mask) << shift, written to byte 'address'. All other bytes stay 0,
//   which the heat pump reads as "no change".

enum cmdType : uint8_t {
  CMD_FUNC,    // built by func
  CMD_NUMBER,  // any number, the leading digits of the payload are used (as String::toInt), not range checked
  CMD_CHOICE,  // as CMD_NUMBER, but everything outside 0..maximum counts as 0
  CMD_STRICT,  // the whole payload must be an integer minimum..maximum in multiples of step, else nothing is sent
  CMD_TENTHS,  // as CMD_STRICT, but the payload may have one decimal ("3", "3.5"); value, minimum, maximum and
               // step are in tenths
};

#define CMD_INSTALLER 0x01  // installer setting, only accepted after setInstallerCommandsAllowed(true)

struct cmdStruct {
  char name[29];
  uint8_t flags;
  uint8_t type;
  uint8_t address;
  uint8_t shift;
  uint8_t mask;
  uint8_t offset;
  uint8_t divisor;
  uint8_t step;
  int16_t minimum;
  int16_t maximum;
  unsigned int (*func)(char *msg, unsigned char *cmd, char *log_msg);
};

// The columns after the flags, for the encodings that are used most:
//                                                  type        address    shift    mask  offset divisor step minimum maximum func
// two bits, 0 = off (b01), 1 = on (b10)
#define CMD_ONOFF(address, shift)                   CMD_CHOICE, (address), (shift), 0x03, 1,     1,      1,   0,      1,      NULL
// two bits, 0..maximum is sent as b01, b10, b11
#define CMD_PAIR(address, shift, maximum)           CMD_CHOICE, (address), (shift), 0x03, 1,     1,      1,   0,      (maximum), NULL
// one bit, 1 triggers the action
#define CMD_TRIGGER(address, shift)                 CMD_CHOICE, (address), (shift), 0x01, 0,     1,      1,   0,      1,      NULL
// a whole byte, value + offset (128 for temperatures, 1 for counters)
#define CMD_VALUE(address, offset)                  CMD_NUMBER, (address), 0,       0xFF, (offset), 1,   1,   0,      0,      NULL
// part of a byte, value + offset
#define CMD_FIELD(address, shift, mask, offset)     CMD_NUMBER, (address), (shift), (mask), (offset), 1, 1,   0,      0,      NULL
// a checked value: minimum..maximum in multiples of step, sent as value / divisor + offset
#define CMD_CHECKED(address, shift, minimum, maximum, step, divisor, offset) \
                                                    CMD_STRICT, (address), (shift), 0xFF, (offset), (divisor), (step), (minimum), (maximum), NULL
// a checked value with one decimal: as CMD_CHECKED, with minimum, maximum, step and divisor in tenths
#define CMD_DECIMAL(address, shift, minimum, maximum, step, divisor, offset) \
                                                    CMD_TENTHS, (address), (shift), 0xFF, (offset), (divisor), (step), (minimum), (maximum), NULL
// built by its own function
#define CMD_FUNCTION(func)                          CMD_FUNC,   0,         0,       0,    0,     1,      1,   0,      0,      (func)

unsigned int set_operation_mode(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_curves(char *msg, unsigned char *cmd, char *log_msg);
unsigned int set_zones(char *msg, unsigned char *cmd, char *log_msg);

// Build the datagram of a main command from its payload. Returns the length to send, 0 when the payload is refused.
unsigned int heatpump_command(const cmdStruct *command, char *msg, unsigned char *cmd, char *log_msg);
void setInstallerCommandsAllowed(bool allowed);

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

const cmdStruct commands[] PROGMEM = {
  // set heatpump state to on by sending 1
  { "SetHeatpump", 0, CMD_ONOFF(4, 0) },
  // set pump state to on by sending 1
  { "SetPump", 0, CMD_ONOFF(4, 4) },
  // set max pump duty
  { "SetMaxPumpDuty", 0, CMD_VALUE(45, 1) },
  // set 0 for Off mode, set 1 for Quiet mode 1, set 2 for Quiet mode 2, set 3 for Quiet mode 3
  { "SetQuietMode", 0, CMD_FIELD(7, 3, 0x07, 1) },
  // z1 heat request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  { "SetZ1HeatRequestTemperature", 0, CMD_VALUE(38, 128) },
  // z1 cool request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  { "SetZ1CoolRequestTemperature", 0, CMD_VALUE(39, 128) },
  // z2 heat request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  { "SetZ2HeatRequestTemperature", 0, CMD_VALUE(40, 128) },
  // z2 cool request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  { "SetZ2CoolRequestTemperature", 0, CMD_VALUE(41, 128) },
  // set mode to force DHW by sending 1
  { "SetForceDHW", 0, CMD_ONOFF(4, 6) },
  // set mode to force defrost  by sending 1
  { "SetForceDefrost", 0, CMD_TRIGGER(8, 1) },
  // set mode to force sterilization by sending 1
  { "SetForceSterilization", 0, CMD_TRIGGER(8, 2) },
  // set sterilization target temperature in whole degrees Celsius (55-75)
  { "SetSterilizationTemp", 0, CMD_CHECKED(100, 0, 55, 75, 1, 1, 128) },
  // set sterilization holding time in 5-minute steps (5-60)
  { "SetSterilizationMaxTime", 0, CMD_CHECKED(101, 0, 5, 60, 5, 1, 1) },
  // set mode to force heater (emergency heating) by sending 1, off will be 0
  { "SetForceHeater", 0, CMD_ONOFF(5, 2) },
  // set Holiday mode by sending 1, off will be 0
  { "SetHolidayMode", 0, CMD_ONOFF(5, 4) },
  // set Powerful mode by sending 0 = off, 1 for 30min, 2 for 60min, 3 for 90 min
  { "SetPowerfulMode", 0, CMD_FIELD(7, 0, 0x07, 1) },
  // set Heat pump operation mode  3 = DHW only, 0 = heat only, 1 = cool only, 2 = Auto, 4 = Heat+DHW, 5 = Cool+DHW, 6 = Auto + DHW
  { "SetOperationMode", 0, CMD_FUNCTION(set_operation_mode) },
  // set DHW temperature by sending desired temperature between 40C-75C
  { "SetDHWTemp", 0, CMD_VALUE(42, 128) },
  // set heat/cool curves on z1 and z2 using a json input
  { "SetCurves", 0, CMD_FUNCTION(set_curves) },
  // set zones to active
  { "SetZones", 0, CMD_FUNCTION(set_zones) },
  { "SetFloorHeatDelta", 0, CMD_VALUE(84, 128) },
  { "SetFloorCoolDelta", 0, CMD_VALUE(94, 128) },
  { "SetDHWHeatDelta", 0, CMD_VALUE(99, 128) },
  { "SetReset", 0, CMD_TRIGGER(8, 0) },
  { "SetHeaterDelayTime", 0, CMD_VALUE(104, 1) },
  { "SetHeaterStartDelta", 0, CMD_VALUE(105, 128) },
  { "SetHeaterStopDelta", 0, CMD_VALUE(106, 128) },
  { "SetMainSchedule", 0, CMD_ONOFF(5, 6) },
  { "SetAltExternalSensor", 0, CMD_ONOFF(20, 4) },
  { "SetExternalPadHeater", 0, CMD_PAIR(25, 4, 2) },
  { "SetBufferDelta", 0, CMD_VALUE(59, 128) },
  { "SetBuffer", 0, CMD_ONOFF(24, 2) },
  // set Outdoor Temperature to stop heating 5-35
  { "SetHeatingOffOutdoorTemp", 0, CMD_VALUE(83, 128) },
  { "SetExternalControl", 0, CMD_ONOFF(23, 0) },
  { "SetExternalError", 0, CMD_ONOFF(23, 4) },
  { "SetExternalCompressorControl", 0, CMD_ONOFF(23, 6) },
  { "SetExternalHeatCoolControl", 0, CMD_ONOFF(23, 2) },
  // set Bivalent control mode  0 = off, 1 = on
  { "SetBivalentControl", 0, CMD_ONOFF(26, 0) },
  // set Bivalent operation mode  0 = Alternativ, 1 = Parallel, 2 = Advanced Parallel
  { "SetBivalentMode", 0, CMD_PAIR(26, 2, 2) },
  // bivalent start temp -  set from -15C to 35C 
  { "SetBivalentStartTemp", 0, CMD_VALUE(65, 128) },
  // bivalent AP start temp -  set from -15C to 35C 
  { "SetBivalentAPStartTemp", 0, CMD_VALUE(66, 128) },
  // bivalent AP stop temp -  set from -15C to 35C
  { "SetBivalentAPStopTemp", 0, CMD_VALUE(68, 128) },
  { "SetHeatingControl", 0, CMD_ONOFF(30, 2) },
  { "SetSmartDHW", 0, CMD_ONOFF(24, 6) },
  { "SetQuietModePriority", 0, CMD_ONOFF(11, 4) },
  { "SetPumpFlowrateMode", 0, CMD_ONOFF(29, 4) },
  { "SetDHWSensorSelection", 0, CMD_ONOFF(11, 0) },
  { "SetDHWHeaterState", 0, CMD_ONOFF(9, 2) },
  { "SetRoomHeaterState", 0, CMD_ONOFF(9, 0) },
  { "SetHeaterOnOutdoorTemp", 0, CMD_VALUE(85, 128) },
  { "SetHeatToCoolTemp", 0, CMD_CHECKED(95, 0, -127, 127, 1, 1, 128) },
  { "SetCoolToHeatTemp", 0, CMD_CHECKED(96, 0, -127, 127, 1, 1, 128) },
  // minutes in steps of 30
  { "SetRoomOperationMaxTime", 0, CMD_CHECKED(97, 0, 30, 7620, 30, 30, 1) },
  { "SetDHWHeatupMaxTime", 0, CMD_CHECKED(98, 0, 1, 254, 1, 1, 1) },
  { "SetQuietModeSchedule", 0, CMD_CHECKED(7, 6, 0, 1, 1, 1, 1) },
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
