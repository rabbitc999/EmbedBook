#include "menu.h"

// TODO: 用户头文件
#include <windows.h>

// TODO: 菜单UI必须包含的数据
UI_Driver MainUI;
uint8_t UI_FlushFlag = ENABLE;

// TODO: 添加数据的声明
int32_t A = 0;
float B = 0;
int32_t Radio = 0;
int32_t Check[10] = { 0 };

// TODO:添加函数菜单的声明 
void UI_Main(UI_MenuItem* sender, int16_t inputKey);
void UI_Image(UI_MenuItem* sender, int16_t inputKey);
void UI_MPU(UI_MenuItem* sender, int16_t inputKey);

void UI_Init(void)
{
    srand((unsigned int)time(NULL));
	UI_HardwareInit();
    UI_Read();
    UI_StartInit(&MainUI, UI_Main);

	// TODO:添加自己的菜单初始化
    UI_Menu* Menu_Root = UI_AddMenu(&MainUI, "主菜单");
    UI_Menu* Menu_Setting  = UI_AddMenu(&MainUI, "数据设置");
    UI_Menu* Menu_RadioBox = UI_AddMenu(&MainUI, "单选按钮");
    UI_Menu* Menu_CheckBox = UI_AddMenu(&MainUI, "多选按钮");
    UI_Menu* Menu_Function = UI_AddMenu(&MainUI, "函数项目");

    UI_MenuAddExitItem(Menu_Root, "返回上一级");
    UI_MenuAddJumpItem(Menu_Root, "数据设置", Menu_Setting);
    UI_MenuAddJumpItem(Menu_Root, "单选按钮", Menu_RadioBox);
    UI_MenuAddJumpItem(Menu_Root, "多选按钮", Menu_CheckBox);
    UI_MenuAddJumpItem(Menu_Root, "函数项目", Menu_Function);

    UI_MenuAddExitItem(Menu_Setting, "返回上一级");
    UI_MenuAddValueItem_Int(Menu_Setting, "整数变量", &A, 0, 100);
    UI_MenuAddValueItem_Float(Menu_Setting, "浮点数变量", &B, 0, 100);

    UI_MenuAddExitItem(Menu_RadioBox, "返回上一级");
    UI_MenuAddChooseItem(Menu_RadioBox, "选项0", &Radio, 0);
    UI_MenuAddChooseItem(Menu_RadioBox, "选项1", &Radio, 1);
    UI_MenuAddChooseItem(Menu_RadioBox, "选项2", &Radio, 2);
    UI_MenuAddChooseItem(Menu_RadioBox, "选项3", &Radio, 3);
    UI_MenuAddChooseItem(Menu_RadioBox, "选项4", &Radio, 4);
    UI_MenuAddChooseItem(Menu_RadioBox, "选项5", &Radio, 5);
    UI_MenuAddChooseItem(Menu_RadioBox, "选项6", &Radio, 6);
    UI_MenuAddChooseItem(Menu_RadioBox, "选项7", &Radio, 7);
    UI_MenuAddChooseItem(Menu_RadioBox, "选项8", &Radio, 8);
    UI_MenuAddChooseItem(Menu_RadioBox, "选项9", &Radio, 9);

    UI_MenuAddExitItem(Menu_CheckBox, "返回上一级");
    UI_MenuAddSelectItem(Menu_CheckBox, "选项0", &Check[0]);
    UI_MenuAddSelectItem(Menu_CheckBox, "选项1", &Check[1]);
    UI_MenuAddSelectItem(Menu_CheckBox, "选项2", &Check[2]);
    UI_MenuAddSelectItem(Menu_CheckBox, "选项3", &Check[3]);
    UI_MenuAddSelectItem(Menu_CheckBox, "选项4", &Check[4]);
    UI_MenuAddSelectItem(Menu_CheckBox, "选项5", &Check[5]);
    UI_MenuAddSelectItem(Menu_CheckBox, "选项6", &Check[6]);
    UI_MenuAddSelectItem(Menu_CheckBox, "选项7", &Check[7]);
    UI_MenuAddSelectItem(Menu_CheckBox, "选项8", &Check[8]);
    UI_MenuAddSelectItem(Menu_CheckBox, "选项9", &Check[9]);


    UI_MenuAddExitItem(Menu_Function, "返回上一级");
    UI_MenuAddFuncItem(Menu_Function, "智能车图像", UI_Image);
    UI_MenuAddFuncItem(Menu_Function, "陀螺仪解算", UI_MPU);

    UI_EndInit(&MainUI);

    printf("缓存已使用%d字节！\n", MainUI.MenuBufferTop);   // 这里打断点可以获取缓存使用的空间
}

/**
 * 主界面UI的循环
 * @param inputKey 输入的按键
 */
void UI_While(void)
{
    if (UI_FlushFlag != ENABLE) return;
	// TODO:添加自己的菜单循环函数
    UI_Run(&MainUI);
    UI_FlushFlag = DISABLE;
}

// TODO:以下是用户定义

/**
 * 主界面UI的定义
 * @param inputKey 输入的按键
 */
void UI_Main(UI_MenuItem* sender, int16_t inputKey)
{
    static uint8_t FirstEnter = ENABLE;
    if (FirstEnter == ENABLE)
    {
        UI_ShowString(0, 0, "主界面UI");
        UI_ShowString(0, 20, "按Enter进入菜单模式");
        UI_ShowString(0, 40, "对应于键盘的'Q'键");

        UI_ShowString(0, 80, "UI的操作:");
        UI_ShowString(0, 100, "'Q':进入");
        UI_ShowString(0, 120, "'A':退出");
        UI_ShowString(0, 140, "'W':菜单项+1或退出修改");
        UI_ShowString(0, 160, "'S':菜单项-1或退出修改");
        UI_ShowString(0, 180, "'E':修改+1");
        UI_ShowString(0, 200, "'D':修改-1");
        FirstEnter = DISABLE;
    }
    if (inputKey == UI_KEY_ENTER)
    {
        UI_StopRunFunc(&MainUI);
        FirstEnter = ENABLE;
    }
}

void UI_Image(UI_MenuItem* sender, int16_t inputKey)
{
    static uint8_t FirstEnter = ENABLE;
    // 显示消息框
    if (FirstEnter == ENABLE)
    {
        UI_ShowString(0, 0, "图像显示(假的)");
        UI_ShowString(0, 20, "按Exit进入菜单模式");
        UI_ShowString(0, 40, "对应于键盘的'A'键");
        FirstEnter = DISABLE;
    }

    // 显示消息框
    if (inputKey == UI_KEY_EXIT)
    {
        UI_StopRunFunc(&MainUI);
        FirstEnter = ENABLE;
    }
}

void UI_MPU(UI_MenuItem* sender, int16_t inputKey)
{
    static uint8_t FirstEnter = ENABLE;

    if (FirstEnter == ENABLE)
    {
        UI_ShowString(0, 20, "陀螺仪显示(假的)");
        UI_ShowString(0, 40, "按Exit进入菜单模式");
        UI_ShowString(0, 60, "对应于键盘的'A'键");
        FirstEnter = DISABLE;
    }

    // 显示消息框
    if (inputKey == UI_KEY_EXIT)
    {
        UI_StopRunFunc(&MainUI);
        FirstEnter = ENABLE;
    }
    else
    {
        UI_ShowString(0, 0, "MPU=");
        UI_ShowInt(4 * UI_CHAR_SIZE_WIDTH, 0, rand());
    }
}

/**
 * UI读取变量
 * @note 需要用户自己实现，并且要包括UI出现的所有变量
 */
void UI_Read(void)
{
//    // Widows没有找到好的存储方案，这个代码是废止的
//    uint32_t dat[512] = { 0 };       // 声明 512*4=2048字节的文件缓存
//    // EEPROM文件不存在，重新创建
//    if (_access("EEPROM.BIN", 0) == -1)
//    {
//        FILE* fp = fopen("EEPROM.BIN", "w");
//        if (fp != NULL)
//        {
//            // 写入数组
//            fwrite(dat, sizeof(uint32_t), 512, fp);
//            fclose(fp);
//        }
//    }
//    FILE* fp = fopen("EEPROM.BIN", "r");
//    uint32_t buffer[512];
//    assert(fp != NULL);
//    fread(&buffer, sizeof(uint32_t), 512, fp);
// 
//    A = UI_TypeTran(buffer[0], int32_t);
//    B = UI_TypeTran(buffer[1], float);
//    Radio = UI_TypeTran(buffer[2], int32_t);
//    memcpy(Check, buffer + 3, sizeof(int32_t) * 10);
//
//    fclose(fp);
}

/**
 * UI保存变量
 * @param val 需要保存的变量地址
 * @note 需要用户自己实现，并且要包括UI出现的所有变量
 */
void UI_Save(uint32_t* val)
{
//    // Widows没有找到好的存储方案，这个代码是废止的
//    FILE* fp = fopen("EEPROM.BIN", "w");
//    size_t index = 0, count = 0;
//    assert(fp != NULL);
//    if (val == UI_TypeTranPtr(&A, uint32_t)) { index = 0; count = 1; }
//    if (val == UI_TypeTranPtr(&B, uint32_t)) { index = 1; count = 1; }
//    if (val == UI_TypeTranPtr(&Radio, uint32_t)) { index = 2; count = 1; }
//    if (val - Check < 10) { index = 3; count = 10; val = Check; }   // 数组指针需要重新定向
//    fseek(fp, 4 * index, SEEK_SET);
//    fwrite(val, sizeof(int32_t), count, fp);
//    fclose(fp);
}
