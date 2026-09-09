#include "stm32f10x.h" // 包含STM32F10x系列微控制器的头文件
#include "lcd.h"
#include "lcd_data.h"
#include "delay.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

struct _LCD LCD;

/* 写入DC引脚 */
void LCD_WR_DC(uint8_t Data)
{
    GPIO_WriteBit(GPIOC, GPIO_Pin_5, (BitAction)Data); // 设置DC引脚状态
}

/* 写入CS引脚 */
void LCD_WR_CS(uint8_t Data)
{
    GPIO_WriteBit(GPIOB, GPIO_Pin_0, (BitAction)Data); // 设置CS引脚状态
}

/* 写入RES引脚 */
void LCD_WR_RES(uint8_t Data)
{
    GPIO_WriteBit(GPIOC, GPIO_Pin_4, (BitAction)Data); // 设置RES引脚状态
}

/* 写入BLK引脚 */
void LCD_WR_BLK(uint8_t Data)
{
    GPIO_WriteBit(GPIOB, GPIO_Pin_1, (BitAction)Data); // 设置BLK引脚状态
}

/* 写入指令或数据到SPI总线 */
void LCD_WR_BUS(uint8_t Data)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);                         // 等待发送缓冲区为空
    SPI_I2S_SendData(SPI1, Data); // 发送数据
    Delay_us(1);
}

/* 写入16位数据到SPI总线 */
void LCD_WR_BUS16(uint16_t Data)
{
    LCD_WR_BUS(Data >> 8);
    LCD_WR_BUS(Data);
}

/* 写8位数据 */
void LCD_WR_DATA8(uint8_t Data)
{
    LCD_WR_CS(0);     // 拉低CS引脚
    LCD_WR_DC(1);     // 设置为数据模式
    LCD_WR_BUS(Data); // 写入数据
    LCD_WR_CS(1);     // 拉高CS引脚
}

/* 写16位数据 */
void LCD_WR_DATA(uint16_t Data)
{
    LCD_WR_CS(0);       // 拉低CS引脚
    LCD_WR_DC(1);       // 设置为数据模式
    LCD_WR_BUS16(Data); // 写入数据
    LCD_WR_CS(1);       // 拉高CS引脚
}

/* 写命令 */
void LCD_WR_REG(uint8_t Data)
{
    LCD_WR_CS(0);     // 拉低CS引脚
    LCD_WR_DC(0);     // 设置为命令模式
    LCD_WR_BUS(Data); // 写入命令
    LCD_WR_CS(1);     // 拉高CS引脚
}

/* 初始化LCD的GPIO引脚 */
void LCD_GPIO_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // 使能GPIO
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // 使能GPIO

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // 速度50MHz
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPIOB, &GPIO_InitStructure); // 初始化GPIOB
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_Init(GPIOC, &GPIO_InitStructure); // 初始化GPIOC
}

/* 初始化SPI接口 */
void LCD_SPI_Init(void)
{
    /* 使能相关时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 使能GPIO
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);  // 使能SPI1

    /* 配置SPI1的MOSI和SCK引脚 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // 速度50MHz
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;   // 复用推挽输出
    GPIO_Init(GPIOA, &GPIO_InitStructure);            // 初始化GPIO
    GPIO_SetBits(GPIOA, GPIO_Pin_5 | GPIO_Pin_7);

    /* 配置SPI1 */
    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;          // 只发送模式
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;                      // 设置SPI工作模式：主机模式
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;                  // 设置SPI数据大小：8位帧结构
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;                        // 串行同步时钟空闲时SCLK位高电平
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;                       // 串行同步时钟空第二个时钟沿捕获
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;                          // NSS信号由硬件管理
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; // 波特率预分频值：波特率预分频值为8
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;                 // 数据传输高位先行
    SPI_InitStructure.SPI_CRCPolynomial = 7;                           // CRC值计算的多项式
    SPI_Init(SPI1, &SPI_InitStructure);                                // 初始化SPI

    /* 使能SPI */
    SPI_Cmd(SPI1, ENABLE);
}

/* 设置显示区域 */
void LCD_Address_Set(uint16_t StartX, uint16_t StartY, uint16_t EndX, uint16_t EndY)
{
#if LCD_DEVICE == 1
    if (LCD.Direction == LCD_Horizontal)
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX + 52);
        LCD_WR_DATA(EndX + 52);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY + 40);
        LCD_WR_DATA(EndY + 40);
        LCD_WR_REG(0x2c); // 储存器写
    }
    else if (LCD.Direction == LCD_RevHorizontal)
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX + 53);
        LCD_WR_DATA(EndX + 53);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY + 40);
        LCD_WR_DATA(EndY + 40);
        LCD_WR_REG(0x2c); // 储存器写
    }
    else if (LCD.Direction == LCD_Vertical)
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX + 40);
        LCD_WR_DATA(EndX + 40);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY + 53);
        LCD_WR_DATA(EndY + 53);
        LCD_WR_REG(0x2c); // 储存器写
    }
    else
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX + 40);
        LCD_WR_DATA(EndX + 40);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY + 52);
        LCD_WR_DATA(EndY + 52);
        LCD_WR_REG(0x2c); // 储存器写
    }
#elif LCD_DEVICE == 2
    if (LCD.Direction == LCD_Vertical)
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX);
        LCD_WR_DATA(EndX);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY);
        LCD_WR_DATA(EndY);
        LCD_WR_REG(0x2c); // 储存器写
    }
    else if (LCD.Direction == LCD_RevVertical)
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX);
        LCD_WR_DATA(EndX);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY + 80);
        LCD_WR_DATA(EndY + 80);
        LCD_WR_REG(0x2c); // 储存器写
    }
    else if (LCD.Direction == LCD_Horizontal)
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX);
        LCD_WR_DATA(EndX);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY);
        LCD_WR_DATA(EndY);
        LCD_WR_REG(0x2c); // 储存器写
    }
    else
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX + 80);
        LCD_WR_DATA(EndX + 80);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY);
        LCD_WR_DATA(EndY);
        LCD_WR_REG(0x2c); // 储存器写
    }
#elif LCD_DEVICE == 3 || LCD_DEVICE == 4
    LCD_WR_REG(0x2a); // 列地址设置
    LCD_WR_DATA(StartX);
    LCD_WR_DATA(EndX);
    LCD_WR_REG(0x2b); // 行地址设置
    LCD_WR_DATA(StartY);
    LCD_WR_DATA(EndY);
    LCD_WR_REG(0x2c); // 储存器写
#else
    if (LCD.Direction == LCD_Horizontal)
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX + 1);
        LCD_WR_DATA(EndX + 1);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY + 2);
        LCD_WR_DATA(EndY + 2);
        LCD_WR_REG(0x2c); // 储存器写
    }
    else if (LCD.Direction == LCD_RevHorizontal)
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX + 1);
        LCD_WR_DATA(EndX + 1);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY + 2);
        LCD_WR_DATA(EndY + 2);
        LCD_WR_REG(0x2c); // 储存器写
    }
    else if (LCD.Direction == LCD_Vertical)
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX + 2);
        LCD_WR_DATA(EndX + 2);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY + 1);
        LCD_WR_DATA(EndY + 1);
        LCD_WR_REG(0x2c); // 储存器写
    }
    else
    {
        LCD_WR_REG(0x2a); // 列地址设置
        LCD_WR_DATA(StartX + 2);
        LCD_WR_DATA(EndX + 2);
        LCD_WR_REG(0x2b); // 行地址设置
        LCD_WR_DATA(StartY + 1);
        LCD_WR_DATA(EndY + 1);
        LCD_WR_REG(0x2c); // 储存器写
    }
#endif
}

/* 判断指定点是否在指定多边形内部 */
uint8_t LCD_IsPointInPoly(uint8_t nvert, uint16_t *vertx, uint16_t *verty, uint16_t testx, uint16_t testy)
{
    uint16_t i, j, c = 0;

    /* 此算法由W. Randolph Franklin提出 */
    /* 参考链接：https://wrfranklin.org/Research/Short_Notes/pnpoly.html */
    for (i = 0, j = nvert - 1; i < nvert; j = i++)
    {
        if (((verty[i] > testy) != (verty[j] > testy)) && (testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i]))
        {
            c = !c;
        }
    }
    return c;
}

/* 获取最大值 */
int16_t LCD_GetMax(uint16_t Array[], int n)
{
    uint16_t Max = Array[0];
    uint8_t i;

    for (i = 1; i < n; i++)
    {
        if (Array[i] > Max)
        {
            Max = Array[i];
        }
    }

    return Max;
}

/* 获取最小值 */
int16_t LCD_GetMin(uint16_t Array[], int n)
{
    uint16_t Min = Array[0];
    uint8_t i;

    for (i = 1; i < n; i++)
    {
        if (Array[i] < Min)
        {
            Min = Array[i];
        }
    }

    return Min;
}

/* 判断指定点是否在指定角度内 */
uint8_t LCD_IsInAngle(uint16_t X, uint16_t Y, int16_t StartAngle, int16_t EndAngle)
{
    uint16_t PointAngle;
    PointAngle = atan2(Y, X) / 3.14 * 180; // 计算指定点的弧度，并转换为角度表示
    if (StartAngle < EndAngle)             // 起始角度小于终止角度的情况
    {
        /* 如果指定角度在起始终止角度之间，则判定指定点在指定角度 */
        if (PointAngle >= StartAngle && PointAngle <= EndAngle)
        {
            return 1;
        }
    }
    else // 起始角度大于于终止角度的情况
    {
        /* 如果指定角度大于起始角度或者小于终止角度，则判定指定点在指定角度 */
        if (PointAngle >= StartAngle || PointAngle <= EndAngle)
        {
            return 1;
        }
    }
    return 0; // 不满足以上条件，则判断判定指定点不在指定角度
}

/* 绝对值函数 */
int32_t LCD_ABS(int32_t X)
{
    return X > 0 ? X : -X; // 不满足以上条件，则判断判定指定点不在指定角度
}

/* 指数函数 */
uint32_t LCD_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1; // 结果默认为1
    while (Y--)          // 累乘Y次
    {
        Result *= X; // 每次把X累乘到结果上
    }
    return Result;
}

/* LCD设置显示方向 */
void LCD_SetDirection(LCD_Direction Direction)
{
    LCD.Direction = Direction; // 更新方向参数

    LCD_WR_REG(0x36); // 写入方向控制寄存器

#if LCD_DEVICE == 2 || LCD_DEVICE == 3 || LCD_DEVICE == 4
    if (LCD.Direction == LCD_Horizontal)
    {
        LCD_WR_DATA8(0x00); // 水平方向，正常显示
        LCD.Height = LCD_DEVICE_WIDTH;
        LCD.Width = LCD_DEVICE_HEIGHT;
    }
    else if (LCD.Direction == LCD_RevHorizontal)
    {
        LCD_WR_DATA8(0xC0); // 水平方向，反转显示
        LCD.Height = LCD_DEVICE_WIDTH;
        LCD.Width = LCD_DEVICE_HEIGHT;
    }
    else if (LCD.Direction == LCD_Vertical)
    {
        LCD_WR_DATA8(0x70); // 垂直方向，正常显示
        LCD.Height = LCD_DEVICE_HEIGHT;
        LCD.Width = LCD_DEVICE_WIDTH;
    }
    else
    {
        LCD_WR_DATA8(0xA0); // 垂直方向，反转显示
        LCD.Height = LCD_DEVICE_HEIGHT;
        LCD.Width = LCD_DEVICE_WIDTH;
    }
#elif LCD_DEVICE == 1
    if (LCD.Direction == LCD_Horizontal)
    {
        LCD_WR_DATA8(0x00); // 水平方向，正常显示
        LCD.Height = LCD_DEVICE_HEIGHT;
        LCD.Width = LCD_DEVICE_WIDTH;
    }
    else if (LCD.Direction == LCD_RevHorizontal)
    {
        LCD_WR_DATA8(0xC0); // 水平方向，反转显示
        LCD.Height = LCD_DEVICE_HEIGHT;
        LCD.Width = LCD_DEVICE_WIDTH;
    }
    else if (LCD.Direction == LCD_Vertical)
    {
        LCD_WR_DATA8(0x70); // 垂直方向，正常显示
        LCD.Height = LCD_DEVICE_WIDTH;
        LCD.Width = LCD_DEVICE_HEIGHT;
    }
    else
    {
        LCD_WR_DATA8(0xA0); // 垂直方向，反转显示
        LCD.Height = LCD_DEVICE_WIDTH;
        LCD.Width = LCD_DEVICE_HEIGHT;
    }
#else
    if (LCD.Direction == LCD_RevVertical)
    {
        LCD_WR_DATA8(0x00); // 水平方向，正常显示
        LCD.Height = LCD_DEVICE_WIDTH;
        LCD.Width = LCD_DEVICE_HEIGHT;
    }
    else if (LCD.Direction == LCD_Vertical)
    {
        LCD_WR_DATA8(0xC0); // 水平方向，反转显示
        LCD.Height = LCD_DEVICE_WIDTH;
        LCD.Width = LCD_DEVICE_HEIGHT;
    }
    else if (LCD.Direction == LCD_RevHorizontal)
    {
        LCD_WR_DATA8(0x70); // 垂直方向，正常显示
        LCD.Height = LCD_DEVICE_HEIGHT;
        LCD.Width = LCD_DEVICE_WIDTH;
    }
    else
    {
        LCD_WR_DATA8(0xA0); // 垂直方向，反转显示
        LCD.Height = LCD_DEVICE_HEIGHT;
        LCD.Width = LCD_DEVICE_WIDTH;
    }
#endif
}

/* LCD设置背景色 */
void LCD_SetBackColor(uint16_t Color)
{
    LCD.BackColor = Color; // 更新背景颜色
}

/* LCD绘图颜色设置 */
void LCD_SetPaintColor(uint16_t Color)
{
    LCD.PaintColor = Color; // 更新绘图颜色
}

/* LCD字体大小设置 */
void LCD_SetFontSize(LCD_FontSize Size)
{
    LCD.FontSize = Size; // 更新绘图颜色
}

/* LCD透明填充模式 */
void LCD_SetMonoFill(LCD_MonoFill Fill)
{
    LCD.MonoFill = Fill; // 更新绘图颜色
}

/* LCD填充颜色 */
void LCD_Fill(uint16_t StartX, uint16_t StartY, uint16_t EndX, uint16_t EndY, uint16_t Color)
{
    if (StartX > LCD.Width || EndX > LCD.Width || StartY > LCD.Height || EndY > LCD.Height)
        return;
    if (StartX >= EndX || StartY >= EndY)
        return;

    DMA_InitTypeDef DMA_InitStructure;
    uint32_t Size = (uint32_t)(EndX - StartX + 1) * (EndY - StartY + 1); // 计算填充区域的像素数
    LCD_Address_Set(StartX, StartY, EndX - 1, EndY - 1);                 // 设置显示范围

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE); // 使能DMA传输时钟
    SPI_DataSizeConfig(SPI1, SPI_DataSize_16b);        // 切换为16位数据

    while (Size > 0)
    {
        uint16_t tmpColor = Color, tmpSize = Size > 65534 ? 65534 : Size;

        DMA_DeInit(DMA1_Channel3);                                                  // 将DMA的通道1寄存器重设为缺省值
        DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI1->DR;             // DMA外设ADC基地址
        DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&tmpColor;                 // DMA内存基地址
        DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;                          // 数据传输方向，从内存读取发送到外设
        DMA_InitStructure.DMA_BufferSize = tmpSize;                                 // 设置DMA通道的DMA缓存的大小
        DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;            // 外设地址寄存器不变
        DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;                    // 内存地址寄存器不变
        DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; // 数据宽度为16位
        DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;         // 数据宽度为16位
        DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;                               // 工作在正常缓存模式
        DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;                       // DMA通道拥有中优先级
        DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;                                // DMA通道没有设置为内存到内存传输
        DMA_Init(DMA1_Channel3, &DMA_InitStructure);                                // 根据DMA_InitStruct中指定的参数初始化DMA的通道USART1_Tx_DMA_Channel所标识的寄存器

        SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);

        LCD_WR_CS(0); // 拉低CS引脚
        LCD_WR_DC(1); // 设置为数据模式
        DMA_Cmd(DMA1_Channel3, DISABLE);
        DMA_SetCurrDataCounter(DMA1_Channel3, tmpSize);
        DMA_Cmd(DMA1_Channel3, ENABLE); // 发送DMA
        while (DMA_GetFlagStatus(DMA1_FLAG_TC3) == RESET)
            ;         // 等待DMA
        LCD_WR_CS(1); // 拉高CS引脚

        if (Size < 65534)
        {
            break;
        }
        else
        {
            Size = Size - 65534;
        }
    }

    SPI_DataSizeConfig(SPI1, SPI_DataSize_8b); // 切换为8位数据
}

/* LCD清空颜色为背景色 */
void LCD_Clear(void)
{
    LCD_Fill(0, 0, LCD.Width, LCD.Height, LCD.BackColor); // 使能SPI
}

/* LCD绘制单个点 */
void LCD_DrawPoint(uint16_t X, uint16_t Y)
{
    if (X >= LCD.Width || Y >= LCD.Height)
        return;
    LCD_Address_Set(X, Y, X, Y); // 设置光标位置
    LCD_WR_DATA(LCD.PaintColor); // 写入颜色数据
}

/* LCD显示ASCII字符 */
void LCD_ShowChar(uint16_t X, uint16_t Y, uint8_t ShowChar)
{
    if (LCD.FontSize == LCD_FontSize16)
        LCD_ShowMonoImage(X, Y, 8, 16, LCD_F16X08[ShowChar - ' ']);
    if (LCD.FontSize == LCD_FontSize24)
        LCD_ShowMonoImage(X, Y, 12, 24, LCD_F24X12[ShowChar - ' ']);
    if (LCD.FontSize == LCD_FontSize32)
        LCD_ShowMonoImage(X, Y, 16, 32, LCD_F32X16[ShowChar - ' ']);
}

/* LCD显示字符串 */
void LCD_ShowString(uint16_t X, uint16_t Y, const char *String)
{
    uint16_t i = 0;
    char SingleChar[5];
    uint8_t CharLength = 0;
    uint16_t XOffset = 0;
    uint16_t pIndex;
    uint16_t FontSize = (uint16_t)LCD.FontSize / 2; // 字体大小

    while (String[i] != '\0' && X + XOffset < LCD.Width) // 遍历字符串
    {
        /* 此段代码的目的是，提取GB2312字符串中的一个字符，转存到SingleChar子字符串中 */
        /* 判断GB2312字节的最高位标志位 */
        if ((String[i] & 0x80) == 0x00) // 最高位为0
        {
            CharLength = 1;              // 字符为1字节
            SingleChar[0] = String[i++]; // 将第一个字节写入SingleChar第0个位置，随后i指向下一个字节
            SingleChar[1] = '\0';        // 为SingleChar添加字符串结束标志位
        }
        else // 最高位为1
        {
            CharLength = 2;              // 字符为2字节
            SingleChar[0] = String[i++]; // 将第一个字节写入SingleChar第0个位置，随后i指向下一个字节
            if (String[i] == '\0')
            {
                break;
            }                            // 意外情况，跳出循环，结束显示
            SingleChar[1] = String[i++]; // 将第二个字节写入SingleChar第1个位置，随后i指向下一个字节
            SingleChar[2] = '\0';        // 为SingleChar添加字符串结束标志位
        }

        /* 显示上述代码提取到的SingleChar */
        if (CharLength == 1) // 如果是单字节字符
        {
            /* 使用LCD_ShowChar显示此字符 */
            LCD_ShowChar(X + XOffset, Y, SingleChar[0]);
            XOffset += FontSize;
        }
        else // 否则，即多字节字符
        {
            /* 遍历整个字模库，从字模库中寻找此字符的数据 */
            /* 如果找到最后一个字符（定义为空字符串），则表示字符未在字模库定义，停止寻找 */
            for (pIndex = 0; strcmp(LCD_CF16X16[pIndex].Index, "") != 0; pIndex++)
            {
                /* 找到匹配的字符 */
                if (strcmp(LCD_CF16X16[pIndex].Index, SingleChar) == 0)
                {
                    break; // 跳出循环，此时pIndex的值为指定字符的索引
                }
            }

            if (LCD.FontSize == LCD_FontSize16) // 给定字体为8*16点阵
            {
                /* 将字模库LCD_CF16x16的指定数据以16*16的图像格式显示 */
                LCD_ShowMonoImage(X + XOffset, Y, 16, 16, LCD_CF16X16[pIndex].Data);
                XOffset += FontSize << 1;
            }
            else
            {
                /* 其他字符则在此位置缩放显示 */
                LCD_ShowChar(X + XOffset, Y, '?');
                XOffset += FontSize;
            }
        }
    }
}

/* LCD显示无符号数 */
void LCD_ShowNumber(uint16_t X, uint16_t Y, uint32_t Number, uint8_t Length)
{
    uint8_t i, FontSize = LCD.FontSize >> 1;
    for (i = 0; i < Length; i++) // 遍历数字的每一位
    {
        /* 调用LCD_ShowChar函数，依次显示每个数字 */
        /* Number / LCD_Pow(10, Length - i - 1) % 10 可以十进制提取数字的每一位 */
        /* + '0' 可将数字转换为字符格式 */
        LCD_ShowChar(X + i * FontSize, Y, Number / LCD_Pow(10, Length - i - 1) % 10 + '0');
    }
}

/* LCD显示有符号数 */
void LCD_ShowSignedNumber(uint16_t X, uint16_t Y, int32_t Number, uint8_t Length)
{
    uint8_t i, FontSize = LCD.FontSize >> 1;
    uint32_t Number1;

    if (Number >= 0) // 数字大于等于0
    {
        LCD_ShowChar(X, Y, '+'); // 显示+号
        Number1 = Number;        // Number1直接等于Number
    }
    else // 数字小于0
    {
        LCD_ShowChar(X, Y, '-'); // 显示-号
        Number1 = -Number;       // Number1等于Number取负
    }

    for (i = 0; i < Length; i++) // 遍历数字的每一位
    {
        /* 调用LCD_ShowChar函数，依次显示每个数字 */
        /* Number1 / LCD_Pow(10, Length - i - 1) % 10 可以十进制提取数字的每一位 */
        /* + '0' 可将数字转换为字符格式 */
        LCD_ShowChar(X + (i + 1) * FontSize, Y, Number1 / LCD_Pow(10, Length - i - 1) % 10 + '0');
    }
}

/* LCD显示十六进制数字 */
void LCD_ShowHexNumber(int16_t X, int16_t Y, uint32_t Number, uint8_t Length)
{
    uint8_t i, SingleNumber, FontSize = LCD.FontSize >> 1;
    for (i = 0; i < Length; i++) /* 遍历数字的每一位 */
    {
        /* 以十六进制提取数字的每一位 */
        SingleNumber = Number / LCD_Pow(16, Length - i - 1) % 16;

        if (SingleNumber < 10) // 单个数字小于10
        {
            /* 调用LCD_ShowChar函数，显示此数字 */
            /* + '0' 可将数字转换为字符格式 */
            LCD_ShowChar(X + i * FontSize, Y, SingleNumber + '0');
        }
        else // 单个数字大于10
        {
            /* 调用LCD_ShowChar函数，显示此数字 */
            /* + 'A' 可将数字转换为从A开始的十六进制字符 */
            LCD_ShowChar(X + i * FontSize, Y, SingleNumber - 10 + 'A');
        }
    }
}

/* LCD显示二进制数字 */
void LCD_ShowBinNumber(int16_t X, int16_t Y, uint32_t Number, uint8_t Length)
{
    uint8_t i, FontSize = LCD.FontSize >> 1;
    ;
    for (i = 0; i < Length; i++) // 遍历数字的每一位
    {
        /* 调用LCD_ShowChar函数，依次显示每个数字 */
        /* Number / LCD_Pow(2, Length - i - 1) % 2 可以二进制提取数字的每一位 */
        /* + '0' 可将数字转换为字符格式 */
        LCD_ShowChar(X + i * FontSize, Y, Number / LCD_Pow(2, Length - i - 1) % 2 + '0');
    }
}

/* LCD显示浮点数 */
void LCD_ShowFloatNumber(int16_t X, int16_t Y, float Number, uint8_t IntLength, uint8_t FraLength)
{
    uint32_t PowNum, IntNum, FraNum, FontSize = LCD.FontSize >> 1;

    if (Number >= 0) // 数字大于等于0
    {
        LCD_ShowChar(X, Y, '+'); // 显示+号
    }
    else // 数字小于0
    {
        LCD_ShowChar(X, Y, '-'); // 显示-号
        Number = -Number;        // Number取负
    }

    /* 提取整数部分和小数部分 */
    IntNum = Number;                  // 直接赋值给整型变量，提取整数
    Number -= IntNum;                 // 将Number的整数减掉，防止之后将小数乘到整数时因数过大造成错误
    PowNum = LCD_Pow(10, FraLength);  // 根据指定小数的位数，确定乘数
    FraNum = roundf(Number * PowNum); // 将小数乘到整数，同时四舍五入，避免显示误差
    IntNum += FraNum / PowNum;        // 若四舍五入造成了进位，则需要再加给整数

    /* 显示整数部分 */
    LCD_ShowNumber(X + FontSize, Y, IntNum, IntLength);

    /* 显示小数点 */
    LCD_ShowChar(X + (IntLength + 1) * FontSize, Y, '.');

    /* 显示小数部分 */
    LCD_ShowNumber(X + (IntLength + 2) * FontSize, Y, FraNum, FraLength);
}

/* LCD打印格式化字符串 */
void LCD_Printf(uint16_t X, uint16_t Y, const char *String, ...)
{
    char Buffer[100];              // 定义字符数组
    va_list arg;                   // 定义可变参数列表
    va_start(arg, String);         // 初始化可变参数列表
    vsprintf(Buffer, String, arg); // 格式化字符串
    va_end(arg);                   // 结束可变参数列表
    LCD_ShowString(X, Y, Buffer);
}

/* LCD显示单色图片 */
void LCD_ShowMonoImage(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, const uint8_t *Image)
{
    uint8_t mask = 0x01, i;

    if (X + Width > LCD.Width || Y + Height > LCD.Height)
        return;

    uint32_t Size = (uint32_t)Width * Height / 8; // 计算填充区域的像素数

    if (LCD.MonoFill == LCD_Cover) // 覆盖模式
    {
        LCD_Address_Set(X, Y, X + Width - 1, Y + Height - 1); // 设置显示区域

        LCD_WR_CS(0); // 拉低CS引脚
        LCD_WR_DC(1); // 拉高DC引脚
        while (Size--)
        {
            mask = 0x01; // 初始掩码值

            for (i = 0; i < 8; i++) // 循环8次，对应8个位
            {
                LCD_WR_BUS16((*Image & mask) ? LCD.PaintColor : LCD.BackColor);
                mask <<= 1; // 每次循环将掩码左移一位
            }
            Image++;
        }

        LCD_WR_CS(1); // 拉高CS引脚
    }
    else // 透明模式
    {
        while (Size--)
        {
            for (i = 0; i < 8; i++) // 循环8次，对应8个位
            {
                if (*Image & (1 << i)) // 检查第i位是否为1
                {
                    uint16_t offsetX = Size * 8 + i + 7; // 计算X方向的偏移量
                    LCD_DrawPoint(X + offsetX % Width, Y + Width - offsetX / Width);
                }
            }
            Image++;
        }
    }
}

/* LCD显示图片 */
void LCD_ShowImage(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height, const uint8_t *Image)
{
    if (X + Width > LCD.Width || Y + Height > LCD.Height)
        return;

    DMA_InitTypeDef DMA_InitStructure;
    uint32_t Size = (uint32_t)Width * Height * 2;         // 计算填充区域的像素数
    LCD_Address_Set(X, Y, X + Width - 1, Y + Height - 1); // 设置显示区域

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE); // 使能DMA传输时钟
    SPI_DataSizeConfig(SPI1, SPI_DataSize_16b);        // 切换为16位数据

    LCD_WR_CS(0); // 拉低CS引脚
    LCD_WR_DC(1); // 设置为数据模式

    while (Size > 0)
    {
        uint16_t tmpSize = Size > 65534 ? 65534 : Size;

        DMA_DeInit(DMA1_Channel3);                                              // 将DMA的通道1寄存器重设为缺省值
        DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI1->DR;         // DMA外设ADC基地址
        DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)Image;                 // DMA内存基地址
        DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;                      // 数据传输方向，从内存读取发送到外设
        DMA_InitStructure.DMA_BufferSize = tmpSize;                             // 设置DMA通道的DMA缓存的大小
        DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;        // 外设地址寄存器不变
        DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;                 // 内存地址寄存器递增
        DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 数据宽度为8位
        DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;         // 数据宽度为8位
        DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;                           // 工作在正常缓存模式
        DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;                   // DMA通道拥有中优先级
        DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;                            // DMA通道没有设置为内存到内存传输
        DMA_Init(DMA1_Channel3, &DMA_InitStructure);                            // 根据DMA_InitStruct中指定的参数初始化DMA的通道USART1_Tx_DMA_Channel所标识的寄存器

        SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);

        LCD_WR_CS(0); // 拉低CS引脚
        LCD_WR_DC(1); // 设置为数据模式
        DMA_Cmd(DMA1_Channel3, DISABLE);
        DMA_SetCurrDataCounter(DMA1_Channel3, Size);
        DMA_Cmd(DMA1_Channel3, ENABLE); // 发送DMA
        while (DMA_GetFlagStatus(DMA1_FLAG_TC3) == RESET)
            ;         // 等待DMA
        LCD_WR_CS(1); // 拉高CS引脚

        if (Size < 65534)
        {
            break;
        }
        else
        {
            Size = Size - 65534;
        }
    }

    LCD_WR_CS(1); // 拉高CS引脚

    SPI_DataSizeConfig(SPI1, SPI_DataSize_8b); // 切换为8位数据
}

/* LCD画直线 */
void LCD_DrawLine(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1)
{
    int16_t x, y, dx, dy, d, incrE, incrNE, temp;
    int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
    uint8_t yflag = 0, xyflag = 0;

    if (y0 == y1) // 横线单独处理
    {
        /* 0号点X坐标大于1号点X坐标，则交换两点X坐标 */
        if (x0 > x1)
        {
            temp = x0;
            x0 = x1;
            x1 = temp;
        }

        /* 遍历X坐标 */
        for (x = x0; x <= x1; x++)
        {
            LCD_DrawPoint(x, y0); // 依次画点
        }
    }
    else if (x0 == x1) // 竖线单独处理
    {
        /*0号点Y坐标大于1号点Y坐标，则交换两点Y坐标*/
        if (y0 > y1)
        {
            temp = y0;
            y0 = y1;
            y1 = temp;
        }

        /*遍历Y坐标*/
        for (y = y0; y <= y1; y++)
        {
            LCD_DrawPoint(x0, y); // 依次画点
        }
    }
    else // 斜线
    {
        /* 使用Bresenham算法画直线，可以避免耗时的浮点运算，效率更高 */
        /* 参考文档：https://www.cs.montana.edu/courses/spring2009/425/dslectures/Bresenham.pdf */
        /* 参考教程：https://www.bilibili.com/video/BV1364y1d7Lo */

        if (x0 > x1) // 0号点X坐标大于1号点X坐标
        {
            temp = x0;
            x0 = x1;
            x1 = temp;
            temp = y0;
            y0 = y1;
            y1 = temp;
        }

        if (y0 > y1) // 0号点Y坐标大于1号点Y坐标
        {
            y0 = -y0;
            y1 = -y1;
            yflag = 1;
        }

        if (y1 - y0 > x1 - x0) // 画线斜率大于1
        {
            temp = x0;
            x0 = y0;
            y0 = temp;
            temp = x1;
            x1 = y1;
            y1 = temp;
            xyflag = 1;
        }

        dx = x1 - x0;
        dy = y1 - y0;
        incrE = 2 * dy;
        incrNE = 2 * (dy - dx);
        d = 2 * dy - dx;
        x = x0;
        y = y0;

        if (yflag && xyflag)
        {
            LCD_DrawPoint(y, -x);
        }
        else if (yflag)
        {
            LCD_DrawPoint(x, -y);
        }
        else if (xyflag)
        {
            LCD_DrawPoint(y, x);
        }
        else
        {
            LCD_DrawPoint(x, y);
        }

        while (x < x1) // 遍历X轴的每个点
        {
            x++;
            if (d < 0) // 下一个点在当前点东方
            {
                d += incrE;
            }
            else // 下一个点在当前点东北方
            {
                y++;
                d += incrNE;
            }

            /* 画每一个点，同时判断标志位，将坐标换回来 */
            if (yflag && xyflag)
            {
                LCD_DrawPoint(y, -x);
            }
            else if (yflag)
            {
                LCD_DrawPoint(x, -y);
            }
            else if (xyflag)
            {
                LCD_DrawPoint(y, x);
            }
            else
            {
                LCD_DrawPoint(x, y);
            }
        }
    }
}

/* LCD画矩形 */
void LCD_DrawRectangle(uint16_t X, uint16_t Y, uint16_t Width, uint16_t Height)
{
    /* 判断是否为填充模式 */
    if (LCD.MonoFill == LCD_Cover)
    {
        LCD_Fill(X, Y, X + Width - 1, Y + Height - 1, LCD.PaintColor);
    }
    else
    {
        LCD_DrawLine(X, Y, X + Width - 1, Y);                           // 上边
        LCD_DrawLine(X, Y, X, Y + Height - 1);                          // 左边
        LCD_DrawLine(X + Width - 1, Y, X + Width - 1, Y + Height - 1);  // 右边
        LCD_DrawLine(X, Y + Height - 1, X + Width - 1, Y + Height - 1); // 下边
    }
}

/* LCD画三角形 */
void LCD_DrawTriangle(uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint16_t X2, uint16_t Y2)
{
    uint16_t minx, miny, maxx, maxy;
    uint16_t i, j;
    uint16_t vx[] = {X0, X1, X2};
    uint16_t vy[] = {Y0, Y1, Y2};

    if (LCD.MonoFill == LCD_Transparent) // 指定三角形不填充
    {
        LCD_DrawLine(X0, Y0, X1, Y1);
        LCD_DrawLine(X0, Y0, X2, Y2);
        LCD_DrawLine(X1, Y1, X2, Y2);
    }
    else // 指定三角形填充
    {
        minx = LCD_GetMin(vx, 3);
        miny = LCD_GetMin(vy, 3);
        maxx = LCD_GetMax(vx, 3);
        maxy = LCD_GetMax(vy, 3);

        for (i = minx; i <= maxx; i++)
        {
            for (j = miny; j <= maxy; j++)
            {
                if (LCD_IsPointInPoly(3, vx, vy, i, j))
                {
                    LCD_DrawPoint(i, j);
                }
            }
        }
    }
}

/* LCD画多边形 */
void LCD_DrawPoly(uint16_t vx[], uint16_t vy[], int n)
{
    uint16_t minx, miny, maxx, maxy;
    uint16_t i, j;

    if (LCD.MonoFill == LCD_Transparent) // 指定填充
    {
        /* 调用画线函数，将各个点用直线连接 */
        for (i = 0; i < n - 1; i++)
        {
            LCD_DrawLine(vx[i], vy[i], vx[i + 1], vy[i + 1]);
        }
        LCD_DrawLine(vx[0], vx[0], vx[n - 1], vx[n - 1]);
    }
    else // 指定填充
    {
        /* 找到最小的X、Y坐标 */
        minx = LCD_GetMin(vx, n);
        miny = LCD_GetMin(vy, n);

        /* 找到最大的X、Y坐标 */
        maxx = LCD_GetMax(vx, n);
        maxy = LCD_GetMax(vy, n);

        for (i = minx; i <= maxx; i++)
        {
            for (j = miny; j <= maxy; j++)
            {
                if (LCD_IsPointInPoly(n, vx, vy, i, j))
                {
                    LCD_DrawPoint(i, j);
                }
            }
        }
    }
}

/* LCD画圆 */
void LCD_DrawCircle(uint16_t X, uint16_t Y, uint8_t Radius)
{
    int16_t x, y, d, j;

    /* 使用Bresenham算法画圆，可以避免耗时的浮点运算，效率更高 */

    d = 1 - Radius;
    x = 0;
    y = Radius;

    /* 画每个八分之一圆弧的起始点 */
    LCD_DrawPoint(X + x, Y + y);
    LCD_DrawPoint(X - x, Y - y);
    LCD_DrawPoint(X + y, Y + x);
    LCD_DrawPoint(X - y, Y - x);

    if (LCD.MonoFill == LCD_Cover) // 指定圆填充
    {
        /* 遍历起始点Y坐标 */
        for (j = -y; j < y; j++)
        {
            /* 在指定区域画点，填充部分圆 */
            LCD_DrawPoint(X, Y + j);
        }
    }

    while (x < y) // 遍历X轴的每个点
    {
        x++;
        if (d < 0) // 下一个点在当前点东方
        {
            d += 2 * x + 1;
        }
        else // 下一个点在当前点东南方
        {
            y--;
            d += 2 * (x - y) + 1;
        }

        /* 画每个八分之一圆弧的点 */
        LCD_DrawPoint(X + x, Y + y);
        LCD_DrawPoint(X + y, Y + x);
        LCD_DrawPoint(X - x, Y - y);
        LCD_DrawPoint(X - y, Y - x);
        LCD_DrawPoint(X + x, Y - y);
        LCD_DrawPoint(X + y, Y - x);
        LCD_DrawPoint(X - x, Y + y);
        LCD_DrawPoint(X - y, Y + x);

        if (LCD.MonoFill == LCD_Cover) // 指定圆填充
        {
            /* 遍历中间部分 */
            for (j = -y; j < y; j++)
            {
                /* 在指定区域画点，填充部分圆 */
                LCD_DrawPoint(X + x, Y + j);
                LCD_DrawPoint(X - x, Y + j);
            }

            /* 遍历两侧部分 */
            for (j = -x; j < x; j++)
            {
                /* 在指定区域画点，填充部分圆 */
                LCD_DrawPoint(X - y, Y + j);
                LCD_DrawPoint(X + y, Y + j);
            }
        }
    }
}

/* LCD画椭圆 */
void LCD_DrawEllipse(uint16_t X, uint16_t Y, uint8_t A, uint8_t B)
{
    uint16_t x, y, j;
    uint16_t a = A, b = B;
    float d1, d2;

    /* 使用Bresenham算法画椭圆，可以避免部分耗时的浮点运算，效率更高 */
    /* 参考链接：https://blog.csdn.net/myf_666/article/details/128167392 */

    x = 0;
    y = b;
    d1 = b * b + a * a * (-b + 0.5);

    if (LCD.MonoFill == LCD_Cover) // 指定椭圆填充
    {
        /* 遍历起始点Y坐标 */
        for (j = -y; j < y; j++)
        {
            /* 在指定区域画点，填充部分椭圆 */
            LCD_DrawPoint(X, Y + j);
            LCD_DrawPoint(X, Y + j);
        }
    }

    /* 画椭圆弧的起始点 */
    LCD_DrawPoint(X + x, Y + y);
    LCD_DrawPoint(X - x, Y - y);
    LCD_DrawPoint(X - x, Y + y);
    LCD_DrawPoint(X + x, Y - y);

    /* 画椭圆中间部分 */
    while (b * b * (x + 1) < a * a * (y - 0.5))
    {
        if (d1 <= 0) /* 下一个点在当前点东方 */
        {
            d1 += b * b * (2 * x + 3);
        }
        else /* 下一个点在当前点东南方 */
        {
            d1 += b * b * (2 * x + 3) + a * a * (-2 * y + 2);
            y--;
        }
        x++;

        if (LCD.MonoFill == LCD_Cover) /* 指定椭圆填充 */
        {
            /* 遍历中间部分 */
            for (j = -y; j < y; j++)
            {
                /* 在指定区域画点，填充部分椭圆 */
                LCD_DrawPoint(X + x, Y + j);
                LCD_DrawPoint(X - x, Y + j);
            }
        }

        /* 画椭圆中间部分圆弧 */
        LCD_DrawPoint(X + x, Y + y);
        LCD_DrawPoint(X - x, Y - y);
        LCD_DrawPoint(X - x, Y + y);
        LCD_DrawPoint(X + x, Y - y);
    }

    /* 画椭圆两侧部分 */
    d2 = b * b * (x + 0.5) * (x + 0.5) + a * a * (y - 1) * (y - 1) - a * a * b * b;

    while (y > 0)
    {
        if (d2 <= 0) // 下一个点在当前点东方
        {
            d2 += b * b * (2 * x + 2) + a * a * (-2 * y + 3);
            x++;
        }
        else // 下一个点在当前点东南方
        {
            d2 += a * a * (-2 * y + 3);
        }
        y--;

        if (LCD.MonoFill == LCD_Cover) // 指定椭圆填充
        {
            /* 遍历两侧部分 */
            for (j = -y; j < y; j++)
            {
                /* 在指定区域画点，填充部分椭圆 */
                LCD_DrawPoint(X + x, Y + j);
                LCD_DrawPoint(X - x, Y + j);
            }
        }

        /* 画椭圆两侧部分圆弧 */
        LCD_DrawPoint(X + x, Y + y);
        LCD_DrawPoint(X - x, Y - y);
        LCD_DrawPoint(X - x, Y + y);
        LCD_DrawPoint(X + x, Y - y);
    }
}

/* LCD画圆弧 */
void LCD_DrawArc(uint16_t X, uint16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle)
{
    int16_t x, y, d, j;

    /* 此函数借用Bresenham算法画圆的方法 */

    d = 1 - Radius;
    x = 0;
    y = Radius;

    /* 在画圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理 */
    if (LCD_IsInAngle(x, y, StartAngle, EndAngle))
    {
        LCD_DrawPoint(X + x, Y + y);
    }
    if (LCD_IsInAngle(-x, -y, StartAngle, EndAngle))
    {
        LCD_DrawPoint(X - x, Y - y);
    }
    if (LCD_IsInAngle(y, x, StartAngle, EndAngle))
    {
        LCD_DrawPoint(X + y, Y + x);
    }
    if (LCD_IsInAngle(-y, -x, StartAngle, EndAngle))
    {
        LCD_DrawPoint(X - y, Y - x);
    }

    if (LCD.MonoFill == LCD_Cover) // 指定圆弧填充
    {
        for (j = -y; j < y; j++)
        {
            if (LCD_IsInAngle(0, j, StartAngle, EndAngle))
            {
                LCD_DrawPoint(X, Y + j);
            }
        }
    }

    while (x < y) // 遍历X轴的每个点
    {
        x++;
        if (d < 0) // 下一个点在当前点东方
        {
            d += 2 * x + 1;
        }
        else // 下一个点在当前点东南方
        {
            y--;
            d += 2 * (x - y) + 1;
        }

        if (LCD_IsInAngle(x, y, StartAngle, EndAngle))
        {
            LCD_DrawPoint(X + x, Y + y);
        }
        if (LCD_IsInAngle(y, x, StartAngle, EndAngle))
        {
            LCD_DrawPoint(X + y, Y + x);
        }
        if (LCD_IsInAngle(-x, -y, StartAngle, EndAngle))
        {
            LCD_DrawPoint(X - x, Y - y);
        }
        if (LCD_IsInAngle(-y, -x, StartAngle, EndAngle))
        {
            LCD_DrawPoint(X - y, Y - x);
        }
        if (LCD_IsInAngle(x, -y, StartAngle, EndAngle))
        {
            LCD_DrawPoint(X + x, Y - y);
        }
        if (LCD_IsInAngle(y, -x, StartAngle, EndAngle))
        {
            LCD_DrawPoint(X + y, Y - x);
        }
        if (LCD_IsInAngle(-x, y, StartAngle, EndAngle))
        {
            LCD_DrawPoint(X - x, Y + y);
        }
        if (LCD_IsInAngle(-y, x, StartAngle, EndAngle))
        {
            LCD_DrawPoint(X - y, Y + x);
        }

        if (LCD.MonoFill == LCD_Cover) // 指定圆弧填充
        {
            for (j = -y; j < y; j++)
            {
                /*在填充圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
                if (LCD_IsInAngle(x, j, StartAngle, EndAngle))
                {
                    LCD_DrawPoint(X + x, Y + j);
                }
                if (LCD_IsInAngle(-x, j, StartAngle, EndAngle))
                {
                    LCD_DrawPoint(X - x, Y + j);
                }
            }

            for (j = -x; j < x; j++)
            {
                /*在填充圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
                if (LCD_IsInAngle(-y, j, StartAngle, EndAngle))
                {
                    LCD_DrawPoint(X - y, Y + j);
                }
                if (LCD_IsInAngle(y, j, StartAngle, EndAngle))
                {
                    LCD_DrawPoint(X + y, Y + j);
                }
            }
        }
    }
}

/* LCD初始化 */
void LCD_Init(void)
{
    LCD_GPIO_Init(); // 初始化GPIO
    LCD_SPI_Init();  // 初始化SPI1

    LCD_WR_RES(0);    // 复位
    Delay_ms(100);    // 延时
    LCD_WR_RES(1);    // 释放复位
    Delay_ms(100);    // 延时
    LCD_WR_BLK(1);    // 打开背光
    Delay_ms(120);    // 延时
    LCD_WR_REG(0x11); // 睡眠模式关闭
    Delay_ms(1200);   // 延时

#if LCD_DEVICE == 1 || LCD_DEVICE == 2 || LCD_DEVICE == 3 || LCD_DEVICE == 4
    LCD_WR_REG(0x3A);   // 设置像素格式
    LCD_WR_DATA8(0x05); // 设置为18位RGB格式

    LCD_WR_REG(0xB2);   // 设置帧速率控制
    LCD_WR_DATA8(0x0C); // 设置帧速率控制参数
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x33);
    LCD_WR_DATA8(0x33);

    LCD_WR_REG(0xB7);   // 设置输出控制
    LCD_WR_DATA8(0x35); // 设置输出控制参数

    LCD_WR_REG(0xBB);   // 设置VCOM控制
    LCD_WR_DATA8(0x19); // 设置VCOM控制参数

    LCD_WR_REG(0xC2);   // 设置功率控制
    LCD_WR_DATA8(0x01); // 设置功率控制参数

    LCD_WR_REG(0xC0);   // 设置电源控制
    LCD_WR_DATA8(0x2C); // 设置电源控制参数

    LCD_WR_REG(0xC3);   // 设置功率控制
    LCD_WR_DATA8(0x12); // 设置功率控制参数

    LCD_WR_REG(0xC4);   // 设置功率控制
    LCD_WR_DATA8(0x20); // 设置功率控制参数

    LCD_WR_REG(0xC6);   // 设置功率控制
    LCD_WR_DATA8(0x0F); // 设置功率控制参数

    LCD_WR_REG(0xD0);   // 设置功率控制
    LCD_WR_DATA8(0xA4); // 设置功率控制参数
    LCD_WR_DATA8(0xA1); // 设置功率控制参数

    LCD_WR_REG(0xE0);   // 设置正伽马控制
    LCD_WR_DATA8(0xD0); // 设置正伽马控制参数
    LCD_WR_DATA8(0x04);
    LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x11);
    LCD_WR_DATA8(0x13);
    LCD_WR_DATA8(0x2B);
    LCD_WR_DATA8(0x3F);
    LCD_WR_DATA8(0x54);
    LCD_WR_DATA8(0x4C);
    LCD_WR_DATA8(0x18);
    LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x0B);
    LCD_WR_DATA8(0x1F);
    LCD_WR_DATA8(0x23);

    LCD_WR_REG(0xE1);   // 设置负伽马控制
    LCD_WR_DATA8(0xD0); // 设置负伽马控制参数
    LCD_WR_DATA8(0x04);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x11);
    LCD_WR_DATA8(0x13);
    LCD_WR_DATA8(0x2C);
    LCD_WR_DATA8(0x3F);
    LCD_WR_DATA8(0x44);
    LCD_WR_DATA8(0x51);
    LCD_WR_DATA8(0x2F);
    LCD_WR_DATA8(0x1F);
    LCD_WR_DATA8(0x1F);
    LCD_WR_DATA8(0x20);
    LCD_WR_DATA8(0x23);

#if LCD_DEVICE == 1 || LCD_DEVICE == 2 || LCD_DEVICE == 3
    LCD_WR_REG(0x21); // 设置内存访问方向
#else
    LCD_WR_REG(0x20); // 设置内存访问方向
#endif
    LCD_WR_REG(0x29); // 正常显示模式
#else
    //------------------------------------ST7735S Frame Rate-----------------------------------------//
    LCD_WR_REG(0xB1);
    LCD_WR_DATA8(0x05);
    LCD_WR_DATA8(0x3C);
    LCD_WR_DATA8(0x3C);
    LCD_WR_REG(0xB2);
    LCD_WR_DATA8(0x05);
    LCD_WR_DATA8(0x3C);
    LCD_WR_DATA8(0x3C);
    LCD_WR_REG(0xB3);
    LCD_WR_DATA8(0x05);
    LCD_WR_DATA8(0x3C);
    LCD_WR_DATA8(0x3C);
    LCD_WR_DATA8(0x05);
    LCD_WR_DATA8(0x3C);
    LCD_WR_DATA8(0x3C);
    //------------------------------------End ST7735S Frame Rate---------------------------------//
    LCD_WR_REG(0xB4); // Dot inversion
    LCD_WR_DATA8(0x03);
    //------------------------------------ST7735S Power Sequence---------------------------------//
    LCD_WR_REG(0xC0);
    LCD_WR_DATA8(0x28);
    LCD_WR_DATA8(0x08);
    LCD_WR_DATA8(0x04);
    LCD_WR_REG(0xC1);
    LCD_WR_DATA8(0XC0);
    LCD_WR_REG(0xC2);
    LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xC3);
    LCD_WR_DATA8(0x8D);
    LCD_WR_DATA8(0x2A);
    LCD_WR_REG(0xC4);
    LCD_WR_DATA8(0x8D);
    LCD_WR_DATA8(0xEE);
    //---------------------------------End ST7735S Power Sequence-------------------------------------//
    LCD_WR_REG(0xC5); // VCOM
    LCD_WR_DATA8(0x1A);
    //------------------------------------ST7735S Gamma Sequence---------------------------------//
    LCD_WR_REG(0xE0);
    LCD_WR_DATA8(0x04);
    LCD_WR_DATA8(0x22);
    LCD_WR_DATA8(0x07);
    LCD_WR_DATA8(0x0A);
    LCD_WR_DATA8(0x2E);
    LCD_WR_DATA8(0x30);
    LCD_WR_DATA8(0x25);
    LCD_WR_DATA8(0x2A);
    LCD_WR_DATA8(0x28);
    LCD_WR_DATA8(0x26);
    LCD_WR_DATA8(0x2E);
    LCD_WR_DATA8(0x3A);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x01);
    LCD_WR_DATA8(0x03);
    LCD_WR_DATA8(0x13);
    LCD_WR_REG(0xE1);
    LCD_WR_DATA8(0x04);
    LCD_WR_DATA8(0x16);
    LCD_WR_DATA8(0x06);
    LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x2D);
    LCD_WR_DATA8(0x26);
    LCD_WR_DATA8(0x23);
    LCD_WR_DATA8(0x27);
    LCD_WR_DATA8(0x27);
    LCD_WR_DATA8(0x25);
    LCD_WR_DATA8(0x2D);
    LCD_WR_DATA8(0x3B);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x01);
    LCD_WR_DATA8(0x04);
    LCD_WR_DATA8(0x13);

    //------------------------------------End ST7735S Gamma Sequence-----------------------------//
    LCD_WR_REG(0x3A); // 65k mode
    LCD_WR_DATA8(0x05);
    LCD_WR_REG(0x29); // Display on
#endif
#if LCD_DEVICE == 3 || LCD_DEVICE == 4
    LCD_SetDirection(LCD_Horizontal);
#else
    LCD_SetDirection(LCD_Vertical); // 设置显示方向为垂直
#endif
    LCD_SetBackColor(LCD_BLACK);     // 设置背景颜色为黑色
    LCD_SetPaintColor(LCD_WHITE);    // 设置绘图颜色为白色
    LCD_SetFontSize(LCD_FontSize16); // 设置字体大小为16
    LCD_SetMonoFill(LCD_Cover);      // 设置透明绘制为覆盖模式
    LCD_Clear();                     // 清空多余显示
}
