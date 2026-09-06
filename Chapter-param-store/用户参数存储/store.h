#ifndef __BSP_STORE_H
#define __BSP_STORE_H

#include "bsp_system.h"
#include "bsp_qspi_w25q64.h"
#include "car.h"
#include "imu.h"
#include "cmd.h"

#include <string.h>

#define PARAM_MAGIC_NUMBER           0x20260426

#define PARAM_SAVE_CAR_CTRL_DATA     0x0001
#define PARAM_SAVE_SYSTEM_DATA       0x0002
#define PARAM_SAVE_AVOIDANCE_DATA    0x0004
#define PARAM_SAVE_COMMAND_DATA      0x0008
#define PARAM_SAVE_INFO_DATA         0x0010
#define PARAM_SAVE_BLUETOOTH_DATA    0x0020
#define PARAM_SAVE_ESP8266_DATA      0x0040
#define PARAM_SAVE_GUI_DATA          0x0080
#define PARAM_SAVE_LOC_X_PID_DATA    0x0100
#define PARAM_SAVE_LOC_Y_PID_DATA    0x0200
#define PARAM_SAVE_YAW_PID_DATA      0x0400
#define PARAM_SAVE_MOTOR_PID_DATA    0x0800
#define PARAM_SAVE_SLIP_DET_DATA     0x1000
#define PARAM_SAVE_MOTOR_MAPPER_DATA 0x2000
#define PARAM_SAVE_ALL_DATA          0xFFFF /* 保存所有数据 */
#define PARAM_SAVE_STRUCTURE_DATA    0x0000 /* 保存结构体数据 */

#ifdef __cplusplus
extern "C" {
#endif

/* 存储结构体 */
typedef struct __ParamData_T
{
    /* 文件标识头 */
    uint32_t MagicNumber;

    /* 输出偏移和输入死区 */
    float car_InputDeadZone;

    /* 电池与蜂鸣器设置 */
    float system_BeepVoltage;
    int32_t system_VoltageCoff;
    uint8_t system_VoltageBeepFlag;
    uint8_t system_BeepEnableFlag;

    /* 底盘参数、车轮PID */
    float car_wheelBase;
    float car_MaxSpeedX;
    float car_MaxSpeedT;
    float car_wheelDiameter;
    PidParam_T car_pidLocationX;
    PidParam_T car_pidLocationY;
    PidParam_T car_pidYaw;
    PidParam_T car_pidMotor;
    ChassisType_T car_chassisType;
    uint32_t car_encoderPerRev;

    /* 电机-编码器映射 */
    CarMapper_T car_motorMapper[CAR_MAX_INDEX];
    CarMapper_T car_encoderMapper[CAR_MAX_INDEX];
    CarIndex_T car_useServoIndex;

    /* 外环控制参数 */
    uint8_t car_enableOuterLoop;                   /* 外环控制使能标志 */
    float car_motorIntSepThreshold[CAR_MAX_INDEX]; /* 积分分离阈值 */

    /* 串口信息广播 */
    uint8_t cmd_comBoardCast;

    /* 串口模式 */
    uint8_t cmd_hexStrictMode;
    uint8_t cmd_ReceiveMode;
    uint16_t cmd_boardCastDeltaTime[COM_COUNT];

    /* 外部设备选择 */
    uint8_t app_UseDevice;

    /* 串口硬件配置 */
    uint32_t uart_baudRate[COM_COUNT]; /* 波特率选项索引 */
    uint32_t uart_parity[COM_COUNT];   /* 奇偶校验 */
    uint32_t uart_stopBits[COM_COUNT]; /* 停止位 */

    /* 激光雷达与避障模式 */
    float lidar_frontBackAngleCheckThreshold;
    float lidar_sideAngleCheckThreshold;
    float avoidance_frontAngleRange;
    float avoidance_obstacleDistanceThreshold;
    float avoidance_ultraSonicThreshold;
    float avoidance_ctrlSpeedTGain;

    /* 打滑检测系数 */
    float car_slipK1;
    float car_slipK2;
    float car_slipTheta;

    /* 串口字符串数据接收模式 */
    uint8_t com_StringReceiveMode;

    /* 待机时间 */
    uint32_t pwr_standbyTime;

    /* 字符串参数 */
    char info_guiMainTitle[32];
    char info_guiMainWinProjectTitle[32];
    char info_guiMainWinProjectName[32];
    char info_guiMainWinExtraTitle[32];
    char info_guiMainWinExtraName[32];
    char info_guiMenuShowAnother[32];
    char info_guiMenuShowProject[32];
    char info_guiMenuShowVerision[32];
    char info_guiMenuShowOrganization1[32];
    char info_guiMenuShowOrganization2[32];
    char info_guiMenuShowExtraInfo[32];

    /* ESP8266参数 */
    char esp8266_wifiSsid[32];
    char esp8266_wifiPassword[32];
    char esp8266_protocol[5];
    char esp8266_serverIp[17];
    char esp8266_serverPort[6];
    char esp8266_serverLocalPort[6];
    char esp8266_clientId[32];
    char esp8266_mqttUser[32];
    char esp8266_mqttPass[32];

    /* 蓝牙参数 */
    char bluetooth_name[32];
    char bluetooth_pin[32];
    char bluetooth_role[9];

    /* GUI界面 */
    uint8_t gui_useNewUI;

    /* PCB版本信息 */
    char info_guiMenuShowPcbVersion[32];

    /* 电机控制模式 */
    uint8_t motor_controlMode;
    
    /* Lua代码程序自启动 */
    uint8_t lua_autoBootEnabled;
    char lua_autoBootPath[128];
} ParamData_T;

void Param_Init(void);
void Param_Save(uint16_t saveFlags);
void Param_LoadDefault(uint16_t saveFlags);
void Param_Load(char *filePath);
void Param_Export(char *filePath);
void Param_Clear(void);
void Param_LoadFromData(ParamData_T *data);
void Param_ExportToData(ParamData_T *data);

extern ParamData_T g_ParamData;
#ifdef __cplusplus
}
#endif

#endif
