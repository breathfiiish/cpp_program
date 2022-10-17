/* * Copyright ? 2010-2020 BYD Corporation. All rights reserved.
*
* BYD Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from BYD Corporation is strictly prohibited.
*
*/
#include <android-base/properties.h>

#ifndef CONFIG_LIST_H
#define CONFIG_LIST_H

#define AUTO_HAL_VERSION     "v3.0"
#define AUTO_HAL_DEV         "dilink4.0_dev"
#define AUTO_HAL_REINFORCED  "not reinforced"

#define AUTO_DEVICE "/dev/spidev_ivi"
#define AUTO_INPUT_DEVICE "/proc/simulate_keys/key_code"
#define AUTO_GAMECAR_INPUT_DEVICE "/proc/virtual_game/key_code"
//#define AUTO_WAKEUP_MCU_NODE "/sys/soc_mcu/soc_wakeup_mcu"
#define AUTO_MOUSE_INPUT_DEVICE "/proc/simulate_mouse/mouse_code"

#define AUTO_SWVERSION_FIELD "apps.setting.product.inswver"
const bool isRSE =  android::base::GetProperty(AUTO_SWVERSION_FIELD, "").find("RSE") != std::string::npos ? true : false;
const bool isFSE =  android::base::GetProperty(AUTO_SWVERSION_FIELD, "").find("FSE") != std::string::npos ? true : false;


#define PAD_HEAD 0xAC
#define MCU_HEAD 0x55

#define FEATURE_ID_CAN_PRO      0x99000158
#define CAN_PRO_CAN             0
#define CAN_PRO_CANFD           1
#define CAN_PRO_TOYOTA          2
#define PROPERTY_KEY_INIT       "sys.car.protocol"
#define PROPERTY_KEY_RECORD     "persist.sys.protocol.record"
#define PROPERTY_CANFD          "CANFD"
#define PROPERTY_CAN            "CAN"
#define PROPERTY_TOYOTA         "TOYOTA"
#define PROPERTY_DEFAULT        "never_set"


/*
 * |<----add by spi ---->|<----          encode by autohal          ---->|<--spi--->|
 * +--------+------------+------+-----------+---------+------------------+----------+
 * | head*1 | totallen*1 | ID*4 | DATALEN*1 | VALUE*? | UNIT FRAME2,3... | parity*1 |
 * +--------+------------+------+-----------+---------+------------------+----------+
 *                       |<----  unit frame1 msg ---->|
 */
#define MSG_UNIT_LEN_MAX            0xFC // 252 0xFF-head-totallen-parity...
#define MSG_UNIT_LEN_ID             4 // sizeof(int)
#define MSG_UNIT_LEN_DL             1 // sizeof(char)
#define MSG_UNIT_LEN_VALUE_INT      4 // sizeof(int)
#define MSG_UNIT_LEN_VALUE_CHAR     1 // sizeof(char)
#define MSG_UNIT_LEN_BASIC          5 // MSG_UNIT_LEN_ID+MSG_UNIT_LEN_DL
#define MSG_UNIT_LEN_BASIC_INT      9 // MSG_UNIT_LEN_BASIC+MSG_UNIT_LEN_VALUE_INT
#define MSG_UNIT_LEN_BASIC_CHAR     6 // MSG_UNIT_LEN_BASIC+MSG_UNIT_LEN_VALUE_CHAR
#define MSG_UNIT_LEN_MIN            MSG_UNIT_LEN_BASIC
#define MSG_UNIT_LEN_MAX_STRING     0xF7 // 247 MSG_UNIT_LEN_MAX-MSG_UNIT_LEN_BASIC
#define MSG_UNIT_LEN_ADD_CAN        12 // (id4+len1+canid4+subid1+route1+mode1)
#define MSG_UNIT_LEN_QUERY_CAN      11 // (id4+len1+canid4+subid1+route1)
#define MSG_MAXSIZE_QUERY_ARRAY     50 // MSG_UNIT_LEN_MAX/MSG_UNIT_LEN_BASIC
#define MSG_MAXSIZE_QUERY_CAN       22 // MSG_UNIT_LEN_MAX/MSG_UNIT_LEN_QUERY_CAN
#define MSG_MAXSIZE_ADD_CAN         20 // MSG_UNIT_LEN_MAX/MSG_UNIT_LEN_ADD_CAN
#define MSG_MAX_LEN_ACC_CAN         (MSG_UNIT_LEN_ADD_CAN*MSG_MAXSIZE_ADD_CAN) // 0xF0
#define MSG_MAXSIZE_REQUEST_ARRAY   28 // MSG_UNIT_LEN_MAX/MSG_UNIT_LEN_BASIC_INT
#define READ_BUFFER_SIZE            260

#define ID_PRIORITY_HIGH    0
#define ID_PRIORITY_NORMAL  1
#define ID_PRIORITY_LOW     2
#define ID_PRIORITY_ALL     3
#define ID_PRIORITY_CUSTOM  9

#define DATA_TYPE_CHAR      0x0
#define DATA_TYPE_INT       0x1
#define DATA_TYPE_IALWAYS   0x2
#define DATA_TYPE_ITUPLE    0x3
#define DATA_TYPE_FLOAT     0x4
#define DATA_TYPE_FALWAYS   0x5
#define DATA_TYPE_FTUPLE    0x6
#define DATA_TYPE_STRING    0x7
#define DATA_TYPE_CAN       0x8
#define DATA_TYPE_CUSTOM    0x9

#define MAP_TYPE_UNDEFINED  0
#define MAP_TYPE_REQ        1
#define MAP_TYPE_QUR        2
#define MAP_TYPE_REQ_QUR    3

#define LOG_LEVEL_ERROR     0
#define LOG_LEVEL_WARNING   1
#define LOG_LEVEL_DEBUG     2

#define FUEL  0
#define EV    1
#define HEV   2

#define REPS 1
#define CEPS 2

#define DM_PLAT1 1
#define DM_PLAT2 2

#define LFPLAT_L1 1

#define CANID_TABLE_ADD     0xAA00001B
#define CANID_TABLE_REMOVE  0xAA00001C
#define CANID_TABLE_CLEAR   0xAA00001D
#define CANID_TABLE_QUERY   0xAA00001F

// app setTable id
#define CANID_SET_OTA       0xAA000020
#define CANID_SET_GB        0xAA000021
#define CANID_SET_BD        0xAA000022
#define CANID_SET_CS        0xAA000023
#define CANID_SET_MQTT      0xAA000165

// callback app id
#define CANID_UPDATE_OTA    0x9900001E
#define CANID_UPDATE_GB     0x9900001F
#define CANID_UPDATE_BD     0x99000020
#define CANID_UPDATE_CS     0x99000021
#define CANID_UPDATE_MQTT    0x99000165

// canid route
#define CANID_ROUTE_COM2       0
#define CANID_ROUTE_ESC        1
#define CANID_ROUTE_EX         2
#define CANID_ROUTE_RESERVED   3
#define CANID_ROUTE_COM1       4
#define CANID_ROUTE_ECM        5

/*  域控实车网络     对应spi can_num
 *  智能进入网       can0
 *  ESC              can1
 *  能量网           can2
 *
 *  非域控实车网络   对应spi can_num
 *  舒适网2          can0
 *  底盘网           can1
 *  动力网           can2
*/

#define CANID_OWNER_NONE    0x0
#define CANID_OWNER_OTA     0x1
#define CANID_OWNER_GB      0x2
#define CANID_OWNER_BD      0x4 // big data
#define CANID_OWNER_CS      0x8 // cloud service
#define CANID_OWNER_MQTT    0x10 // mqtt
#define CANID_OWNER_ALL     0xFF

#define CANID_MODE_ALWAYS   0
#define CANID_MODE_CHANGED  1
#define CANID_MODE_CYCLED   2
#define CANID_MODE_CNGCYC   3

#define CANID_FLAG_KEEP      0
#define CANID_FLAG_ADD       1
#define CANID_FLAG_REMOVE    2
#define CANID_FLAG_CLEAR     3
#define CANID_FLAG_CLEAR_ALL 4
#define CANID_FLAG_QUERY     5

#define CANID_DATA_LENGTH   8 // canid4+subid1+route1+mode1+flag1

// common state.
#define NONE         0
#define UNDEFINED    0xFF
#define ON           1
#define OFF          0
#define OPEN         1
#define CLOSE        0
#define CONNECTED    1
#define DISCONNECTED 0

#define ONLINE_STATE_ONLINE  1
#define ONLINE_STATE_OFFLINE 0
#define ONLINE_STATE_INVALID -1

// hal error code.
#define STA_SUCCESS              0
#define STA_SUCCESS_SYNC         1
#define STA_NOTE_NONEED_CALLBACK 2     /* no necessary callback, like keycode,slope */
#define STA_NOTE_FUNCTION_NULL  -10000 /* no function registered */
#define STA_NOTE_NO_PAIRED_ID   -10001 /* no uplink id found by checking downlink id */
#define STA_ERR_FAIL            -10010 /* common error */
#define STA_ERR_ID_UNDEFINED    -10011 /* id is not in feature map */
#define STA_ERR_INVALID_PARAM   -10012 /* input param is invalid */
#define STA_ERR_INVALID_TYPE    -10013 /* type is not int, float or string */
#define STA_ERR_INVALID_VALUE   -10014 /* value converted by func is invalid */
#define STA_ERR_UNSUPPORT       -10015 /* reserved */
#define STA_ERR_UNSUPPORT_SIZE  -10016 /* size of array is too large */
#define STA_ERR_PARITY          -10017 /* check parity bit failed */
#define STA_ERR_BUSY            -10018 /* device is busy */
#define STA_ERR_NOMEM           -10019 /* new memory failed */
#define STA_ERR_NO_NECEDATA     -10020 /* no necessary data, like autocode, gearbox style */
#define STA_ERR_UNKNOWN         -10030 /* unknown error */

#define UNDEFINED_FEATURE_ID 0x0

// definet the set function return value.
#define ASYNC        0 // wait mcu response
#define SYNC         1 // don't wait the mcu return
#define PARAM_FAIL  -1 // mcu protocal not support

struct floattuple {
    int id1;
    int id2;
    float value1;
    float value2;
};

// keycode value.
#define KEY_CLICK                       0x01
#define INPUT_KEY_VOL_ADD               0x01
#define INPUT_KEY_VOL_SUB               0x04
#define INPUT_KEY_SEEK_BACKWARD_SP      0x07 // previous station
#define INPUT_KEY_SEEK_BACKWARD_LP      0x08
#define INPUT_KEY_SEEK_FORWARD_SP       0x0A // next station
#define INPUT_KEY_SEEK_FORWARD_LP       0x0B
#define INPUT_KEY_PHONE_SP              0x0C
#define INPUT_KEY_PHONE_LP              0x0D
#define INPUT_KEY_AUDIO_SP              0x0E
#define INPUT_KEY_AUDIO_LP              0x0F
#define INPUT_KEY_MODE_SP               0x10
#define INPUT_KEY_MODE_LP               0x11
#define INPUT_KEY_VOICE_TRIGGER_SP      0x13
#define INPUT_KEY_VOICE_TRIGGER_LP      0x14
#define INPUT_KEY_PANORAMA_SP           0x15
#define INPUT_KEY_PANORAMA_LP           0x16
#define INPUT_KEY_LEFT_SP               0x18
#define INPUT_KEY_LEFT_LP               0x19
#define INPUT_KEY_RIGHT_SP              0x1B
#define INPUT_KEY_RIGHT_LP              0x1C
#define INPUT_KEY_UP_SP                 0x1E
#define INPUT_KEY_UP_LP                 0x1F
#define INPUT_KEY_DOWN_SP               0x21
#define INPUT_KEY_DOWN_LP               0x22
#define INPUT_KEY_INSTRUMENT_RETURN_HAD 0x23
#define INPUT_KEY_MUTE                  0x24
#define INPUT_KEY_CUSTOM_SP             0x25
#define INPUT_KEY_CUSTOM_LP             0x26

#define INPUT_KEY_INSTRUMENT_ENTER      0x01
#define INPUT_KEY_INSTRUMENT_UP         0x01
#define INPUT_KEY_INSTRUMENT_DOWN       0x01
#define INPUT_KEY_SHIFT_PADDLE          0x2A
#define INPUT_KEY_CRUISE_CONTROL        0x2B
#define INPUT_KEY_INSTRUMENT_RETURN     0x01 // share with hungup the call

#define INPUT_KEY_MM_BOARD_POWER        0x01
#define INPUT_KEY_MM_BOARD_BACK         0x02
#define INPUT_KEY_MM_BOARD_HOME         0x03
#define INPUT_KEY_MM_BOARD_VOL_SUB      0x04
#define INPUT_KEY_MM_BOARD_VOL_ADD      0x05
#define INPUT_KEY_MM_BOARD_MUSIC        0x06
#define INPUT_KEY_MM_BOARD_FM           0x07
#define INPUT_KEY_MM_BOARD_VIDEO        0x08
#define INPUT_KEY_MM_BOARD_CALL         0x09
#define INPUT_KEY_MM_BOARD_SD           0x0A
#define INPUT_KEY_MM_BOARD_USB          0x0B
#define INPUT_KEY_MM_BOARD_SETTING      0x0C
#define INPUT_KEY_MM_BOARD_GPS          0x0D
#define INPUT_KEY_MM_BOARD_AUX          0x0E
#define INPUT_KEY_MM_BOARD_MUTE         0x0F
#define INPUT_KEY_MM_BOARD_TUN_ADD      0x10
#define INPUT_KEY_MM_BOARD_TUN_SUB      0x11
#define INPUT_KEY_MM_BOARD_SEEK_ADD     0x12
#define INPUT_KEY_MM_BOARD_SEEK_SUB     0x13
#define INPUT_KEY_MM_BOARD_SCREEN_OFF   0x14
#define INPUT_KEY_MM_BOARD_AUDIO        0x15
#define INPUT_KEY_MM_BOARD_RADAR        0x16
#define INPUT_KEY_MM_BOARD_APP_SWITCH   0x24
#define INPUT_KEY_MM_BOARD_CUSTOM_SP    0x2A
#define INPUT_KEY_MM_BOARD_CUSTOM_LP    0x2B

#define MODE_SP_DOWN            "891"
#define MODE_SP_UP              "890"

#define AUTO_MODE_REAR_DOWN       "931"
#define AUTO_MODE_REAR_UP       "930"

#define AUTO_MODE_RSE_L_DOWN   "941"
#define AUTO_MODE_RSE_L_UP     "940"


#define AUTO_MODE_RSE_R_DOWN   "951"
#define AUTO_MODE_RSE_R_UP     "950"


#define MODE_LP_DOWN            "901"
#define MODE_LP_UP              "900"
#define SAVE_LOG_DOWN           "911"
#define SAVE_LOG_UP             "910"
#define SCREEN_SAVER_DOWN       "921"
#define SCREEN_SAVER_UP         "920"
#define VOL_ADD_DOWN            "1151"
#define VOL_ADD_UP              "1150"
#define VOL_SUB_DOWN            "1141"
#define VOL_SUB_UP              "1140"
#define MUTE_DOWN               "1131"
#define MUTE_UP                 "1130"
#define PHONE_SP_DOWN           "2661" //answer call
#define PHONE_SP_UP             "2660"
#define INSTRUMENT_RETURN_DOWN  "2671" //hungup call and instrument back key
#define INSTRUMENT_RETURN_UP    "2670"
#define SEEK_BACKWARD_SP_DOWN   "2681" //previous station
#define SEEK_BACKWARD_SP_UP     "2680"
#define SEEK_BACKWARD_LP_DOWN   "2691"
#define SEEK_BACKWARD_LP_UP     "2690"
#define SEEK_FORWARD_SP_DOWN    "2701" //next station
#define SEEK_FORWARD_SP_UP      "2700"
#define SEEK_FORWARD_LP_DOWN    "2711"
#define SEEK_FORWARD_LP_UP      "2710"
#define VOICE_TRIGGER_SP_DOWN   "2901"
#define VOICE_TRIGGER_SP_UP     "2900"
#define PANORAMA_SP_DOWN        "2881"  //auto_video or auto_video_switch
#define PANORAMA_SP_UP          "2880"
#define CUSTOM_SP_DOWN          "3001"
#define CUSTOM_SP_UP            "3000"
#define CUSTOM_LP_DOWN          "3011"
#define CUSTOM_LP_UP            "3010"
#define BACK_DOWN               "2921"
#define BACK_UP                 "2920"
#define HOME_DOWN               "2911"
#define HOME_UP                 "2910"
#define APP_SWITCH_DOWN         "2931"
#define APP_SWITCH_UP           "2930"

#define VOICE_TRIGGER_LP_DOWN   "3121"
#define VOICE_TRIGGER_LP_UP     "3120"
#define PANORAMA_LP_DOWN        "4011"
#define PANORAMA_LP_UP          "4010"
#define PHONE_LP_DOWN           "4021"
#define PHONE_LP_UP             "4020"
#define AUDIO_SP_DOWN           "4031"
#define AUDIO_SP_UP             "4030"
#define AUDIO_LP_DOWN           "4041"
#define AUDIO_LP_UP             "4040"
#define LEFT_SP_DOWN            "3041" //"4061"
#define LEFT_SP_UP              "3040" //"4060"
#define LEFT_LP_DOWN            "4071"
#define LEFT_LP_UP              "4070"
#define RIGHT_SP_DOWN           "3051" //"4081"
#define RIGHT_SP_UP             "3050" //"4080"
#define RIGHT_LP_DOWN           "4091"
#define RIGHT_LP_UP             "4090"
#define UP_SP_DOWN              "3031" //"4101"
#define UP_SP_UP                "3030" //"4100"
#define UP_LP_DOWN              "4111"
#define UP_LP_UP                "4110"
#define DOWN_SP_DOWN            "3021" //"4121"
#define DOWN_SP_UP              "3020" //"4120"
#define DOWN_LP_DOWN            "4131"
#define DOWN_LP_UP              "4130"
#define INSTRUMENT_ENTER_DOWN   "4141"
#define INSTRUMENT_ENTER_UP     "4140"
#define INSTRUMENT_UP_DOWN      "4151"
#define INSTRUMENT_UP_UP        "4150"
#define INSTRUMENT_DOWN_DOWN    "4161"
#define INSTRUMENT_DOWN_UP      "4160"
#define SHIFT_PADDLE_DOWN       "4171"
#define SHIFT_PADDLE_UP         "4170"
#define CRUISE_CONTROL_DOWN     "4181"
#define CRUISE_CONTROL_UP       "4180"

#define AUTO_REAR_SCREEN_PLAY_DOWN      "3161"
#define AUTO_REAR_SCREEN_PLAY_UP        "3160"

#define AUTO_RSE_L_SCREEN_PLAY_DOWN  "4671"
#define AUTO_RSE_L_SCREEN_PLAY_UP  "4670"

#define AUTO_RSE_R_SCREEN_PLAY_DOWN    "4711"
#define AUTO_RSE_R_SCREEN_PLAY_UP      "4710"

#define AUTO_REAR_SCREEN_PAUSE_DOWN     "3131"
#define AUTO_REAR_SCREEN_PAUSE_UP       "3130"

#define AUTO_RSE_L_SCREEN_PAUSE_DOWN   "4641"
#define AUTO_RSE_L_SCREEN_PAUSE_UP     "4640"

#define AUTO_RSE_R_SCREEN_PAUSE_DOWN   "4681"
#define AUTO_RSE_R_SCREEN_PAUSE_UP     "4680"


#define AUTO_REAR_SCREEN_PREV_DOWN      "3141"
#define AUTO_REAR_SCREEN_PREV_UP        "3140"
#define AUTO_REAR_SCREEN_NEXT_DOWN      "3151"
#define AUTO_REAR_SCREEN_NEXT_UP        "3150"

#define AUTO_MODE_LAS_LRSE_DOWN   "4761"
#define AUTO_MODE_LAS_LRSE_UP     "4760"
#define AUTO_LAS_LRSE_PLAY_DOWN   "4721"
#define AUTO_LAS_LRSE_PLAY_UP     "4720"
#define AUTO_LAS_LRSE_PAUSE_DOWN  "4731"
#define AUTO_LAS_LRSE_PAUSE_UP    "4730"
#define AUTO_LAS_LRSE_PREV_DOWN   "4741"
#define AUTO_LAS_LRSE_PREV_UP     "4740"
#define AUTO_LAS_LRSE_NEXT_DOWN   "4751"
#define AUTO_LAS_LRSE_NEXT_UP     "4750"

#define AUTO_MODE_RAS_RRSE_DOWN   "4811"
#define AUTO_MODE_RAS_RRSE_UP     "4810"
#define AUTO_RAS_RRSE_PLAY_DOWN   "4771"
#define AUTO_RAS_RRSE_PLAY_UP     "4770"
#define AUTO_RAS_RRSE_PAUSE_DOWN  "4781"
#define AUTO_RAS_RRSE_PAUSE_UP    "4780"
#define AUTO_RAS_RRSE_PREV_DOWN   "4791"
#define AUTO_RAS_RRSE_PREV_UP     "4790"
#define AUTO_RAS_RRSE_NEXT_DOWN   "4801"
#define AUTO_RAS_RRSE_NEXT_UP     "4800"


#define AUTO_RSE_L_SCREEN_PREV_DOWN     "4651"
#define AUTO_RSE_L_SCREEN_PREV_UP       "4650"
#define AUTO_RSE_L_SCREEN_NEXT_DOWN     "4661"
#define AUTO_RSE_L_SCREEN_NEXT_UP       "4660"

#define AUTO_RSE_R_SCREEN_PREV_DOWN  "4691"
#define AUTO_RSE_R_SCREEN_PREV_UP    "4690"
#define AUTO_RSE_R_SCREEN_NEXT_DOWN  "4701"
#define AUTO_RSE_R_SCREEN_NEXT_UP    "4700"



#define GAMECAR_GAMEHOME_REQUESR_DOWN   "3171"
#define GAMECAR_GAMEHOME_REQUESR_UP     "3170"

//xiaorui add for game car demo
#define SET_CANCEL_CRUISE_BUTTON_DOWN    31
#define SET_CANCEL_CRUISE_BUTTON_UP      30

#define SET_RESET_PLUS_BUTTON_DOWN       131
#define SET_RESET_PLUS_BUTTON_UP         130

#define SET_SETTING_MINUS_BUTTON_DOWN    141
#define SET_SETTING_MINUS_BUTTON_UP      140

#define SET_PANORAMIC_IMAGE_BUTTON_DOWN  41
#define SET_PANORAMIC_IMAGE_BUTTON_UP    40

#define SET_VEHICLE_DISTANCE_MINUS_BUTTON_DOWN 91
#define SET_VEHICLE_DISTANCE_MINUS_BUTTON_UP   90

#define SET_VEHICLE_DISTANCE_PLUE_BUTTON_DOWN  71
#define SET_VEHICLE_DISTANCE_PLUE_BUTTON_UP    70

#define SET_CRUISE_BUTTON_DOWN           61
#define SET_CRUISE_BUTTON_UP             60

#define SET_ROTATING_BUTTON_DOWN         111
#define SET_ROTATING_BUTTON_UP           110

#define SET_RIGHT_BUTTON_DOWN            11
#define SET_RIGHT_BUTTON_UP              10

#define SET_SPEECH_RECOGNITION_BUTTON_DOWN    21
#define SET_SPEECH_RECOGNITION_BUTTON_UP      20

#define SET_ROLLER_MINUS_BUTTON_DOWN     151
#define SET_ROLLER_MINUS_BUTTON_UP       150

#define SET_ROLLER_PLUS_BUTTON_DOWN      161
#define SET_ROLLER_PLUS_BUTTON_UP        160

#define SET_MODE_BUTTON_DOWN             81
#define SET_MODE_BUTTON_UP               80

#define SET_BACK_BUTTON_DOWN             101
#define SET_BACK_BUTTON_UP               100

#define SET_PHONE_BUTTON_DOWN            121
#define SET_PHONE_BUTTON_UP              120

#define SET_LEFT_BUTTON_DOWN             51
#define SET_LEFT_BUTTON_UP               50

#define SET_MUTE_BUTTON_DOWN             171
#define SET_MUTE_BUTTON_UP               170

#define SPEED_BRAKE_S_DOWN               181

#define SPEED_ACCELERATOR_S_DOWN         191

#define SET_GAME_SWHEEL_ANGLE_DOWN       201

#define SET_LEFT_ROCKER_X_DIRECTION      210
#define SET_LEFT_ROCKER_Y_DIRECTION      2100
#define SET_RIGHT_ROCKER_X_DIRECTION     220
#define SET_RIGHT_ROCKER_Y_DIRECTION     2200



// feature define for api device.
typedef enum {
    AUTO_FEATURE_UNDEFINED       = 0,
    AUTO_FEATURE_AC              = 1000,
    AUTO_FEATURE_BODY            = 1001,
    AUTO_FEATURE_AUDIO           = 1002,
    AUTO_FEATURE_FM              = 1003,
    AUTO_FEATURE_LIGHTS          = 1004,
    AUTO_FEATURE_POWER           = 1005,
    AUTO_FEATURE_ENERGY          = 1006,
    AUTO_FEATURE_INSTRUMENT      = 1007,
    AUTO_FEATURE_PM25            = 1008,
    AUTO_FEATURE_CHARGING        = 1009,
    AUTO_FEATURE_SECURITY        = 1010,
    AUTO_FEATURE_GEARBOX         = 1011,
    AUTO_FEATURE_ENGINE          = 1012,
    AUTO_FEATURE_SPEED           = 1013,
    AUTO_FEATURE_STATISTICS      = 1014,
    AUTO_FEATURE_COLLISION       = 1015,
    AUTO_FEATURE_TYRE            = 1016,
    AUTO_FEATURE_LOCATION        = 1017,
    AUTO_FEATURE_VIDEO           = 1018,
    AUTO_FEATURE_AUX             = 1019,
    AUTO_FEATURE_MOTOR           = 1020,
    AUTO_FEATURE_RADIO           = 1021,
    AUTO_FEATURE_TEST            = 1022,
    AUTO_FEATURE_SETTING         = 1023,
    AUTO_FEATURE_TIME            = 1024,
    AUTO_FEATURE_RADAR           = 1025,
    AUTO_FEATURE_REMINDER        = 1026,
    AUTO_FEATURE_VERSION         = 1027,
    AUTO_FEATURE_FUNCNOTICE      = 1028,
    AUTO_FEATURE_PHONE           = 1029,
    AUTO_FEATURE_REF_TEMPERATURE = 1030,
    AUTO_FEATURE_PANORAMA        = 1031,
    AUTO_FEATURE_OTA             = 1032,
    AUTO_FEATURE_MQTT            = 1033,
    AUTO_FEATURE_YUNCTRL_DIV15   = 1034,
    AUTO_FEATURE_PROPERTY        = 1035,
    AUTO_FEATURE_QCFS            = 1036,
    AUTO_FEATURE_SIGNAL_4G       = 1037,
    AUTO_FEATURE_ADAS            = 1038,
    AUTO_FEATURE_GB              = 1039,
    AUTO_FEATURE_CALL            = 1040,
    AUTO_FEATURE_DOORLOCK        = 1041,
    AUTO_FEATURE_SAFETYBELT      = 1042,
    AUTO_FEATURE_SENSOR          = 1043,
    AUTO_FEATURE_MONITOR         = 1044,
    AUTO_FEATURE_DTC             = 1045,
    AUTO_FEATURE_WIPER           = 1046,
    AUTO_FEATURE_REAR_VIEWMIRROR = 1047,
    AUTO_FEATURE_VEHICLE_DATA    = 1048,
    AUTO_FEATURE_SPECIAL         = 1049,
    AUTO_FEATURE_INPUT           = 1060,
    AUTO_FEATURE_BIGDATA         = 1061,
    AUTO_FEATURE_RSE             = 1062,
    AUTO_FEATURE_MAX_END
} auto_feature_type;

using iFunType = int(*)(const unsigned int &, const int &, void*);
using fFunType = int(*)(const unsigned int &, const float &, void*);
using sFunType = int(*)(const unsigned int &, char*, char*);

#endif
