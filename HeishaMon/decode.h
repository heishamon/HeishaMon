#include <ArduinoJson.h>
#include <PubSubClient.h>

#define MQTT_RETAIN_VALUES 1

void resetlastalldatatime();
void websocket_write_all(char *data, uint16_t data_len);


String getDataValue(char* data, unsigned int Topic_Number);
String getDataValueExtra(char* data, unsigned int Topic_Number);
String getOptDataValue(char* data, unsigned int Topic_Number);
void decode_heatpump_data(char* data, char* actData, PubSubClient &mqtt_client, void (*log_message)(char*), char* mqtt_topic_base, unsigned int updateAllTime);
void decode_heatpump_data_extra(char* data, char* actDataExtra, PubSubClient &mqtt_client, void (*log_message)(char*), char* mqtt_topic_base, unsigned int updateAllTime);
void decode_optional_heatpump_data(char* data, char* actOptDat, PubSubClient &mqtt_client, void (*log_message)(char*), char* mqtt_topic_base, unsigned int updateAllTime);

String getRawByte(byte input);
String getHeaterCapacity(byte input);
String getIntMinus1Times30(byte input);
String getZone2Active(byte input);
String getZone1Active(byte input);
String unknown(byte input);
String getBit1(byte input);
String getBit1and2(byte input);
String getBit3and4(byte input);
String getBit5and6(byte input);
String getBit7and8(byte input);
String getBit3and4and5(byte input);
String getLeft5bits(byte input);
String getRight3bits(byte input);
String getIntMinus1(byte input);
String getIntMinus128(byte input);
String getIntMinus1Div5(byte input);
String getIntMinus1Div50(byte input);
String getIntMinus1Times10(byte input);
String getIntMinus1Times50(byte input);
String getValvePID(byte input);
String getOpMode(byte input);
String getPower(byte input);
String getHeatMode(byte input);
String getModel(byte input);
String getFirstByte(byte input);
String getSecondByte(byte input);
String getUintt16(char * data, byte input);

static const char _unknown[] PROGMEM = "unknown";



#define NUMBER_OF_TOPICS 170 //last topic number + 1
#define NUMBER_OF_TOPICS_EXTRA 6 //last topic number + 1
#define NUMBER_OF_OPT_TOPICS 7 //last topic number + 1
#define MAX_TOPIC_LEN 42 // max length + 1

static const char optTopics[][20] PROGMEM = {
  "Z1_Water_Pump", // OPT0
  "Z1_Mixing_Valve", // OPT1
  "Z2_Water_Pump", // OPT2
  "Z2_Mixing_Valve", // OPT3
  "Pool_Water_Pump", // OPT4
  "Solar_Water_Pump", // OPT5
  "Alarm_State", // OPT6
};

static const char xtopics[][MAX_TOPIC_LEN] PROGMEM = {
  "Heat_Power_Consumption_Extra", //XTOP0
  "Cool_Power_Consumption_Extra", //XTOP1
  "DHW_Power_Consumption_Extra", //XTOP2
  "Heat_Power_Production_Extra",  //XTOP3
  "Cool_Power_Production_Extra",  //XTOP4
  "DHW_Power_Production_Extra",  //XTOP5
};

static const byte xtopicBytes[] PROGMEM = { //can store the index as byte (8-bit unsigned humber) as there aren't more then 255 bytes (actually only 203 bytes) to decode
  14,      //XTOP0
  16,      //XTOP1
  18,      //XTOP2
  20,      //XTOP3
  22,      //XTOP4
  24,      //XTOP5
};

static const char topics[][MAX_TOPIC_LEN] PROGMEM = {
  "Heatpump_State",          //TOP0
  "Pump_Flow",               //TOP1
  "Force_DHW_State",         //TOP2
  "Quiet_Mode_Schedule",     //TOP3
  "Operating_Mode_State",    //TOP4
  "Main_Inlet_Temp",         //TOP5
  "Main_Outlet_Temp",        //TOP6
  "Main_Target_Temp",        //TOP7
  "Compressor_Freq",         //TOP8
  "DHW_Target_Temp",         //TOP9
  "DHW_Temp",                //TOP10
  "Operations_Hours",        //TOP11
  "Operations_Counter",      //TOP12
  "Main_Schedule_State",     //TOP13
  "Outside_Temp",            //TOP14
  "Heat_Power_Production",  //TOP15
  "Heat_Power_Consumption", //TOP16
  "Powerful_Mode_Time",      //TOP17
  "Quiet_Mode_Level",        //TOP18
  "Holiday_Mode_State",      //TOP19
  "ThreeWay_Valve_State",    //TOP20
  "Outside_Pipe_Temp",       //TOP21
  "DHW_Heat_Delta",          //TOP22
  "Heat_Delta",              //TOP23
  "Cool_Delta",              //TOP24
  "DHW_Holiday_Shift_Temp",  //TOP25
  "Defrosting_State",        //TOP26
  "Z1_Heat_Request_Temp",    //TOP27
  "Z1_Cool_Request_Temp",    //TOP28
  "Z1_Heat_Curve_Target_High_Temp",      //TOP29
  "Z1_Heat_Curve_Target_Low_Temp",       //TOP30
  "Z1_Heat_Curve_Outside_High_Temp",     //TOP31
  "Z1_Heat_Curve_Outside_Low_Temp",      //TOP32
  "Room_Thermostat_Temp",    //TOP33
  "Z2_Heat_Request_Temp",    //TOP34
  "Z2_Cool_Request_Temp",    //TOP35
  "Z1_Water_Temp",           //TOP36
  "Z2_Water_Temp",           //TOP37
  "Cool_Power_Production",  //TOP38
  "Cool_Power_Consumption", //TOP39
  "DHW_Power_Production",   //TOP40
  "DHW_Power_Consumption",  //TOP41
  "Z1_Water_Target_Temp",    //TOP42
  "Z2_Water_Target_Temp",    //TOP43
  "Error",                   //TOP44
  "Room_Holiday_Shift_Temp", //TOP45
  "Buffer_Temp",             //TOP46
  "Solar_Temp",              //TOP47
  "Pool_Temp",               //TOP48
  "Main_Hex_Outlet_Temp",    //TOP49
  "Discharge_Temp",          //TOP50
  "Inside_Pipe_Temp",        //TOP51
  "Defrost_Temp",            //TOP52
  "Eva_Outlet_Temp",         //TOP53
  "Bypass_Outlet_Temp",      //TOP54
  "Ipm_Temp",                //TOP55
  "Z1_Temp",                 //TOP56
  "Z2_Temp",                 //TOP57
  "DHW_Heater_State",        //TOP58
  "Room_Heater_State",       //TOP59
  "Internal_Heater_State",   //TOP60
  "External_Heater_State",   //TOP61
  "Fan1_Motor_Speed",        //TOP62
  "Fan2_Motor_Speed",        //TOP63
  "High_Pressure",           //TOP64
  "Pump_Speed",              //TOP65
  "Low_Pressure",            //TOP66
  "Compressor_Current",      //TOP67
  "Force_Heater_State",      //TOP68
  "Sterilization_State",     //TOP69
  "Sterilization_Temp",      //TOP70
  "Sterilization_Max_Time",  //TOP71
  "Z1_Cool_Curve_Target_High_Temp",      //TOP72
  "Z1_Cool_Curve_Target_Low_Temp",       //TOP73
  "Z1_Cool_Curve_Outside_High_Temp",     //TOP74
  "Z1_Cool_Curve_Outside_Low_Temp",      //TOP75
  "Heating_Mode",            //TOP76
  "Heating_Off_Outdoor_Temp",//TOP77
  "Heater_On_Outdoor_Temp",  //TOP78
  "Heat_To_Cool_Temp",       //TOP79
  "Cool_To_Heat_Temp",       //TOP80
  "Cooling_Mode",            //TOP81
  "Z2_Heat_Curve_Target_High_Temp",      //TOP82
  "Z2_Heat_Curve_Target_Low_Temp",       //TOP83
  "Z2_Heat_Curve_Outside_High_Temp",     //TOP84
  "Z2_Heat_Curve_Outside_Low_Temp",      //TOP85
  "Z2_Cool_Curve_Target_High_Temp",      //TOP86
  "Z2_Cool_Curve_Target_Low_Temp",       //TOP87
  "Z2_Cool_Curve_Outside_High_Temp",     //TOP88
  "Z2_Cool_Curve_Outside_Low_Temp",      //TOP89
  "Room_Heater_Operations_Hours",        //TOP90
  "DHW_Heater_Operations_Hours",         //TOP91
  "Heat_Pump_Model",         //TOP92
  "Pump_Duty",               //TOP93
  "Zones_State",             //TOP94
  "Max_Pump_Duty",           //TOP95
  "Heater_Delay_Time",       //TOP96
  "Heater_Start_Delta",      //TOP97
  "Heater_Stop_Delta",       //TOP98
  "Buffer_Installed",        //TOP99
  "DHW_Installed",           //TOP100
  "Solar_Mode",              //TOP101
  "Solar_On_Delta",          //TOP102
  "Solar_Off_Delta",         //TOP103
  "Solar_Frost_Protection",  //TOP104
  "Solar_High_Limit",        //TOP105
  "Pump_Flowrate_Mode",      //TOP106
  "Liquid_Type",             //TOP107
  "Alt_External_Sensor",     //TOP108
  "Anti_Freeze_Mode",        //TOP109
  "Optional_PCB",            //TOP110
  "Z1_Sensor_Settings",      //TOP111
  "Z2_Sensor_Settings",      //TOP112
  "Buffer_Tank_Delta",       //TOP113
  "External_Pad_Heater",     //TOP114
  "Water_Pressure",          //TOP115
  "Second_Inlet_Temp",       //TOP116
  "Economizer_Outlet_Temp",  //TOP117
  "Second_Room_Thermostat_Temp",//TOP118
  "External_Control",        //TOP119
  "External_Heat_Cool_Control", //TOP120
  "External_Error_Signal",   //TOP121
  "External_Compressor_Control", //TOP122
  "Z2_Pump_State",           //TOP123
  "Z1_Pump_State",           //TOP124
  "TwoWay_Valve_State",      //TOP125
  "ThreeWay_Valve_State2",   //TOP126
  "Z1_Valve_PID",            //TOP127
  "Z2_Valve_PID",            //TOP128
  "Bivalent_Control",        //TOP129
  "Bivalent_Mode",           //TOP130
  "Bivalent_Start_Temp",     //TOP131
  "Bivalent_Advanced_Heat",  //TOP132
  "Bivalent_Advanced_DHW",   //TOP133
  "Bivalent_Advanced_Start_Temp",//TOP134
  "Bivalent_Advanced_Stop_Temp",//TOP135
  "Bivalent_Advanced_Start_Delay",//TOP136
  "Bivalent_Advanced_Stop_Delay",//TOP137
  "Bivalent_Advanced_DHW_Delay",//TOP138
  "Heating_Control",         //TOP139
  "Smart_DHW",               //TOP140
  "Quiet_Mode_Priority",     //TOP141
  "Expansion_Valve",         //TOP142
  "DHW_Sensor_Selection",    //TOP143
  "Dry_Concrete_State", //TOP144
  "DHW_Capacity_Mode", //TOP145
  "Zone_Configuration", //TOP146
  "Internal_Heater_Capacity", //TOP147
  "DHW_Heater_Type", //TOP148
  "SG_Ready_Control", //TOP149
  "Demand_Control", //TOP150
  "DHW_Defrost", //TOP151
  "Dry_Concrete_Target_Temp", //TOP152
  "Pool_Delta", //TOP153
  "SG_Ready_1_Heat_Capacity", //TOP154
  "SG_Ready_1_DHW_Capacity", //TOP155
  "SG_Ready_2_Heat_Capacity", //TOP156
  "SG_Ready_2_DHW_Capacity", //TOP157
  "Room_Operation_Max_Time", //TOP158
  "DHW_Heatup_Max_Time", //TOP159
  "Quiet_Mode_Actual_State", //TOP160
  "Powerful_Mode_Actual_State", //TOP161
  "Heat_Cool_Switch_State", //TOP162
  "External_Switch_State", //TOP163
  "Bivalent_Boiler_Contact_State", //TOP164
  "Z2_Active", //TOP165
  "Z1_Active", //TOP166
  "Backup_Heater_Enable", //TOP167
  "Cooling_Enable", //TOP168
  "Heat_Request_Temp_With_Buffer", //TOP169
};

static const byte topicBytes[] PROGMEM = { //can store the index as byte (8-bit unsigned humber) as there aren't more then 255 bytes (actually only 203 bytes) to decode
  4,      //TOP0
  0,      //TOP1
  4,      //TOP2
  7,      //TOP3
  6,      //TOP4
  143,    //TOP5
  144,    //TOP6
  153,    //TOP7
  166,    //TOP8
  42,     //TOP9
  141,    //TOP10
  0,      //TOP11
  0,      //TOP12
  5,      //TOP13
  142,    //TOP14
  194,    //TOP15
  193,    //TOP16
  7,      //TOP17
  7,      //TOP18
  5,      //TOP19
  111,    //TOP20
  158,    //TOP21
  99,     //TOP22
  84,     //TOP23
  94,     //TOP24
  44,     //TOP25
  111,    //TOP26
  38,     //TOP27
  39,     //TOP28
  75,     //TOP29
  76,     //TOP30
  78,     //TOP31
  77,     //TOP32
  156,    //TOP33
  40,     //TOP34
  41,     //TOP35
  145,    //TOP36
  146,    //TOP37
  196,    //TOP38
  195,    //TOP39
  198,    //TOP40
  197,    //TOP41
  147,    //TOP42
  148,    //TOP43
  0,      //TOP44
  43,     //TOP45
  149,    //TOP46
  150,    //TOP47
  151,    //TOP48
  154,    //TOP49
  155,    //TOP50
  157,    //TOP51
  159,    //TOP52
  160,    //TOP53
  161,    //TOP54
  162,    //TOP55
  139,    //TOP56
  140,    //TOP57
  9,      //TOP58
  9,      //TOP59
  112,    //TOP60
  112,    //TOP61
  173,    //TOP62
  174,    //TOP63
  163,    //TOP64
  171,    //TOP65
  164,    //TOP66
  165,    //TOP67
  5,      //TOP68
  117,    //TOP69
  100,    //TOP70
  101,    //TOP71
  86,     //TOP72
  87,     //TOP73
  89,     //TOP74
  88,     //TOP75
  28,     //TOP76
  83,     //TOP77
  85,     //TOP78
  95,     //TOP79
  96,     //TOP80
  28,     //TOP81
  79,     //TOP82
  80,     //TOP83
  82,     //TOP84
  81,     //TOP85
  90,     //TOP86
  91,     //TOP87
  93,     //TOP88
  92,     //TOP89
  0,      //TOP90
  0,      //TOP91
  0,      //TOP92
  172,    //TOP93
  6,      //TOP94
  45,     //TOP95
  104,    //TOP96
  105,    //TOP97
  106,    //TOP98
  24,     //TOP99
  24,     //TOP100
  24,     //TOP101
  61,     //TOP102
  62,     //TOP103
  63,     //TOP104
  64,     //TOP105
  29,     //TOP106
  20,     //TOP107
  20,     //TOP108
  20,     //TOP109
  20,     //TOP110
  22,     //TOP111
  22,     //TOP112
  59,     //TOP113
  25,     //TOP114
  125,    //TOP115
  126,    //TOP116
  127,    //TOP117
  128,    //TOP118
  23,     //TOP119
  23,     //TOP120
  23,     //TOP121
  23,     //TOP122
  116,    //TOP123
  116,    //TOP124
  116,    //TOP125
  116,    //TOP126
  177,    //TOP127
  178,    //TOP128
  26,    //TOP129
  26,    //TOP130
  65,    //TOP131
  26,    //TOP132
  26,    //TOP133
  66,    //TOP134
  68,    //TOP135
  67,    //TOP136
  69,    //TOP137
  70,    //TOP138
  30,    //TOP139
  24,    //TOP140
  11,    //TOP141
  175,   //TOP142
  11,    //TOP143
  5, //TOP144
  9, //TOP145
  21, //TOP146
  25, //TOP147
  25, //TOP148
  27, //TOP149
  27, //TOP150
  30, //TOP151
  46, //TOP152
  58, //TOP153
  71, //TOP154
  72, //TOP155
  73, //TOP156
  74, //TOP157
  97, //TOP158
  98, //TOP159
  110, //TOP160
  110, //TOP161
  110, //TOP162
  110, //TOP163
  112, //TOP164
  117, //TOP165
  117, //TOP166
  120, //TOP167
  120, //TOP168
  152, //TOP169
};


typedef String (*xtopicFP)(char*, byte);
static const xtopicFP xtopicFunctions[] PROGMEM = {
  getUintt16,         //XTOP0
  getUintt16,         //XTOP1
  getUintt16,         //XTOP2
  getUintt16,         //XTOP3
  getUintt16,         //XTOP4
  getUintt16,         //XTOP5
};

typedef String (*topicFP)(byte);
static const topicFP topicFunctions[] PROGMEM = {
  getBit7and8,         //TOP0
  unknown,             //TOP1
  getBit1and2,         //TOP2
  getBit1and2,         //TOP3
  getOpMode,           //TOP4
  getIntMinus128,      //TOP5
  getIntMinus128,      //TOP6
  getIntMinus128,      //TOP7
  getIntMinus1,        //TOP8
  getIntMinus128,      //TOP9
  getIntMinus128,      //TOP10
  unknown,             //TOP11
  unknown,             //TOP12
  getBit1and2,         //TOP13
  getIntMinus128,      //TOP14
  getPower,            //TOP15
  getPower,            //TOP16
  getRight3bits,       //TOP17
  getBit3and4and5,     //TOP18
  getBit3and4,         //TOP19
  getBit7and8,         //TOP20
  getIntMinus128,      //TOP21
  getIntMinus128,      //TOP22
  getIntMinus128,      //TOP23
  getIntMinus128,      //TOP24
  getIntMinus128,      //TOP25
  getBit5and6,         //TOP26
  getIntMinus128,      //TOP27
  getIntMinus128,      //TOP28
  getIntMinus128,      //TOP29
  getIntMinus128,      //TOP30
  getIntMinus128,      //TOP31
  getIntMinus128,      //TOP32
  getIntMinus128,      //TOP33
  getIntMinus128,      //TOP34
  getIntMinus128,      //TOP35
  getIntMinus128,      //TOP36
  getIntMinus128,      //TOP37
  getPower,            //TOP38
  getPower,            //TOP39
  getPower,            //TOP40
  getPower,            //TOP41
  getIntMinus128,      //TOP42
  getIntMinus128,      //TOP43
  unknown,             //TOP44
  getIntMinus128,      //TOP45
  getIntMinus128,      //TOP46
  getIntMinus128,      //TOP47
  getIntMinus128,      //TOP48
  getIntMinus128,      //TOP49
  getIntMinus128,      //TOP50
  getIntMinus128,      //TOP51
  getIntMinus128,      //TOP52
  getIntMinus128,      //TOP53
  getIntMinus128,      //TOP54
  getIntMinus128,      //TOP55
  getIntMinus128,      //TOP56
  getIntMinus128,      //TOP57
  getBit5and6,         //TOP58
  getBit7and8,         //TOP59
  getBit7and8,         //TOP60
  getBit5and6,         //TOP61
  getIntMinus1Times10, //TOP62
  getIntMinus1Times10, //TOP63
  getIntMinus1Div5,    //TOP64
  getIntMinus1Times50, //TOP65
  getIntMinus1Times50, //TOP66
  getIntMinus1Div5,    //TOP67
  getBit5and6,         //TOP68
  getBit5and6,         //TOP69
  getIntMinus128,      //TOP70
  getIntMinus1,        //TOP71
  getIntMinus128,      //TOP72
  getIntMinus128,      //TOP73
  getIntMinus128,      //TOP74
  getIntMinus128,      //TOP75
  getBit7and8,         //TOP76
  getIntMinus128,      //TOP77
  getIntMinus128,      //TOP78
  getIntMinus128,      //TOP79
  getIntMinus128,      //TOP80
  getBit5and6,         //TOP81
  getIntMinus128,      //TOP82
  getIntMinus128,      //TOP83
  getIntMinus128,      //TOP84
  getIntMinus128,      //TOP85
  getIntMinus128,      //TOP86
  getIntMinus128,      //TOP87
  getIntMinus128,      //TOP88
  getIntMinus128,      //TOP89
  unknown,             //TOP90
  unknown,             //TOP91
  unknown,			       //TOP92
  getIntMinus1,        //TOP93
  getBit1and2,         //TOP94
  getIntMinus1,        //TOP95
  getIntMinus1,        //TOP96
  getIntMinus128,      //TOP97
  getIntMinus128,      //TOP98
  getBit5and6,         //TOP99
  getBit7and8,         //TOP100
  getBit3and4,         //TOP101
  getIntMinus128,      //TOP102
  getIntMinus128,      //TOP103
  getIntMinus128,      //TOP104
  getIntMinus128,      //TOP105
  getBit3and4,         //TOP106
  getBit1,             //TOP107
  getBit3and4,         //TOP108
  getBit5and6,         //TOP109
  getBit7and8,         //TOP110  
  getSecondByte,       //TOP111
  getFirstByte,        //TOP112 
  getIntMinus128,      //TOP113
  getBit3and4,         //TOP114
  getIntMinus1Div50,   //TOP115
  getIntMinus128,      //TOP116
  getIntMinus128,      //TOP117
  getIntMinus128,      //TOP118
  getBit7and8,         //TOP119
  getBit5and6,         //TOP120
  getBit3and4,         //TOP121
  getBit1and2,         //TOP122
  getBit1and2,         //TOP123
  getBit3and4,         //TOP124
  getBit5and6,         //TOP125
  getBit7and8,         //TOP126
  getValvePID,        //TOP127
  getValvePID,        //TOP128
  getBit7and8,      //TOP129
  getBit5and6,      //TOP130
  getIntMinus128,      //TOP131
  getBit3and4,      //TOP132
  getBit1and2,      //TOP133
  getIntMinus128,      //TOP134
  getIntMinus128,      //TOP135
  getIntMinus1,      //TOP136
  getIntMinus1,      //TOP137
  getIntMinus1,      //TOP138
  getBit5and6,       //TOP139
  getBit1and2,       //TOP140
  getBit3and4,       //TOP141
  getIntMinus1,      //TOP142
  getBit7and8,       //TOP143
  getBit7and8, //TOP144
  getBit3and4, //TOP145
  getRawByte, //TOP146
  getHeaterCapacity, //TOP147
  getBit7and8, //TOP148
  getBit5and6, //TOP149
  getBit7and8, //TOP150
  getBit7and8, //TOP151
  getIntMinus128, //TOP152
  getIntMinus128, //TOP153
  getIntMinus1, //TOP154
  getIntMinus1, //TOP155
  getIntMinus1, //TOP156
  getIntMinus1, //TOP157
  getIntMinus1Times30, //TOP158
  getIntMinus1, //TOP159
  getBit1and2, //TOP160
  getBit3and4, //TOP161
  getBit5and6, //TOP162
  getBit7and8, //TOP163
  getBit1and2, //TOP164
  getZone2Active, //TOP165
  getZone1Active, //TOP166
  getBit3and4, //TOP167
  getBit5and6, //TOP168
  getIntMinus128, //TOP169
};

static const char *DisabledEnabled[] PROGMEM = {"2", "Disabled", "Enabled"};
static const char *BlockedFree[] PROGMEM = {"2", "Blocked", "Free"};
static const char *OffOn[] PROGMEM = {"2", "Off", "On"};
static const char *InactiveActive[] PROGMEM = {"2", "Inactive", "Active"};
static const char *PumpFlowRateMode[] PROGMEM = {"2", "DeltaT", "Max flow"};
static const char *HolidayState[] PROGMEM = {"3", "Off", "Scheduled", "Active"};
static const char *OpModeDesc[] PROGMEM = {"9", "Heat", "Cool", "Auto(heat)", "DHW", "Heat+DHW", "Cool+DHW", "Auto(heat)+DHW", "Auto(cool)", "Auto(cool)+DHW"};
static const char *Powerfulmode[] PROGMEM = {"4", "Off", "30min", "60min", "90min"};
static const char *Quietmode[] PROGMEM = {"4", "Off", "Level 1", "Level 2", "Level 3"};
static const char *Valve[] PROGMEM = {"2", "Room", "DHW"};
static const char *Valve2[] PROGMEM = {"2", "Cool", "Heat"};
static const char *MixingValve[] PROGMEM = {"4", "Off", "Decrease","Increase","Invalid"};
static const char *LitersPerMin[] PROGMEM = {"0", "l/min"};
static const char *RotationsPerMin[] PROGMEM = {"0", "r/min"};
static const char *Bar[] PROGMEM = {"0", "Bar"};
static const char *Pressure[] PROGMEM = {"0", "Kgf/cm2"};
static const char *Celsius[] PROGMEM = {"0", "°C"};
static const char *Kelvin[] PROGMEM = {"0", "K"};
static const char *Hertz[] PROGMEM = {"0", "Hz"};
static const char *Counter[] PROGMEM = {"0", "count"};
static const char *Hours[] PROGMEM = {"0", "hours"};
static const char *Watt[] PROGMEM = {"0", "Watt"};
static const char *ErrorState[] PROGMEM = {"0", "Error"};
static const char *Ampere[] PROGMEM = {"0", "Ampere"};
static const char *Minutes[] PROGMEM = {"0", "Minutes"};
static const char *Duty[] PROGMEM = {"0", "Duty"};
static const char *ZonesState[] PROGMEM = {"3", "Zone1 active", "Zone2 active", "Zone1 and zone2 active"};
static const char *HeatCoolModeDesc[] PROGMEM = {"2", "Comp. Curve", "Direct"};
static const char *SolarModeDesc[] PROGMEM = {"3", "Disabled", "Buffer", "DHW"};
static const char *ZonesSensorType[] PROGMEM = {"4", "Water Temperature", "External Thermostat", "Internal Thermostat", "Thermistor"};
static const char *LiquidType[] PROGMEM = {"2", "Water", "Glycol"};
static const char *ExtPadHeaterType[] PROGMEM = {"3", "Disabled", "Type-A","Type-B"};
static const char *Bivalent[] PROGMEM = {"3", "Alternative", "Parallel", "Advanced Parallel"};
static const char *Percent[] PROGMEM = {"0", "%"};
static const char *Model[] PROGMEM = {"0", "Model"};
static const char *HeatingControl[] PROGMEM = {"2", "Comfort", "Efficiency"};
static const char *SmartDHW[] PROGMEM = {"2", "Variable", "Standard"};
static const char *QuietModePriority[] PROGMEM = {"2", "Sound", "Capacity"};
static const char *DHWSensorType[] PROGMEM = {"2", "Top", "Center"};
static const char *DHWCapacityMode[] PROGMEM = {"2", "Standard", "Variable"};
static const char *DHWHeaterType[] PROGMEM = {"2", "Internal", "External"};
static const char *Kilowatt[] PROGMEM = {"0", "kW"};
static const char *HeatCoolSwitch[] PROGMEM = {"2", "Heat", "Cool"};
static const char *ClosedOpen[] PROGMEM = {"2", "Closed", "Open"};
static const char *ZoneConfiguration[] PROGMEM = {"0", "Configuration code"};
static const char *Steps[] PROGMEM = {"0", "Steps"};

static const char **opttopicDescription[] PROGMEM = {
  OffOn,          //OPT0
  MixingValve,    //OPT1
  OffOn,          //OPT2
  MixingValve,    //OPT3
  OffOn,          //OPT4
  OffOn,          //OPT5
  OffOn,          //OPT6
};

static const char **xtopicDescription[] PROGMEM = {
  Watt,           //XTOP0
  Watt,           //XTOP1
  Watt,           //XTOP2
  Watt,           //XTOP3
  Watt,           //XTOP4
  Watt,           //XTOP5
};

static const char **topicDescription[] PROGMEM = {
  OffOn,           //TOP0
  LitersPerMin,    //TOP1
  DisabledEnabled, //TOP2
  DisabledEnabled, //TOP3
  OpModeDesc,      //TOP4
  Celsius,         //TOP5
  Celsius,         //TOP6
  Celsius,         //TOP7
  Hertz,           //TOP8
  Celsius,         //TOP9
  Celsius,         //TOP10
  Hours,           //TOP11
  Counter,         //TOP12
  DisabledEnabled, //TOP13
  Celsius,         //TOP14
  Watt,            //TOP15
  Watt,            //TOP16
  Powerfulmode,    //TOP17
  Quietmode,       //TOP18
  HolidayState,    //TOP19
  Valve,           //TOP20
  Celsius,         //TOP21
  Kelvin,          //TOP22
  Kelvin,          //TOP23
  Kelvin,          //TOP24
  Kelvin,          //TOP25
  DisabledEnabled, //TOP26
  Celsius,         //TOP27
  Celsius,         //TOP28
  Celsius,         //TOP29
  Celsius,         //TOP30
  Celsius,         //TOP31
  Celsius,         //TOP32
  Celsius,         //TOP33
  Celsius,         //TOP34
  Celsius,         //TOP35
  Celsius,         //TOP36
  Celsius,         //TOP37
  Watt,            //TOP38
  Watt,            //TOP39
  Watt,            //TOP40
  Watt,            //TOP41
  Celsius,         //TOP42
  Celsius,         //TOP43
  ErrorState,      //TOP44
  Kelvin,          //TOP45
  Celsius,         //TOP46
  Celsius,         //TOP47
  Celsius,         //TOP48
  Celsius,         //TOP49
  Celsius,         //TOP50
  Celsius,         //TOP51
  Celsius,         //TOP52
  Celsius,         //TOP53
  Celsius,         //TOP54
  Celsius,         //TOP55
  Celsius,         //TOP56
  Celsius,         //TOP57
  BlockedFree,     //TOP58
  BlockedFree,     //TOP59
  InactiveActive,  //TOP60
  InactiveActive,  //TOP61
  RotationsPerMin, //TOP62
  RotationsPerMin, //TOP63
  Pressure,        //TOP64
  RotationsPerMin, //TOP65
  Pressure,        //TOP66
  Ampere,          //TOP67
  InactiveActive,  //TOP68
  InactiveActive,  //TOP69
  Celsius,         //TOP70
  Minutes,         //TOP71
  Celsius,         //TOP72
  Celsius,         //TOP73
  Celsius,         //TOP74
  Celsius,         //TOP75
  HeatCoolModeDesc,//TOP76
  Celsius,         //TOP77
  Celsius,         //TOP78
  Celsius,         //TOP79
  Celsius,         //TOP80
  HeatCoolModeDesc,//TOP81
  Celsius,         //TOP82
  Celsius,         //TOP83
  Celsius,         //TOP84
  Celsius,         //TOP85
  Celsius,         //TOP86
  Celsius,         //TOP87
  Celsius,         //TOP88
  Celsius,         //TOP89
  Hours,           //TOP90
  Hours,           //TOP91
  Model,           //TOP92
  Duty,            //TOP93
  ZonesState,      //TOP94
  Duty,            //TOP95
  Minutes,         //TOP96
  Kelvin,          //TOP97
  Kelvin,          //TOP98
  DisabledEnabled, //TOP99
  DisabledEnabled, //TOP100
  SolarModeDesc,   //TOP101
  Kelvin,          //TOP102
  Kelvin,          //TOP103
  Celsius,         //TOP104
  Celsius,         //TOP105
  PumpFlowRateMode,//TOP106
  LiquidType,      //TOP107
  DisabledEnabled, //TOP108
  DisabledEnabled, //TOP109
  DisabledEnabled, //TOP110
  ZonesSensorType, //TOP111
  ZonesSensorType, //TOP112
  Kelvin,          //TOP113
  ExtPadHeaterType,//TOP114
  Bar,             //TOP115
  Celsius,         //TOP116
  Celsius,         //TOP117
  Celsius,         //TOP118
  DisabledEnabled, //TOP119
  DisabledEnabled, //TOP120
  DisabledEnabled, //TOP121
  DisabledEnabled, //TOP122
  OffOn,           //TOP123
  OffOn,           //TOP124
  Valve2,          //TOP125
  Valve,           //TOP126
  Percent,         //TOP127
  Percent,         //TOP128
  DisabledEnabled, //TOP129
  Bivalent,        //TOP130
  Celsius,         //TOP131
  DisabledEnabled, //TOP132
  DisabledEnabled, //TOP133
  Celsius,         //TOP134
  Celsius,         //TOP135
  Minutes,         //TOP136
  Minutes,         //TOP137
  Minutes,         //TOP138
  HeatingControl,  //TOP139
  SmartDHW,        //TOP140
  QuietModePriority, //TOP141
  Steps,             //TOP142
  DHWSensorType,     //TOP143
  OffOn, //TOP144
  DHWCapacityMode, //TOP145
  ZoneConfiguration, //TOP146
  Kilowatt, //TOP147
  DHWHeaterType, //TOP148
  DisabledEnabled, //TOP149
  DisabledEnabled, //TOP150
  DisabledEnabled, //TOP151
  Celsius, //TOP152
  Kelvin, //TOP153
  Percent, //TOP154
  Percent, //TOP155
  Percent, //TOP156
  Percent, //TOP157
  Minutes, //TOP158
  Minutes, //TOP159
  OffOn, //TOP160
  OffOn, //TOP161
  HeatCoolSwitch, //TOP162
  ClosedOpen, //TOP163
  OffOn, //TOP164
  InactiveActive, //TOP165
  InactiveActive, //TOP166
  InactiveActive, //TOP167
  InactiveActive, //TOP168
  Celsius, //TOP169
};
