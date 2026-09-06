/**
 * 单片机环境的特殊性：
 * 1. 使用malloc/free会造成内存碎片
 * 2. 使用c++会导致程序的不稳定性
 * 3. 使用LVGL的大型UI库并不适合于新手
 *
 * 菜单分为几个文件：
 * 1. menu_port.c   目的是定义菜单的接口函数
 * 2. menu.c/h      目的是定义菜单的主操作方法
 * 3. menu_user.c   目的是定义用户创建菜单的过程
 *
 * 作者：糖糖爱吹泡泡
 */

#include <math.h>
#include "menu.h"

// TODO: 用户头文件
#include "hardware.h"

//**********************************************************************************************
//*******************************************用户定义*******************************************
//**********************************************************************************************
/**
 * UI硬件初始化函数
 * @note 需要用户自己实现
 */
void UI_HardwareInit(void)
{
    LCD_Init();
}

/**
 * UI清除界面
 * @note 需要用户自己实现
 */
void UI_Clear(void)
{
    LCD_Clear();
}

/**
 * UI显示字符串
 * @param x 横坐标
 * @param y 纵坐标
 * @param str 显示的字符串
 * @note 需要用户自己实现
 */
void UI_ShowString(uint16_t x, uint16_t y, const char* str)
{
    LCD_ShowString(x, y, str);
}

/**
 * UI获取键盘事件
 * @param event 需要获取的事件类型
 * @retrun 获取的事件
 * @note 需要用户自己实现
 */
int8_t UI_GetKeyEvent(MenuEvent menuEvent)
{
    
    // 按键处理函数
    switch (menuEvent) {
    case UI_MENU_EVENT_ENTER:  // 进入事件，返回 ENABLE/DISABLE
    {
        if (Key_GetNum() == UI_KEY_ENTER) { Key_Clear(); return ENABLE; }
        return DISABLE;
    }
    case UI_MENU_EVENT_EXIT:   // 退出事件，返回 ENABLE/DISABLE
    {
        if (Key_GetNum() == UI_KEY_EXIT) { Key_Clear(); return ENABLE; }
        return DISABLE;
    }
    case UI_MENU_EVENT_ITEM:   // 数值修改事件，返回修改偏移量
    {
        if (Key_GetNum() == UI_KEY_ITEM_DOWN) { Key_Clear(); return 1; }
        if (Key_GetNum() == UI_KEY_ITEM_UP) { Key_Clear(); return -1; }
        return 0;
    }
    case UI_MENU_EVENT_VALUE:  // 项目修改事件，返回修改偏移量
    {
        if (Key_GetNum() == UI_KEY_VAL_UP) { Key_Clear(); return 1; }
        if (Key_GetNum() == UI_KEY_VAL_DOWN) { Key_Clear(); return -1; }
        return 0;
    }
    default:
    {
        return DISABLE;
    }
    }
}

/**
 * UI按键扫描函数
 * @return 返回按键扫描的状态
 * @arg ENABLE 按键有扫描数据
 * @arg DISENABLE 按键没有扫描数据
 * @note 需要用户自己实现
 */
uint8_t UI_KeyScan(void)
{
    return Key_Scan();
}

/**
 * UI按键获取函数
 * @return 返回按键扫描的状态
 * @arg ENABLE 按键有扫描数据
 * @arg DISENABLE 按键没有扫描数据
 * @note 需要用户自己实现
 */
uint8_t UI_GetKeyValue(void)
{
    uint8_t temp = Key_GetNum();
    Key_Clear();
    return temp;
}

//**********************************************************************************************
//*******************************************程序定义*******************************************
//**********************************************************************************************

/**
 * UI显示整数
 * @param x 横坐标
 * @param y 纵坐标
 * @param num 显示的整数
 */
void UI_ShowInt(uint16_t x, uint16_t y, int32_t num)
{
    char tmp[UI_MENU_VALUE_LEN + 1];
    const uint8_t numLen = UI_MENU_VALUE_LEN - 1;
    int32_t power = (int32_t)round(pow(10, numLen)); // 初始值
    uint8_t zf = 0, prev_zf = 0, temp, ch, sign = 0, i;

    if (num < 0) {
        sign = 1;
        num = -num;
    }

    tmp[0] = ' ';
    for (i = 0; i < numLen; i++) {
        power /= 10;             // 减少power的值
        temp = num / power % 10; // 计算当前位的数字
        if (temp > 0 || zf == 1) {
            zf = 1;              // 标记至少有一个非零位
            ch = temp + '0';     // 将数字转换为字符并填充
        }
        else {
            ch = ' ';        // 填充空格
        }
        if (prev_zf == 0 && zf == 1 && sign == 1) { // 如果前一个零标志为0且当前非零
            tmp[i] = '-';
        }
        tmp[i + 1] = ch;  // 将计算的数字放入tmp数组
        prev_zf = zf;     // 更新前一个零标志
    }
    tmp[numLen + 0] = num % 10 + '0';   // 处理个位数字
    tmp[numLen + 1] = '\0';             // 确保字符串以null结尾

    UI_ShowString(x, y, tmp);
}

/**
 * UI显示单精度浮点数
 * @param x 横坐标
 * @param y 纵坐标
 * @param num 显示的单精度浮点数
 */
void UI_ShowFloat(uint16_t x, uint16_t y, float num)
{
    char tmp[UI_MENU_VALUE_LEN + 1];
    const uint8_t numLen = UI_MENU_VALUE_LEN - 2;
    int32_t tmp_num = (int32_t)round(num * 100);
    int32_t power = (int32_t)round(pow(10, numLen)); // 初始值
    uint8_t zf = 0, prev_zf = 0, temp, ch, sign = 0, i;

    if (tmp_num < 0) {
        sign = 1;
        tmp_num = -tmp_num;
    }

    tmp[0] = ' ';

    for (i = 0; i < numLen - 2; i++) {
        power /= 10;
        temp = tmp_num / power % 10;
        if (temp > 0 || zf == 1) {
            zf = 1;          // 标记至少有一个非零位
            ch = temp + '0'; // 将数字转换为字符并填充
        }
        else {
            ch = ' ';     // 填充空格
        }
        if (prev_zf == 0 && zf == 1 && sign == 1) { // 如果前一个零标志为0且当前非零
            tmp[i] = '-'; // 插入负号
        }
        tmp[i + 1] = ch;  // 将计算的数字放入tmp数组
        prev_zf = zf;     // 更新前一个零标志
    }
    tmp[numLen - 2] = tmp_num / 100 % 10 + '0'; // 处理小数点前的个位数字
    tmp[numLen - 1] = '.';
    tmp[numLen + 0] = tmp_num / 10 % 10 + '0';  // 处理小数点后的第一位数
    tmp[numLen + 1] = tmp_num % 10 + '0';       // 处理小数点后的第二位数
    tmp[numLen + 2] = '\0';                     // 确保字符串以null结尾

    UI_ShowString(x, y, tmp);
}
