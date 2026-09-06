#include "bsp_periph.h"

/**
 * @brief  使能指定 GPIO 端口的时钟
 * @param  port: GPIO 端口（如 GPIOA）
 * @retval 无
 * @note   仅内部调用，自动匹配端口对应的时钟使能函数
 */
void BSP_GPIO_ClkEnable(GPIO_TypeDef *port)
{
    /* 参数有效性校验 */
    if (port == NULL)
    {
        return;
    }

    if (port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    else if (port == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
    else if (port == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
    else if (port == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }
    else if (port == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    }
}

/**
 * @brief  使能指定 GPIO 端口的时钟
 * @param  port: GPIO 端口（如 GPIOA）
 * @retval 无
 * @note   仅内部调用，自动匹配端口对应的时钟使能函数
 */
void BSP_GPIO_ClkDisable(GPIO_TypeDef *port)
{
    /* 参数有效性校验 */
    if (port == NULL)
    {
        return;
    }

    if (port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_DISABLE();
    }
    else if (port == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_DISABLE();
    }
    else if (port == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_DISABLE();
    }
    else if (port == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_DISABLE();
    }
    else if (port == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_DISABLE();
    }
}

/**
 * @brief  使能指定定时器的外设时钟
 * @param  timInstance: 定时器实例指针（如TIM1、TIM8等）
 * @retval 无
 * @note   1. 自动匹配定时器对应的RCC时钟使能函数；
 *         2. 支持STM32H7系列常用定时器（TIM1~TIM17，STM32H7没有TIM9，TIM10，TIM11定时器）；
 *         3. 重复调用无副作用（RCC时钟使能可多次执行）
 */
void BSP_TIM_ClkEnable(TIM_TypeDef *timInstance)
{
    /* 参数有效性校验 */
    if (timInstance == NULL)
    {
        return;
    }

    if (timInstance == TIM1)
    {
        __HAL_RCC_TIM1_CLK_ENABLE();
    }
    else if (timInstance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
    else if (timInstance == TIM3)
    {
        __HAL_RCC_TIM3_CLK_ENABLE();
    }
    else if (timInstance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_ENABLE();
    }
}

/**
 * @brief  使能指定I2C外设的时钟
 * @param  i2cInstance: I2C实例指针（如I2C1、I2C2等）
 * @retval 无
 * @note   1. 自动匹配I2C对应的RCC时钟使能函数；
 *         2. 支持STM32H7系列常用I2C（I2C1~I2C4）；
 *         3. 重复调用无副作用（RCC时钟使能可多次执行）
 */
void BSP_I2C_ClkEnable(I2C_TypeDef *i2cInstance)
{
    /* 参数有效性校验 */
    if (i2cInstance == NULL)
    {
        return;
    }

    if (i2cInstance == I2C1)
    {
        __HAL_RCC_I2C1_CLK_ENABLE();
    }
    else if (i2cInstance == I2C2)
    {
        __HAL_RCC_I2C2_CLK_ENABLE();
    }
}

/**
 * @brief  使能指定SPI外设的时钟
 * @param  spiInstance: SPI实例指针（如SPI1、SPI2等）
 * @retval 无
 * @note   1. 自动匹配SPI对应的RCC时钟使能函数；
 *         2. 支持STM32H7系列常用SPI（SPI1~SPI6）；
 *         3. 重复调用无副作用（RCC时钟使能可多次执行）
 */
void BSP_SPI_ClkEnable(SPI_TypeDef *spiInstance)
{
    /* 参数有效性校验 */
    if (spiInstance == NULL)
    {
        return;
    }

    if (spiInstance == SPI1)
    {
        __HAL_RCC_SPI1_CLK_ENABLE();
    }
    else if (spiInstance == SPI2)
    {
        __HAL_RCC_SPI2_CLK_ENABLE();
    }
}

/**
 * @brief  使能指定串口外设的时钟
 * @param  usartInstance: 串口实例指针（如USART1、UART4等）
 * @retval 无
 * @note   1. 自动匹配串口对应的RCC时钟使能函数，兼容USART/UART前缀实例；
 *         2. 支持STM32H7系列常用串口（USART1~USART3、UART4~UART8）；
 *         3. 重复调用无副作用（RCC时钟使能可多次执行）；
 *         4. 仅支持硬件串口，不包含虚拟串口（如USB转串口）
 */
void BSP_USART_ClkEnable(USART_TypeDef *usartInstance)
{
    /* 参数有效性校验 */
    if (usartInstance == NULL)
    {
        return;
    }

    if (usartInstance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
    }
    else if (usartInstance == USART2)
    {
        __HAL_RCC_USART2_CLK_ENABLE();
    }
    else if (usartInstance == USART3)
    {
        __HAL_RCC_USART3_CLK_ENABLE();
    }
}

/**
 * @brief  使能指定DMA控制器的时钟
 * @param  dmaInstance: DMA实例指针（如DMA1、DMA2）
 * @retval 无
 * @note   1. 支持STM32H7常用DMA控制器（DMA1、DMA2）；
 *         2. 重复调用无副作用
 */
void BSP_DMA_ClkEnable(DMA_TypeDef *dmaInstance)
{
    if (dmaInstance == NULL)
    {
        return;
    }

    if (dmaInstance == DMA1)
    {
        __HAL_RCC_DMA1_CLK_ENABLE();
    }
}

/**
 * @brief 获取GPIO端口号
 */
static inline GPIO_TypeDef *BSP_GPIO_GetPort(GPIO_IO_T IO)
{
    switch (IO & 0xFFFF0000)
    {
    case GPIO_A:
        return GPIOA;
    case GPIO_B:
        return GPIOB;
    case GPIO_C:
        return GPIOC;
    default:
        return NULL;  // 无效端口
    }
}

/**
 * @brief  GPIO引脚初始化函数
 * @param  IO: GPIO引脚枚举值（如PA1、PB5等）
 * @param  mode: GPIO模式（如INPUT_UP）
 */
void BSP_GPIO_Init(GPIO_IO_T IO, GPIO_MODE_T mode)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    /* 参数有效性校验 */
    GPIO_TypeDef *PORT = BSP_GPIO_GetPort(IO);
    uint16_t PIN = IO & 0xFFFF;  // 提取引脚号
    if (PORT == NULL)
    {
        return;
    }

    /* 使能GPIO时钟 */
    if (PORT == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    if (PORT == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
    if (PORT == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
    if (PORT == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }
    if (PORT == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    }

    /* 初始化GPIO引脚 */
    GPIO_InitStruct.Pin = PIN;
    switch (mode)
    {
    case INPUT_U:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case INPUT_D:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case INPUT_F:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUTPUT_PP:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUTPUT_OD_U:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUTPUT_OD_D:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUTPUT_OD_F:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case ANALOG:
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        break;
    default:
        break;
    }

    HAL_GPIO_Init(PORT, &GPIO_InitStruct);
}

/**
 * @brief  GPIO引脚初始化函数
 * @param  IO: GPIO引脚枚举值（如PA1、PB5等）
 * @param  mode: GPIO模式（如INPUT_UP）
 * @param  alternate: GPIO复用功能（如GPIO_AF0_RTC_50Hz）
 */
void BSP_GPIO_InitEx(GPIO_IO_T IO, GPIO_MODE_T mode, uint8_t alternate)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    /* 参数有效性校验 */
    GPIO_TypeDef *PORT = BSP_GPIO_GetPort(IO);
    uint16_t PIN = IO & 0xFFFF;  // 提取引脚号
    if (PORT == NULL)
    {
        return;
    }

    /* 使能GPIO时钟 */
    if (PORT == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    if (PORT == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
    if (PORT == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
    if (PORT == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }
    if (PORT == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    }

    /* 初始化GPIO引脚 */
    GPIO_InitStruct.Pin = PIN;
    switch (mode)
    {
    case INPUT_U:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case INPUT_D:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case INPUT_F:
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUTPUT_PP:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUTPUT_OD_U:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUTPUT_OD_D:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUTPUT_OD_F:
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        break;
    case OUTPUT_AF_PP:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        // GPIO_InitStruct.Alternate = alternate;
        break;
    case OUTPUT_AF_OD_D:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        // GPIO_InitStruct.Alternate = alternate;
        break;
    case OUTPUT_AF_OD_U:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        // GPIO_InitStruct.Alternate = alternate;
        break;
    case OUTPUT_AF_OD_F:
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        // GPIO_InitStruct.Alternate = alternate;
        break;
    case ANALOG:
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        break;
    default:
        break;
    }

    HAL_GPIO_Init(PORT, &GPIO_InitStruct);
}

/**
 * @brief  GPIO引脚写入函数
 * @param  IO: GPIO引脚枚举值（如PA1、PB5等）
 * @param  state: GPIO引脚状态（GPIO_PIN_SET或GPIO_PIN_RESET）
 * @note 不检测参数有效性，调用前请确保参数正确
 */
void BSP_GPIO_WritePin(GPIO_IO_T IO, uint8_t state)
{
    GPIO_TypeDef *PORT = BSP_GPIO_GetPort(IO);
    uint16_t PIN = IO & 0xFFFF;  // 提取引脚号
    PORT->BSRR = ((state == GPIO_PIN_SET) ? PIN : (PIN << 16));
}

/**
 * @brief  GPIO引脚读取函数
 * @param  IO: GPIO引脚枚举值（如PA1、PB5等）
 * @retval GPIO引脚状态（1-高电平，0-低电平）
 * @note 不检测参数有效性，调用前请确保参数正确
 */
uint8_t BSP_GPIO_ReadPin(GPIO_IO_T IO)
{
    GPIO_TypeDef *PORT = BSP_GPIO_GetPort(IO);
    uint16_t PIN = IO & 0xFFFF;  // 提取引脚号
    return (PORT->IDR & PIN) ? 1 : 0;
}

/**
 * @brief  GPIO引脚翻转函数
 * @param  IO: GPIO引脚枚举值（如PA1、PB5等）
 * @retval GPIO引脚新状态（1-高电平，0-低电平）
 * @note 不检测参数有效性，调用前请确保参数正确
 */
void BSP_GPIO_TogglePin(GPIO_IO_T IO)
{
    __set_PRIMASK(1);
    GPIO_TypeDef *PORT = BSP_GPIO_GetPort(IO);
    uint16_t PIN = IO & 0xFFFF;  // 提取引脚号
    uint16_t ODR = PORT->ODR;
    PORT->BSRR = ((ODR & PIN) << 16) | (~ODR & PIN);
    __set_PRIMASK(0);
}

/**
 * @brief  GPIO引脚获取函数
 * @param  PORT: GPIO端口（如GPIOA）
 * @param  MASK: GPIO引脚掩码（如GPIO_PIN_0 | GPIO_PIN_1）
 * @retval GPIO引脚16位状态（1-高电平，0-低电平）
 * @note 不检测参数有效性，调用前请确保参数正确
 */
uint16_t BSP_GPIO_ReadPinEx(GPIO_TypeDef *PORT, uint16_t MASK)
{
    return PORT->IDR & MASK;
}

/**
 * @brief  GPIO引脚写入函数
 * @param  PORT: GPIO端口（如GPIOA）
 * @param  MASK: GPIO引脚掩码（如GPIO_PIN_0 | GPIO_PIN_1）
 * @param  STATE: GPIO 16位引脚状态（1-高电平，0-低电平）
 * @note 不检测参数有效性，调用前请确保参数正确
 */
void BSP_GPIO_WritePinEx(GPIO_TypeDef *PORT, uint16_t MASK, uint16_t STATE)
{
    __set_PRIMASK(1);
    uint16_t ODR = PORT->ODR;
    PORT->BSRR = (((~ODR & MASK) & STATE) << 16) | (((ODR & MASK) & ~STATE));
    __set_PRIMASK(0);
}
