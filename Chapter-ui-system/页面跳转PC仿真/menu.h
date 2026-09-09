/**
 * 单片机环境的特殊性：
 * 1. 使用malloc/free会造成内存碎片
 * 2. 使用c++会导致程序的不稳定性
 * 3. 使用LVGL的大型UI库并不适合于新手
 *
 * 菜单分为几个文件：
 * 1. menu_port.c/h 目的是定义菜单的接口函数
 * 2. menu.c/h      目的是定义菜单的主操作方法
 * 3. menu_user.c/h 目的是定义用户创建菜单的过程
 *
 * 本项目解决的痛点：菜单的静态创建，使用缓冲区
 *
 * 作者：糖糖爱吹泡泡
 */

#ifndef __MENU_H
#define __MENU_H

//**********************************************************************************************
//********************************************头文件********************************************
//**********************************************************************************************
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

//**********************************************************************************************
//********************************************宏定义********************************************
//**********************************************************************************************

// 标志位说明定义
#define ENABLE 1
#define DISABLE 0

// UI信息
#define UI_SIZE_WIDTH            240       // UI显示宽度
#define UI_SIZE_HEIGHT           300       // UI显示高度
#define UI_LINE_INTERVAL         20        // UI每一行显示间隔
#define UI_CHAR_SIZE_WIDTH       8         // ASCII字符的显示宽度
#define UI_CHAR_SIZE_HEIGHT      16        // ASCII字符的显示高度

// 外围硬件信息
#define UI_KEYNUM                6         // 按键个数
#define UI_HAVE_ENCODER          0         // 是否有编码器

// 菜单空间分配
#define UI_MENU_STACK_SIZE       4         // 菜单调用栈
#define UI_MENU_BUFFER_SIZE      4096      // 菜单缓冲区大小 (字节)

// 菜单上的显示字符串
#define UI_MENU_ITEM_CHOOSE_STR      "->"      // 项目选中
#define UI_MENU_ITEM_NOT_CHOOSE_STR  "- "      // 项目未选中
#define UI_MENU_ITEM_CHOOSE_STR_LEN  2         // 项目选中/未选中字符串的长度
#define UI_MENU_JUMP_ITEM_STR        "[跳转]"  // 跳转项目显示的字符串
#define UI_MENU_JUMP_ITEM_STR_LEN    6         // 跳转项目字符串长度
#define UI_MENU_EXIT_ITEM_STR        "[退出]"  // 退出项目显示的字符串
#define UI_MENU_EXIT_ITEM_STR_LEN    6         // 退出项目字符串长度
#define UI_MENU_FUNC_ITEM_STR        "[功能]"  // 函数项目显示的字符串
#define UI_MENU_FUNC_ITEM_STR_LEN    6         // 函数项目字符串长度
#define UI_MENU_RADIO_NONE_STR       "[ ]"     // 单选项目显示的未选中字符串
#define UI_MENU_RADIO_SEL_STR        "[*]"     // 单选项目显示的选中字符串
#define UI_MENU_RADIO_STR_LEN        3         // 单选项目显示的字符串长度
#define UI_MENU_CHECK_NONE_STR       "[ ]"     // 多选项目显示的未选中字符串
#define UI_MENU_CHECK_SEL_STR        "[+]"     // 多选项目显示的选中字符串
#define UI_MENU_CHECK_STR_LEN        3         // 多选项目显示的字符串长度
#define UI_MENU_SPEARATE_CHAR        '-'       // 菜单分隔显示的字符
#define UI_MENU_VALUE_LEN            8         // 值数据长度 (包括符号)
#define UI_MENU_EDIT_MODE_STR        "数值>"   // 编辑模式字符串
#define UI_MENU_ITEM_MODE_STR        "项目>"   // 项目模式字符串
#define UI_MENU_MODE_STR_LEN         5         // 模式字符串的长度
#define UI_UNIT_MA_STR               "<01.00F" // 单位A显示字符串
#define UI_UNIT_MB_STR               "<10.00F" // 单位B显示字符串
#define UI_UNIT_MC_STR               "<00.10F" // 单位C显示字符串
#define UI_UNIT_MD_STR               "<00.01F" // 单位D显示字符串
#define UI_UNIT_STR_LEN              7         // 单位字符串的长度
#define UI_UNIT_MA_UNIT              01.00f    // 单位A步进值 (A/B为整数)
#define UI_UNIT_MB_UNIT              10.00f    // 单位B步进值
#define UI_UNIT_MC_UNIT              00.10f    // 单位C步进值 (C/D为小数)
#define UI_UNIT_MD_UNIT              00.01f    // 单位D步进值

// 系统生成宏定义
#define UI_MAX_CHAR_LINE             (UI_SIZE_HEIGHT / UI_LINE_INTERVAL)
#define UI_TypeTran(SRC,T)           *(T*)&SRC
#define UI_TypeTranPtr(SRC,T)        (T*)SRC

//**********************************************************************************************
//*******************************************类型定义*******************************************
//**********************************************************************************************

// 0. 按键宏定义
enum UI_KEY_SEL
{
    UI_KEY_NONE = '\0',
    UI_KEY_ENTER = 'Q',
    UI_KEY_EXIT = 'A',
    UI_KEY_ITEM_UP = 'W',
    UI_KEY_ITEM_DOWN = 'S',
    UI_KEY_VAL_UP = 'E',
    UI_KEY_VAL_DOWN = 'D',
};

// 1.枚举定义
typedef enum __UI_MenuChangeValueType
{
    UI_TYPE_INT,
    UI_TYPE_FLOAT
} UI_MenuChangeValueType;

typedef enum __UI_MenuChangeUnit
{
    UI_UNIT_MA,
    UI_UNIT_MB,
    UI_UNIT_MC,
    UI_UNIT_MD
} UI_MenuChangeUnit;

// 2.类型定义
struct _UI_MenuItem;
struct _UI_Menu;
struct _UI_MenuItemListNode;
struct _UI_MenuListNode;
struct _UI_Driver;
typedef struct _UI_MenuItem UI_MenuItem;
typedef struct _UI_Menu UI_Menu;
typedef struct _UI_MenuItemListNode UI_MenuItemListNode;
typedef struct _UI_MenuListNode UI_MenuListNode;
typedef struct _UI_Driver UI_Driver;

// 菜单函数事件
typedef void (*UI_FuncEvent)(UI_MenuItem* sender, int16_t inputKey);
typedef void (*UI_Event)(UI_MenuItem* sender);

struct _UI_MenuItem                  // 所有菜单项目的类
{
    const char* ItemTitle;           // 选项标题字符串
    UI_Menu* BaseMenu;               // 基于的父菜单

    UI_FuncEvent DrawDetail;
    UI_FuncEvent Change;
    UI_Event Enter;
    UI_Event Exit;

    union _ChildUnion
    {
        UI_Menu* Jump;                   // 需要跳转的菜单
        UI_FuncEvent Func;               // 需要进入的函数
        uint32_t* BindPtr;               // 菜单变量绑定

        struct _ValueItem
        {
            uint32_t* BindPtr;               // 菜单变量绑定
            uint32_t Min;                    // 修改下限
            uint32_t Max;                    // 修改上限
            UI_MenuChangeValueType Type;     // 变量修改类型
        } Value;
        struct _ChooseItem
        {
            uint32_t* BindPtr;               // 菜单变量绑定
            int32_t Establish;               // 选择成立值
        } Choose;
    } Union;
};

struct _UI_MenuItemListNode           // 项目链表节点
{
    UI_MenuItem* Item;                // 当前项目
    UI_MenuItemListNode* pNext;       // 下一个项目
    UI_MenuItemListNode* pPrev;       // 上一个项目
    uint16_t Index;                   // 菜单所在的索引
};

struct _UI_Menu                              // 菜单
{
    const char* MenuTitle;                   // 选项标题字符串
    UI_MenuItemListNode* MenuItemList;       // 菜单列表
    UI_Driver* BaseUI;                        // 基于的父界面

    UI_MenuItemListNode* StartItemNode;      // 选项列表开始显示的项目节点
    UI_MenuItemListNode* SelectItemNode;     // 选项列表选中显示的项目节点

    uint16_t ItemCount;                      // 项目个数
    uint16_t CurrentIndex;                   // 当前项目索引
};

struct _UI_MenuListNode             // 菜单链表节点
{
    UI_Menu* Item;                  // 当前项目
    UI_MenuListNode* pNext;         // 下一个项目
};

struct _UI_Driver                            // UI驱动器
{
    uint16_t MenuBufferTop;                  // 菜单缓冲区使用的大小

    uint8_t MenuCount;                       // 菜单数目
    uint8_t StackTop;                        // 栈顶
    uint8_t UpdateAllFlag;                   // 所有项目更新标志
    uint8_t UpdateValueFlag;                 // 数值更新标志
    uint8_t ValueFlag;                       // 数字修改模式标志
    uint8_t FunctionRunFlag;                 // 执行函数标志
    uint8_t LastFunctionRunFlag;             // 上次执行函数标志
    uint8_t ChangeUnit;                      // 修改单位值

    UI_FuncEvent MainUI;                     // 主界面的函数
    UI_FuncEvent RunningFun;                 // 运行的函数

    UI_MenuListNode* MenuList;               // 菜单列表 (第0个为初始调用菜单)
    UI_Menu* MenuStack[UI_MENU_STACK_SIZE];  // 菜单栈
    uint8_t MenuBuffer[UI_MENU_BUFFER_SIZE]; // 菜单缓冲区
};

typedef enum __MenuEvent
{
    UI_MENU_EVENT_ENTER,  // 进入事件，返回 ENABLE/DISENABLE
    UI_MENU_EVENT_EXIT,   // 退出事件，返回 ENABLE/DISENABLE
    UI_MENU_EVENT_VALUE,  // 数值修改事件，返回修改偏移量
    UI_MENU_EVENT_ITEM,   // 项目修改事件，返回修改偏移量
} MenuEvent;

//**********************************************************************************************
//*******************************************函数定义*******************************************
//**********************************************************************************************

// 菜单的操作函数 (用户调用)
void UI_StartInit(UI_Driver* pthis, UI_FuncEvent mainUI);
void UI_EndInit(UI_Driver* pthis);
void UI_StopRunFunc(UI_Driver* pDirver);
void UI_Run(UI_Driver* pDirver);

// UI的读取与保存变量 (菜单UI或用户调用)
void UI_Save(uint32_t* val);
void UI_Read(void);

// UI的运行与循环函数 (用户调用)
void UI_Init(void);
void UI_While(void);

// 菜单的添加 (用户调用)
UI_Menu* UI_AddMenu(UI_Driver* pthis, const char* menuTitle);

// 菜单项的添加 (用户调用)
void UI_MenuAddJumpItem(UI_Menu* pMenu, const char* itemTitle, UI_Menu* jumpMenu);
void UI_MenuAddExitItem(UI_Menu* pMenu, const char* itemTitle);
void UI_MenuAddFuncItem(UI_Menu* pMenu, const char* itemTitle, UI_FuncEvent jumpFunction);
void UI_MenuAddValueItem_Int(UI_Menu* pMenu, const char* itemTitle, int32_t* bindValPtr, int32_t valueMin, int32_t valueMax);
void UI_MenuAddValueItem_Float(UI_Menu* pMenu, const char* itemTitle, float* bindValPtr, float valueMin, float valueMax);
void UI_MenuAddChooseItem(UI_Menu* pMenu, const char* itemTitle, int32_t* bindValPtr, int32_t establishValue);
void UI_MenuAddSelectItem(UI_Menu* pMenu, const char* itemTitle, int32_t* bindValPtr);

// 接口函数定义
void UI_HardwareInit(void);
void UI_Clear(void);

void UI_ShowString(uint16_t x, uint16_t y, const char* str);
void UI_ShowInt(uint16_t x, uint16_t y, int32_t num);
void UI_ShowFloat(uint16_t x, uint16_t y, float num);
uint8_t UI_KeyScan(void);
uint8_t UI_GetKeyValue(void);
int8_t UI_GetKeyEvent(MenuEvent event);

// 外部变量定义
extern uint8_t UI_FlushFlag;

// 菜单项创建定义
void UI_BaseItemChange(UI_MenuItem* pthis, int16_t delta);
void UI_BaseItemExit(UI_MenuItem* pthis);
void UI_JumpItemDrawDetail(UI_MenuItem* pthis, int16_t line);
void UI_JumpItemEnter(UI_MenuItem* pthis);
void UI_ExitItemDrawDetail(UI_MenuItem* pthis, int16_t line);
void UI_ExitItemEnter(UI_MenuItem* pthis);
void UI_FuncItemDrawDetail(UI_MenuItem* pthis, int16_t line);
void UI_FuncItemEnter(UI_MenuItem* pthis);
void UI_ValueItemDrawDetail(UI_MenuItem* pthis, int16_t line);
void UI_ValueItemEnter(UI_MenuItem* pthis);
void UI_ValueItemExit(UI_MenuItem* pthis);
void UI_ValueItemChange(UI_MenuItem* pthis, int16_t delta);
void UI_ChooseItemDrawDetail(UI_MenuItem* pthis, int16_t line);
void UI_ChooseItemEnter(UI_MenuItem* pthis);
void UI_ChooseItemExit(UI_MenuItem* pthis);
void UI_SelectItemDrawDetail(UI_MenuItem* pthis, int16_t line);
void UI_SelectItemEnter(UI_MenuItem* pthis);
void UI_SelectItemExit(UI_MenuItem* pthis);

#ifdef __cplusplus
};
#endif

#endif
