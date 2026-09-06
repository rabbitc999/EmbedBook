#include "bsp_system.h"
#include "bsp_periph.h"

#define DHT11_GPIO_PIN  PA3
#define DHT11_GPIO_PORT GPIOA

/**
 * @brief 向DHT11写入数据
 * @param dat 要写入的数据（0或1）
 */
void DHT11_WR_DATA(uint8_t dat)
{
    BSP_GPIO_WritePin(DHT11_GPIO_PIN, dat);
}

/**
 * @brief 从DHT11读取数据
 * @return uint8_t 1-读取到高电平，0-读取到低电平
 */
uint8_t DHT11_RD_DATA(void)
{
    return BSP_GPIO_ReadPin(DHT11_GPIO_PIN);
}

/**
 * @brief DHT11复位函数
 * @details 通过拉低DQ线至少18ms，然后拉高20~40us来复位DHT11
 */
void DHT11_Reset()
{
    DHT11_WR_DATA(0);  // 拉低DQ
    DWT_DelayMs(20);   // 拉低至少18ms
    DHT11_WR_DATA(1);  // DQ=1
    DWT_DelayUs(30);   // 主机拉高20~40us
}

/**
 * @brief 检测DHT11响应
 * @return uint8_t 0-响应正常，1-响应异常
 * @details DHT11复位后会拉低40~50us，然后拉高40~50us，函数通过检测这两个状态来判断响应是否正常
 */
uint8_t DHT11_Check()
{
    uint8_t retry = 0;
    while (DHT11_RD_DATA() && retry < 100)  // DHT11会拉低40~50us
    {
        retry++;
        DWT_DelayUs(1);
    };
    if (retry >= 100) { return 1; }
    else { retry = 0; }
    while (!DHT11_RD_DATA() && retry < 100)  // DHT11拉低后会再次拉高40~50us
    {
        retry++;
        DWT_DelayUs(1);
    };
    if (retry >= 100) { return 1; }
    return 0;
}

/**
 * @brief 从DHT11读取一个比特
 * @return uint8_t 1-读取到高电平，0-读取到低电平
 * @details DHT11发送一个比特时会先拉低50us，然后根据持续时间来表示0或1：26~28us表示0，116~118us表示1。函数通过检测这两个状态来判断读取到的比特值。
 */
uint8_t DHT11_ReadBit(void)
{
    uint8_t retry = 0;
    while (DHT11_RD_DATA() && retry < 100)  // 等待变为低电平 12-14us 开始
    {
        retry++;
        DWT_DelayUs(1);
    }
    retry = 0;
    while (!DHT11_RD_DATA() && retry < 100)  // 等待变高电平 26-28us表示0,116-118us表示1
    {
        retry++;
        DWT_DelayUs(1);
    }
    DWT_DelayUs(40);  // 等待40us
    if (DHT11_RD_DATA()) { return 1; }
    else { return 0; }
}

/**
 * @brief 从DHT11读取一个字节
 * @return uint8_t 读到的数据
 */
uint8_t DHT11_ReadByte(void)
{
    uint8_t i, dat;
	dat = 0;
    for (i = 0; i < 8; i++)
    {
        dat <<= 1;
        dat |= DHT11_ReadBit();
    }
    return dat;
}

/**
 * @brief DHT11初始化函数
 * @return uint8_t 0-初始化成功，1-初始化失败
 */
uint8_t DHT11_Init()
{
    BSP_GPIO_Init(DHT11_GPIO_PIN, OUTPUT_OD_U);

	BSP_DISABLE_INT();  // 关闭中断，确保初始化过程中不被打断
    DHT11_Reset();
    uint8_t check = DHT11_Check();
	BSP_ENABLE_INT();
	return check;
}

/**
 * @brief 从DHT11读取温湿度数据
 * @param temp 指向存储温度数据的变量指针
 * @param humi 指向存储湿度数据的变量指针
 * @return uint8_t 0-读取成功，1-读取失败
 */
uint8_t DHT11_ReadData(uint8_t *temp, uint8_t *humi)
{
    uint8_t buf[5];
    uint8_t i;
	BSP_DISABLE_INT();  // 关闭中断，确保读取过程中不被打断
    DHT11_Reset();  // 复位DHT11
    if (DHT11_Check() == 0)
    {
        for (i = 0; i < 5; i++)  // 读取40位数据
        {
            buf[i] = DHT11_ReadByte();
        }
        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])
        {
            *humi = buf[0];
            *temp = buf[2];
        }
    }
	else
	{
		BSP_ENABLE_INT();  // 使能中断
		return 1;  // 读取失败
	}
	
	BSP_ENABLE_INT();
    return 0;
}
