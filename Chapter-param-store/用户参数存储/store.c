#include "store.h"

#include "bsp_memory.h"
#include "file.h"

ParamData_T g_ParamData = { 0 };

/**
  * @brief  将参数数据加载到全局变量
  */
static void Param_ReadToGlobal(void)
{
    g_Car.remote.wheelBase = g_ParamData.car_wheelBase;
    g_Car.remote.remoteMaxSpeedX = g_ParamData.car_MaxSpeedX;
    g_Car.remote.remoteMaxSpeedT = g_ParamData.car_MaxSpeedT;
    g_Car.remote.wheelDiameter = g_ParamData.car_wheelDiameter;
    g_Car.remote.pidLocationX.Kp = g_ParamData.car_pidLocationX.Kp;
    g_Car.remote.pidLocationX.Ki = g_ParamData.car_pidLocationX.Ki;
    g_Car.remote.pidLocationX.Kd = g_ParamData.car_pidLocationX.Kd;
    g_Car.remote.pidLocationX.OutputLimit = g_ParamData.car_pidLocationX.OutputLimit;
    g_Car.remote.pidLocationX.IntegralLimit = g_ParamData.car_pidLocationX.IntegralLimit;
    g_Car.remote.pidLocationY.Kp = g_ParamData.car_pidLocationY.Kp;
    g_Car.remote.pidLocationY.Ki = g_ParamData.car_pidLocationY.Ki;
    g_Car.remote.pidLocationY.Kd = g_ParamData.car_pidLocationY.Kd;
    g_Car.remote.pidLocationY.OutputLimit = g_ParamData.car_pidLocationY.OutputLimit;
    g_Car.remote.pidLocationY.IntegralLimit = g_ParamData.car_pidLocationY.IntegralLimit;
    g_Car.remote.pidYaw.Kp = g_ParamData.car_pidYaw.Kp;
    g_Car.remote.pidYaw.Ki = g_ParamData.car_pidYaw.Ki;
    g_Car.remote.pidYaw.Kd = g_ParamData.car_pidYaw.Kd;
    g_Car.remote.pidYaw.OutputLimit = g_ParamData.car_pidYaw.OutputLimit;
    g_Car.remote.pidYaw.IntegralLimit = g_ParamData.car_pidYaw.IntegralLimit;
    g_Car.remote.pidMotorParam.Kp = g_ParamData.car_pidMotor.Kp;
    g_Car.remote.pidMotorParam.Ki = g_ParamData.car_pidMotor.Ki;
    g_Car.remote.pidMotorParam.Kd = g_ParamData.car_pidMotor.Kd;
    g_Car.remote.pidMotorParam.OutputLimit = g_ParamData.car_pidMotor.OutputLimit;
    g_Car.remote.pidMotorParam.IntegralLimit = g_ParamData.car_pidMotor.IntegralLimit;
    g_Car.remote.encoderPerRev = g_ParamData.car_encoderPerRev;
    g_Car.remote.slipK1 = g_ParamData.car_slipK1;
    g_Car.remote.slipK2 = g_ParamData.car_slipK2;
    g_Car.remote.slipTheta = g_ParamData.car_slipTheta;
    g_CMD.comBoardCast = g_ParamData.cmd_comBoardCast;
    g_CMD.hexStrictMode = g_ParamData.cmd_hexStrictMode;
    g_CMD.receiveMode = g_ParamData.cmd_ReceiveMode;

    for (int comX = COM1; comX < COM_COUNT; comX++)
    {
        /* COM1/3/5 不支持设置 */
        if (comX == COM1 || comX == COM3 || comX == COM5) { continue; }

        g_CMD.comBoardCastDeltaTime[comX] = g_ParamData.cmd_boardCastDeltaTime[comX];
        

        /* 动态设置 */
        uint32_t bandrate = g_ParamData.uart_baudRate[comX];
        uint32_t parity = g_ParamData.uart_parity[comX];
        uint32_t stopBits = g_ParamData.uart_stopBits[comX];

        UART_SetBaudRate(comX, bandrate);
        UART_SetParity(comX, parity);
        UART_SetStopBits(comX, stopBits);
    }
    
    g_Car.avoidance.frontAngleRange = g_ParamData.avoidance_frontAngleRange;
    g_Car.avoidance.ultraSonicThreshold = g_ParamData.avoidance_ultraSonicThreshold;
    g_Car.avoidance.ctrlSpeedTGain = g_ParamData.avoidance_ctrlSpeedTGain;
    g_LidarState.frontBackAngleCheckThreshold = g_ParamData.lidar_frontBackAngleCheckThreshold;
    g_LidarState.sideAngleCheckThreshold = g_ParamData.lidar_sideAngleCheckThreshold;
}

/**
  * @brief  初始化存储模块
  */
void Param_Init(void)
{
    /* 读取数据 */
    if (W25QXX_ReadBuffer((uint8_t *)&g_ParamData, QSPI_ADDR_PARAM, sizeof(ParamData_T)) != W25QXX_OK)
    {
        DEBUG_PRINTF("SPI Flash读取失败\r\n");
        return;
    }

    /* 验证魔数 */
    if (g_ParamData.MagicNumber != PARAM_MAGIC_NUMBER)
    {
        DEBUG_PRINTF("警告：魔数不匹配，请详细检查设置\r\n");
        DEBUG_PRINTF("文件魔数=0x%08X，期望魔数=0x%08X\r\n", g_ParamData.MagicNumber, PARAM_MAGIC_NUMBER);
        Param_Clear(); /* 清理并创建新文件 */
    }

    Param_ReadToGlobal(); /* 将数据加载到全局变量 */
}

/**
  * @brief  保存数据到 SPI Flash
  * @param  saveFlags 保存标志位，指定需要保存的数据类别
  */
void Param_Save(uint16_t saveFlags)
{
    /* 写入配置 */
    g_ParamData.MagicNumber = PARAM_MAGIC_NUMBER;
    
    /* 保存X方向的PID数据 */
    if (saveFlags & PARAM_SAVE_LOC_X_PID_DATA)
    {
        g_ParamData.car_pidLocationX.Kp = g_Car.remote.pidLocationX.Kp;
        g_ParamData.car_pidLocationX.Ki = g_Car.remote.pidLocationX.Ki;
        g_ParamData.car_pidLocationX.Kd = g_Car.remote.pidLocationX.Kd;
        g_ParamData.car_pidLocationX.OutputLimit = g_Car.remote.pidLocationX.OutputLimit;
        g_ParamData.car_pidLocationX.IntegralLimit = g_Car.remote.pidLocationX.IntegralLimit;
    }
    
    /* 保存Y方向的PID数据 */
    if (saveFlags & PARAM_SAVE_LOC_Y_PID_DATA)
    {
        g_ParamData.car_pidLocationY.Kp = g_Car.remote.pidLocationY.Kp;
        g_ParamData.car_pidLocationY.Ki = g_Car.remote.pidLocationY.Ki;
        g_ParamData.car_pidLocationY.Kd = g_Car.remote.pidLocationY.Kd;
        g_ParamData.car_pidLocationY.OutputLimit = g_Car.remote.pidLocationY.OutputLimit;
        g_ParamData.car_pidLocationY.IntegralLimit = g_Car.remote.pidLocationY.IntegralLimit;
    }
    
    /* 保存Yaw方向的PID数据 */
    if (saveFlags & PARAM_SAVE_YAW_PID_DATA)
    {
        g_ParamData.car_pidYaw.Kp = g_Car.remote.pidYaw.Kp;
        g_ParamData.car_pidYaw.Ki = g_Car.remote.pidYaw.Ki;
        g_ParamData.car_pidYaw.Kd = g_Car.remote.pidYaw.Kd;
        g_ParamData.car_pidYaw.OutputLimit = g_Car.remote.pidYaw.OutputLimit;
        g_ParamData.car_pidYaw.IntegralLimit = g_Car.remote.pidYaw.IntegralLimit;
    }
    
    /* 保存电机速度的PID数据 */
    if (saveFlags & PARAM_SAVE_MOTOR_PID_DATA)
    {
        g_ParamData.car_pidMotor.Kp = g_Car.remote.pidMotorParam.Kp;
        g_ParamData.car_pidMotor.Ki = g_Car.remote.pidMotorParam.Ki;
        g_ParamData.car_pidMotor.Kd = g_Car.remote.pidMotorParam.Kd;
        g_ParamData.car_pidMotor.OutputLimit = g_Car.remote.pidMotorParam.OutputLimit;
        g_ParamData.car_pidMotor.IntegralLimit = g_Car.remote.pidMotorParam.IntegralLimit;
    }
    
    /* 保存电机映射数据 */
    if (saveFlags & PARAM_SAVE_MOTOR_MAPPER_DATA)
    {

    }
    
    /* 保存打滑检测数据 */
    if (saveFlags & PARAM_SAVE_SLIP_DET_DATA)
    {
        g_ParamData.car_slipK1 = g_Car.remote.slipK1;
        g_ParamData.car_slipK2 = g_Car.remote.slipK2;
        g_ParamData.car_slipTheta = g_Car.remote.slipTheta;
    }

    /* 保存遥控相关数据 */
    if (saveFlags & PARAM_SAVE_CAR_CTRL_DATA)
    {
        g_ParamData.car_wheelBase = g_Car.remote.wheelBase;
        g_ParamData.car_MaxSpeedX = g_Car.remote.remoteMaxSpeedX;
        g_ParamData.car_MaxSpeedT = g_Car.remote.remoteMaxSpeedT;
        g_ParamData.car_wheelDiameter = g_Car.remote.wheelDiameter;
        g_ParamData.car_encoderPerRev = g_Car.remote.encoderPerRev;
    }

    /* 保存串口相关数据 */
    if (saveFlags & PARAM_SAVE_COMMAND_DATA)
    {
        g_ParamData.cmd_comBoardCast = g_CMD.comBoardCast;
        g_ParamData.cmd_hexStrictMode = g_CMD.hexStrictMode;
        g_ParamData.cmd_ReceiveMode = g_CMD.receiveMode;
        for (int comX = COM1; comX < COM_COUNT; comX++)
        {
            /* COM1/3/5 不支持设置 */
            if (comX == COM1 || comX == COM3 || comX == COM5) { continue; }

            g_ParamData.cmd_boardCastDeltaTime[comX] = g_CMD.comBoardCastDeltaTime[comX];
        }
    }

    /* 保存避障相关数据 */
    if (saveFlags & PARAM_SAVE_AVOIDANCE_DATA)
    {
        g_ParamData.avoidance_frontAngleRange = g_Car.avoidance.frontAngleRange;
        g_ParamData.avoidance_ultraSonicThreshold = g_Car.avoidance.ultraSonicThreshold;
        g_ParamData.avoidance_ctrlSpeedTGain = g_Car.avoidance.ctrlSpeedTGain;
        g_ParamData.lidar_frontBackAngleCheckThreshold = g_LidarState.frontBackAngleCheckThreshold;
        g_ParamData.lidar_sideAngleCheckThreshold = g_LidarState.sideAngleCheckThreshold;
    }

    /* 写入数据 */
    if (W25QXX_WriteBuffer((uint8_t *)&g_ParamData, QSPI_ADDR_PARAM, sizeof(ParamData_T)) != W25QXX_OK)
    {
        /* 处理错误，例如输出日志 */
        DEBUG_PRINTF("SPI Flash保存失败\r\n");
        return;
    }
}

/**
  * @brief  从文件中读取参数数据
  * @param  filePath 文件路径
  */
void Param_Load(char *filePath)
{
    if (FatFS_ReadFile(filePath, (uint8_t *)&g_ParamData, sizeof(ParamData_T)) != sizeof(ParamData_T))
    {
        /* 处理错误，例如输出日志 */
        DEBUG_PRINTF("存储加载失败\r\n");
    }

    /* 写入数据 */
    Param_ReadToGlobal(); /* 将数据加载到全局变量 */
    Param_Save(PARAM_SAVE_ALL_DATA);
}

/**
  * @brief  导出参数数据到文件
  * @param  filePath 文件路径
  */
void Param_Export(char *filePath)
{
    if (FatFS_WriteFile(filePath, (uint8_t *)&g_ParamData, sizeof(ParamData_T)) != sizeof(ParamData_T))
    {
        /* 处理错误，例如输出日志 */
        DEBUG_PRINTF("存储导出失败\r\n");
    }
}

/**
  * @brief  恢复默认参数设置
  * @param  saveFlags 保存标志位，指定需要恢复的数据类别
  */
void Param_LoadDefault(uint16_t saveFlags)
{
    /* 设置标志位 */
    g_ParamData.MagicNumber = PARAM_MAGIC_NUMBER;

    /* 保存X方向的PID数据 */
    if (saveFlags & PARAM_SAVE_LOC_X_PID_DATA)
    {
        g_ParamData.car_pidLocationX.Kp = 2.5;
        g_ParamData.car_pidLocationX.Ki = 15.0;
        g_ParamData.car_pidLocationX.Kd = 2.5;
        g_ParamData.car_pidLocationX.OutputLimit = 1000.0;
        g_ParamData.car_pidLocationX.IntegralLimit = 3500.0;
    }
    
    /* 保存Y方向的PID数据 */
    if (saveFlags & PARAM_SAVE_LOC_Y_PID_DATA)
    {
        g_ParamData.car_pidLocationY.Kp = 25.0;
        g_ParamData.car_pidLocationY.Ki = 45.0;
        g_ParamData.car_pidLocationY.Kd = 0.0;
        g_ParamData.car_pidLocationY.OutputLimit = 985.0;
        g_ParamData.car_pidLocationY.IntegralLimit = 2000.0;
    }
    
    /* 保存Yaw方向的PID数据 */
    if (saveFlags & PARAM_SAVE_YAW_PID_DATA)
    {
        g_ParamData.car_pidYaw.Kp = 12.5;
        g_ParamData.car_pidYaw.Ki = 0.15;
        g_ParamData.car_pidYaw.Kd = 0.25;
        g_ParamData.car_pidYaw.OutputLimit = 360.0;
        g_ParamData.car_pidYaw.IntegralLimit = 360.0;
    }
    
    /* 保存电机速度的PID数据 */
    if (saveFlags & PARAM_SAVE_MOTOR_PID_DATA)
    {
        g_ParamData.car_pidMotor.Kp = 10.0;
        g_ParamData.car_pidMotor.Ki = 85.0;
        g_ParamData.car_pidMotor.Kd = 1.0;
        g_ParamData.car_pidMotor.OutputLimit = 980.0;
        g_ParamData.car_pidMotor.IntegralLimit = 2000.0;
    }
    
    /* 保存电机映射数据 */
    if (saveFlags & PARAM_SAVE_MOTOR_MAPPER_DATA)
    {
        g_ParamData.car_motorMapper[0] = 0;
        g_ParamData.car_motorMapper[1] = 0;
        g_ParamData.car_motorMapper[2] = 0;
        g_ParamData.car_motorMapper[3] = 0;
        g_ParamData.car_encoderMapper[0] = 0;
        g_ParamData.car_encoderMapper[1] = 0;
        g_ParamData.car_encoderMapper[2] = 0;
        g_ParamData.car_encoderMapper[3] = 0;
    }
    
    /* 保存打滑检测数据 */
    if (saveFlags & PARAM_SAVE_SLIP_DET_DATA)
    {
        g_ParamData.car_slipK1 = 0.0;
        g_ParamData.car_slipK2 = 0.0;
        g_ParamData.car_slipTheta = 0.0;
    }
    
    /* 设置小车默认数据 */
    if (saveFlags & PARAM_SAVE_CAR_CTRL_DATA)
    {
        g_ParamData.car_InputDeadZone = 1.0;
        g_ParamData.car_wheelBase = 16.0;
        g_ParamData.car_MaxSpeedX = 0.3;
        g_ParamData.car_MaxSpeedT = 60.0;
        g_ParamData.car_wheelDiameter = 6.5;
        g_ParamData.car_chassisType = 1;
        g_ParamData.car_encoderPerRev = 1250;
        g_ParamData.car_useServoIndex = 0;
        g_ParamData.car_enableOuterLoop = 200;
    }

    /* 设置串口默认数据 */
    if (saveFlags & PARAM_SAVE_COMMAND_DATA)
    {
        g_ParamData.cmd_comBoardCast = 0;
        g_ParamData.cmd_hexStrictMode = 0;
        g_ParamData.cmd_ReceiveMode = 0xFF;
        g_ParamData.uart_baudRate[COM2] = 115200;
        g_ParamData.uart_baudRate[COM3] = 115200;
        g_ParamData.uart_baudRate[COM5] = 115200;
        g_ParamData.uart_baudRate[COM6] = 115200;
        g_ParamData.uart_baudRate[COM7] = 115200;
        g_ParamData.uart_parity[COM2] = USART_PARITY_NONE;
        g_ParamData.uart_parity[COM5] = USART_PARITY_NONE;
        g_ParamData.uart_parity[COM6] = USART_PARITY_NONE;
        g_ParamData.uart_parity[COM7] = USART_PARITY_NONE;
        g_ParamData.uart_stopBits[COM2] = USART_STOPBITS_1;
        g_ParamData.uart_stopBits[COM5] = USART_STOPBITS_1;
        g_ParamData.uart_stopBits[COM6] = USART_STOPBITS_1;
        g_ParamData.uart_stopBits[COM7] = USART_STOPBITS_1;
        g_ParamData.cmd_boardCastDeltaTime[COM2] = 10;
        g_ParamData.cmd_boardCastDeltaTime[COM5] = 10;
        g_ParamData.cmd_boardCastDeltaTime[COM6] = 10;
        g_ParamData.cmd_boardCastDeltaTime[COM7] = 10;
    }

    /* 设置避障相关默认数据 */
    if (saveFlags & PARAM_SAVE_AVOIDANCE_DATA)
    {
        g_ParamData.lidar_frontBackAngleCheckThreshold = 30;
        g_ParamData.lidar_sideAngleCheckThreshold = 25;
        g_ParamData.avoidance_frontAngleRange = 60;
        g_ParamData.avoidance_ultraSonicThreshold = 25;
        g_ParamData.avoidance_ctrlSpeedTGain = 1.0;
    }

    /* 设置系统相关默认数据 */
    if (saveFlags & PARAM_SAVE_SYSTEM_DATA)
    {
        g_ParamData.pwr_standbyTime = 30;
        g_ParamData.system_BeepVoltage = 10.8;
        g_ParamData.system_VoltageCoff = 392;
        g_ParamData.system_VoltageBeepFlag = 1;
        g_ParamData.system_BeepEnableFlag = 1;
    }

    /* 设置信息相关默认数据 */
    if (saveFlags & PARAM_SAVE_INFO_DATA)
    {
        String_Copy(g_ParamData.info_guiMainTitle, "智能小车开发板", 32);
        String_Copy(g_ParamData.info_guiMainWinProjectTitle, "项目:", 32);
        String_Copy(g_ParamData.info_guiMainWinProjectName, "基于STM32&K230的智能小车", 32);
        String_Copy(g_ParamData.info_guiMainWinExtraTitle, "视觉:", 32);
        String_Copy(g_ParamData.info_guiMainWinExtraName, "嘉立创K230开发板", 32);
        String_Copy(g_ParamData.info_guiMenuShowAnother, "陈承岩", 32);
        String_Copy(g_ParamData.info_guiMenuShowProject, "基于STM32&K230的智能小车", 32);
        String_Copy(g_ParamData.info_guiMenuShowVerision, "V4.1.0", 32);
        String_Copy(g_ParamData.info_guiMenuShowOrganization1, "安徽新华学院", 32);
        String_Copy(g_ParamData.info_guiMenuShowOrganization2, "大数据与人工智能学院", 32);
        String_Copy(g_ParamData.info_guiMenuShowExtraInfo, "使用于2026年毕业设计", 32);
        String_Copy(g_ParamData.info_guiMenuShowPcbVersion, "V4.1.0", 32);
    }

    /* 设置 ESP8266 相关默认数据 */
    if (saveFlags & PARAM_SAVE_ESP8266_DATA)
    {
        String_Copy(g_ParamData.esp8266_wifiSsid, "YourSSID", 32);
        String_Copy(g_ParamData.esp8266_wifiPassword, "YourPassword", 32);
        String_Copy(g_ParamData.esp8266_protocol, "TCP", 5);
        String_Copy(g_ParamData.esp8266_serverIp, "192.168.1.100", 16);
    }

    /* 设置蓝牙相关默认数据 */
    if (saveFlags & PARAM_SAVE_BLUETOOTH_DATA)
    {
        String_Copy(g_ParamData.bluetooth_name, "SmartCar", 32);
        String_Copy(g_ParamData.bluetooth_pin, "1234", 32);
        String_Copy(g_ParamData.bluetooth_role, "Slave", 9);
    }

    /* 保存默认数据 */
    Param_ReadToGlobal(); /* 将数据加载到全局变量 */
    Param_Save(saveFlags);
}

/**
  * @brief  清理参数数据并恢复到默认设置
  * @note   这将清除所有数据并恢复到默认设置，慎用！
  */
void Param_Clear(void)
{
    /* 清理数据 */
    memset(&g_ParamData, 0, sizeof(ParamData_T));

    /* 设置标志位 */
    Param_LoadDefault(PARAM_SAVE_ALL_DATA);
}

/**
  * @brief  从外部参数数据加载到全局变量
  * @param  data 指向外部参数数据的指针
  */
void Param_LoadFromData(ParamData_T *data)
{
    /* 验证指针的合法性 */
    if (data == NULL) { return; }

    /* 验证魔数 */
    if (data->MagicNumber != PARAM_MAGIC_NUMBER)
    {
        DEBUG_PRINTF("警告：魔数不匹配，请详细检查设置\r\n");
        DEBUG_PRINTF("数据魔数=0x%08X，期望魔数=0x%08X\r\n", data->MagicNumber, PARAM_MAGIC_NUMBER);
        return;
    }

    /* 加载数据 */
    g_ParamData = *data;

    /* 将数据加载到全局变量 */
    Param_ReadToGlobal();
}

/**
  * @brief  从全局变量导出到参数数据
  * @param  data 指向外部参数数据的指针
  */
void Param_ExportToData(ParamData_T *data)
{
    /* 导出数据 */
    if (data == NULL) { return; }
    *data = g_ParamData;
}
