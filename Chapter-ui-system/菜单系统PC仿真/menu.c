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

#include "menu.h"

//**********************************************************************************************
//*******************************************运行函数*******************************************
//**********************************************************************************************
static void UI_ShowItems(UI_Driver* pDirver);
static void UI_KeyPress(UI_Driver* pDirver);

/**
 * UI运行函数
 * @param pDriver 当前UI界面
 */
void UI_Run(UI_Driver* pDirver)
{
    // 函数调用标志
    if (pDirver->FunctionRunFlag == ENABLE)
    {
        // 把键码传给相应调用函数
        UI_KeyScan();

        // 判断栈是否为空
        if (pDirver->StackTop == 0)
        {
            // 主函数直接传值，发送者为NULL (没有菜单项发出)
            pDirver->RunningFun(NULL, UI_GetKeyValue());
        }
        else
        {
            // 获取发送者并传值
            UI_MenuItem* currentMenu = pDirver->MenuStack[pDirver->StackTop - 1]->SelectItemNode->Item;
            pDirver->RunningFun(currentMenu, UI_GetKeyValue());
        }
        pDirver->LastFunctionRunFlag = ENABLE;
    }
    else if (pDirver->LastFunctionRunFlag == ENABLE)
    {
        // 函数退出，判断是否为主函数
        if (pDirver->StackTop == 0)
        {
            // 压入新的菜单到栈
            UI_Menu* newMenu = pDirver->MenuList->Item;
            // 设置初始选项
            newMenu->StartItemNode = newMenu->MenuItemList;
            newMenu->SelectItemNode = newMenu->MenuItemList;
            // 赋值到菜单栈
            pDirver->MenuStack[pDirver->StackTop++] = newMenu;
        }
        // 更新标志
        pDirver->UpdateAllFlag = ENABLE;
        pDirver->LastFunctionRunFlag = DISABLE;
    }
    else
    {
        // 按键处理
        UI_KeyPress(pDirver);
        // 更新显示菜单项
        UI_ShowItems(pDirver);
    }
}

//**********************************************************************************************
//*******************************************函数声明*******************************************
//**********************************************************************************************
static uint16_t UI_GetMenuItemIndex(UI_MenuItemListNode* currentItemNode);
static void UI_ShowChooseList(UI_Menu* currentMenu);

//**********************************************************************************************
//*******************************************工具函数*******************************************
//**********************************************************************************************

/**
 * 缓冲区分配
 * @param pDirver UI驱动器主体
 * @param size 缓冲区分配的大小
 * @return 缓冲区分配的首地址
 */
static void* UI_BufferMalloc(UI_Driver* pDirver, uint32_t size)
{
    // 判断分配空间是否溢出
    if (pDirver->MenuBufferTop + size > UI_MENU_BUFFER_SIZE) { return NULL; }
    // 分配新的空间并字节自增
    void* alloc = (void*)(pDirver->MenuBuffer + pDirver->MenuBufferTop);
    pDirver->MenuBufferTop += size;
    // 返回分配空间的首地址
    return alloc;
}

/**
 * 绘制项目的名称
 * @param pthis 菜单项
 * @param line  行数
 * @return 新菜单的地址
 */
static void UI_DrawTitle(UI_MenuItem* pthis, int16_t line)
{
    uint16_t x, y;
    x = UI_MENU_ITEM_CHOOSE_STR_LEN * UI_CHAR_SIZE_WIDTH;
    y = line * UI_LINE_INTERVAL;
    UI_ShowString(x, y, pthis->ItemTitle);  // 显示标题
}

/**
 * 求最大值函数
 * @param a 第一个参数
 * @param b 第二个参数
 * @return 最大值
 */
static uint16_t UI_Max(uint16_t a, uint16_t b)
{
    return a > b ? a : b;
}

/**
 * 限幅 (float)
 * @param a 第一个参数
 * @param min 最小值
 * @param max 最大值
 * @return 限幅值
 */
static float UI_LimitA(float a, float min, float max)
{
    if (a > max) { return max; }
    if (a < min) { return min; }
    return a;
}

/**
 * 限幅 (int32_t)
 * @param a 第一个参数
 * @param min 最小值
 * @param max 最大值
 * @return 限幅值
 */
static int32_t UI_LimitB(int32_t a, int16_t min, int16_t max)
{
    if (a > max) { return max; }
    if (a < min) { return min; }
    return a;
}

/**
 * 是否超过区间[min, max)
 * @param a 第一个参数
 * @param min 最小值
 * @param max 最大值
 * @return 限幅值
 */
static uint8_t UI_IsOverFlow(int16_t a, int16_t min, int16_t max)
{
    if (a >= max) { return ENABLE; }
    if (a < min)  { return ENABLE; }
    return DISABLE;
}

/**
 * 显示菜单的单位值
 * @param a 第一个参数
 * @param min 最小值
 * @param max 最大值
 * @return 限幅值
 */
static void UI_ShowUnit(UI_Driver* pDirver)
{
    const uint16_t unitStrLocation = UI_SIZE_WIDTH - UI_UNIT_STR_LEN * UI_CHAR_SIZE_WIDTH;
    switch (pDirver->ChangeUnit)
    {
    case UI_UNIT_MA: UI_ShowString(unitStrLocation, 0, UI_UNIT_MA_STR); break;
    case UI_UNIT_MB: UI_ShowString(unitStrLocation, 0, UI_UNIT_MB_STR); break;
    case UI_UNIT_MC: UI_ShowString(unitStrLocation, 0, UI_UNIT_MC_STR); break;
    case UI_UNIT_MD: UI_ShowString(unitStrLocation, 0, UI_UNIT_MD_STR); break;
    }
}

static void UI_ShowMode(UI_Driver* pDirver)
{
    UI_ShowString(0, 0, pDirver->ValueFlag == ENABLE ? UI_MENU_EDIT_MODE_STR : UI_MENU_ITEM_MODE_STR);
}

static float UI_GetUnit(UI_Driver* pDirver)
{
    switch (pDirver->ChangeUnit)
    {
    case UI_UNIT_MA: return UI_UNIT_MA_UNIT;
    case UI_UNIT_MB: return UI_UNIT_MB_UNIT;
    case UI_UNIT_MC: return UI_UNIT_MC_UNIT;
    case UI_UNIT_MD: return UI_UNIT_MD_UNIT;
    }
    return 0;
}

static void UI_ShowTitle(UI_Driver* pDirver, UI_Menu* currentMenu)
{
    char Buf[255], i;
    const uint16_t charMaxNum = UI_SIZE_WIDTH / UI_CHAR_SIZE_WIDTH;
    const uint16_t modeStrSize = UI_MENU_MODE_STR_LEN * UI_CHAR_SIZE_WIDTH;
    const uint16_t unitStrSize = UI_UNIT_STR_LEN * UI_CHAR_SIZE_WIDTH;
    const uint16_t titleWidth = (uint16_t)strlen(currentMenu->MenuTitle) * UI_CHAR_SIZE_WIDTH;

    for (i = 0; i < charMaxNum - UI_UNIT_STR_LEN - UI_MENU_MODE_STR_LEN; i++)
    {
        Buf[i] = UI_MENU_SPEARATE_CHAR;
    }
    Buf[i] = '\0';
    UI_ShowString(modeStrSize, 0, (const char*)Buf);
    UI_ShowString(modeStrSize + UI_Max(UI_SIZE_WIDTH - modeStrSize - unitStrSize - titleWidth, 0) / 2, 0, currentMenu->MenuTitle);
    UI_ShowMode(pDirver);
    UI_ShowUnit(pDirver);
}

static uint8_t UI_IsChangeMode(UI_Driver* pDirver)
{
    return ((UI_KEYNUM <= 4) && (pDirver->ValueFlag == ENABLE)) || (UI_KEYNUM > 4) || UI_HAVE_ENCODER;
}

/**
 * 开始初始化新的UI界面
 * @param pDirver UI驱动器主体
 * @param mainUI 菜单的主界面函数
 * @return 新菜单的地址
 */
static void UI_ShowItems(UI_Driver* pDirver)
{
    // 获取当前操作菜单
    if (pDirver->StackTop == 0) { return; }
    UI_Menu* currentMenu = pDirver->MenuStack[pDirver->StackTop - 1];
    UI_MenuItemListNode* selectItemNode = currentMenu->SelectItemNode;
    if (selectItemNode == NULL) { selectItemNode = currentMenu->MenuItemList; }
    UI_MenuItem* selectItem = selectItemNode->Item;

    // 更新标志
    if (pDirver->UpdateAllFlag == ENABLE || pDirver->UpdateValueFlag == ENABLE)
    {
        UI_MenuItemListNode* currentItemNode = currentMenu->StartItemNode;
        UI_MenuItem* currentItem;

        if (pDirver->UpdateAllFlag == ENABLE)
        {
            UI_Clear();
            UI_ShowChooseList(currentMenu);
            UI_ShowTitle(pDirver, currentMenu);
        }

        // 对项目进行计数，并且不能超过项目的最大数目
        for (uint16_t line = 1; line < UI_MAX_CHAR_LINE && currentItemNode != NULL; line++)
        {
            currentItem = currentItemNode->Item;
            currentItem->DrawDetail(currentItem, line);
            if (pDirver->UpdateAllFlag == ENABLE) { UI_DrawTitle(currentItem, line); }
            // 移动项目
            currentItemNode = currentItemNode->pNext;
        }
        // 清空标志
        pDirver->UpdateAllFlag = DISABLE;
        pDirver->UpdateValueFlag = DISABLE;
    }
}

/**
 * 开始初始化新的UI界面
 * @param pthis UI驱动器主体
 * @param mainUI 菜单的主界面函数
 * @return 新菜单的地址
 */
static void UI_KeyPress(UI_Driver* pDirver)
{
    // 获取当前操作菜单
    UI_Menu* currentMenu = pDirver->MenuStack[pDirver->StackTop - 1];
    UI_MenuItemListNode* selectItemNode = currentMenu->SelectItemNode;
    if (selectItemNode == NULL) { selectItemNode = currentMenu->MenuItemList; }
    UI_MenuItem* selectItem = selectItemNode->Item;

    // 函数没有调用，判断按键是否扫描具有数据
    if (UI_KeyScan())
    {
        int8_t item = UI_GetKeyEvent(UI_MENU_EVENT_ITEM);
        int8_t value = UI_GetKeyEvent(UI_MENU_EVENT_VALUE);
        // 进入
        if (UI_GetKeyEvent(UI_MENU_EVENT_ENTER) == ENABLE)
        {
            selectItem->Enter(selectItem);
        }
        // 退出
        else if (UI_GetKeyEvent(UI_MENU_EVENT_EXIT) == ENABLE)
        {
            selectItem->Exit(selectItem);
        }
        // 数值修改
        else if (value != 0)
        {
            if (UI_IsChangeMode(pDirver))
                selectItem->Change(selectItem, value);
        }
        // 菜单修改
        else
        {
            // 菜单为修改模式，退出修改模式
            if (pDirver->ValueFlag == ENABLE)
            {
                pDirver->ValueFlag = DISABLE;
                UI_ShowMode(pDirver);
                return;
            }
            // 当有4个按键时自动赋值
            if (value != 0) { item = value; }

            UI_MenuItemListNode* startItemNode = currentMenu->StartItemNode;
            uint16_t startIndex = UI_GetMenuItemIndex(startItemNode);
            uint16_t selectIndex = UI_GetMenuItemIndex(selectItemNode);
            uint8_t changeFlag = UI_IsOverFlow(selectIndex - startIndex + item, 0, UI_MAX_CHAR_LINE - 1);

            // 项目增量大于0，向前改变
            while (item > 0)
            {
                if (selectItemNode->pNext != NULL)
                {
                    // 若开始项目需要移动，则带着开始项目一起移动
                    if (changeFlag != 0 && startItemNode->pNext != NULL)
                    {
                        startItemNode = startItemNode->pNext;
                        pDirver->UpdateAllFlag = ENABLE;
                    }

                    selectItemNode = selectItemNode->pNext;
                }
                item--;
            }
            // 否则向后改变
            while (item < 0)
            {
                if (selectItemNode->pPrev != NULL)
                {
                    // 若开始项目需要移动，则带着开始项目一起移动
                    if (changeFlag != 0 && startItemNode->pPrev != NULL)
                    {
                        startItemNode = startItemNode->pPrev;
                        pDirver->UpdateAllFlag = ENABLE;
                    }

                    selectItemNode = selectItemNode->pPrev;
                }
                item++;
            }

            // 更新选中索引
            startIndex = UI_GetMenuItemIndex(startItemNode);
            selectIndex = UI_GetMenuItemIndex(selectItemNode);
            currentMenu->CurrentIndex = selectIndex - startIndex;

            // 更新选中和开始节点
            currentMenu->StartItemNode = startItemNode;
            currentMenu->SelectItemNode = selectItemNode;

            // 显示选项列表
            UI_ShowChooseList(currentMenu);
        }
    }
}

/**
 * 开始初始化新的UI界面
 * @param pthis UI驱动器主体
 * @param mainUI 菜单的主界面函数
 * @return 新菜单的地址
 */
void UI_StartInit(UI_Driver* pDirver, UI_FuncEvent mainUI)
{
    // 初始化默认值
    pDirver->MainUI = mainUI;              // 赋值UI运行主函数
    pDirver->RunningFun = mainUI;          // 默认运行函数为UI运行主函数
    pDirver->MenuCount = 0;                // 没有菜单加入
    pDirver->MenuBufferTop = 0;            // 缓冲区无分配空间
    pDirver->StackTop = 0;                 // 没有菜单栈的加入
    pDirver->UpdateAllFlag = DISABLE;      // 所有内容不更新
    pDirver->UpdateValueFlag = DISABLE;    // 所有值不更新
    pDirver->ChangeUnit = UI_UNIT_MA;      // 修改单位值
}

/**
 * 结束初始化新的UI界面
 * @param pthis UI驱动器主体
 * @param mainUI 菜单的主界面函数
 * @return 新菜单的地址
 */
void UI_EndInit(UI_Driver* pDirver)
{
    // 结束初始化，菜单更新
    assert(pDirver->MenuCount != 0);
    pDirver->FunctionRunFlag = ENABLE;     // 默认主函数运行

    // 遍历所有菜单，设置项目节点值
    UI_MenuListNode* pCurrentNode = pDirver->MenuList;
    // 遍历元素
    while (pCurrentNode != NULL)
    {
        UI_Menu* pCurrent = pCurrentNode->Item;
        UI_MenuItemListNode* pItemNodeFirst = pCurrent->MenuItemList;
        assert(pItemNodeFirst);
        // 设置初始值
        pCurrent->StartItemNode = pItemNodeFirst;
        pCurrent->SelectItemNode = pItemNodeFirst;
        // 移动元素
        pCurrentNode = pCurrentNode->pNext;
    }
}

/**
 * 创建新的菜单
 * @param pthis UI驱动器主体
 * @param menuTitle 菜单标题
 * @return 新菜单的地址
 */
UI_Menu* UI_AddMenu(UI_Driver* pDirver, const char* menuTitle)
{
    // 声明新的菜单
    UI_Menu* newMenu = (UI_Menu*)UI_BufferMalloc(pDirver, sizeof(UI_Menu));
    UI_MenuListNode* pNode = UI_BufferMalloc(pDirver, sizeof(UI_MenuListNode));  // 分配新的菜单项目节点
    assert(pNode != NULL);
    assert(newMenu != NULL);
    // 赋值相关信息
    newMenu->BaseUI = pDirver;            // 基于的UI界面
    newMenu->MenuTitle = menuTitle;       // 菜单标题
    newMenu->ItemCount = 0;               // 项目个数
    newMenu->MenuItemList = NULL;         // 菜单项目列表
    newMenu->StartItemNode = NULL;        // UI开始的项目节点
    newMenu->SelectItemNode = NULL;       // UI选择的项目节点
    newMenu->CurrentIndex = 0;
    // 添加到菜单列表
    if (pDirver->MenuCount == 0)
    {
        // 菜单没有项目，使用pNode作为菜单的第一个元素
        pNode->pNext = NULL;
        pNode->Item = newMenu;

        pDirver->MenuList = pNode;
        pDirver->MenuCount = 1;
    }
    else
    {
        // 菜单有项目，附加到菜单的最后一个元素
        UI_MenuListNode* pLast = pDirver->MenuList;
        // 移动到最后一个元素
        while (pLast->pNext != NULL)
        {
            pLast = pLast->pNext;
        }

        // 附加元素
        pNode->pNext = NULL;
        pNode->Item = newMenu;
        pLast->pNext = pNode;

        pDirver->MenuCount++;
    }
    // 返回菜单指针
    return newMenu;
}

/**
 * 添加相应项目到指定菜单
 * @param pthis 添加到的目的菜单
 * @param pitem 添加的源菜单项目
 * @return 新菜单的地址
 */
static void UI_AddMenuItem(UI_Menu* pthis, UI_MenuItem* pItem)
{
    UI_Driver* pDirver = pthis->BaseUI;
    UI_MenuItemListNode* pNode = UI_BufferMalloc(pDirver, sizeof(UI_MenuItemListNode));  // 分配新的菜单项目节点
    assert(pNode != NULL);

    if (pthis->ItemCount == 0)
    {
        // 菜单没有项目，使用pNode作为菜单的第一个元素
        pNode->pNext = NULL;
        pNode->pPrev = NULL;
        pNode->Index = 0;
        pNode->Item = pItem;

        pthis->MenuItemList = pNode;
        pthis->ItemCount = 1;
    }
    else
    {
        // 菜单有项目，附加到菜单的最后一个元素
        UI_MenuItemListNode* pLast = pthis->MenuItemList;
        assert(pLast != NULL);
        // 移动到最后一个元素
        while (pLast->pNext != NULL)
        {
            pLast = pLast->pNext;
        }

        // 附加元素
        pNode->pNext = NULL;
        pNode->pPrev = pLast;
        pNode->Index = pthis->ItemCount;
        pNode->Item = pItem;
        pLast->pNext = pNode;
        // 添加项目
        pthis->ItemCount++;
    }
}

/**
 * 添加跳转项目到指定菜单
 * @param pMenu 添加到的目的菜单
 * @param itemTitle 项目标题
 * @param jumpItem 跳转的菜单
 */
void UI_MenuAddJumpItem(UI_Menu* pMenu, const char* itemTitle, UI_Menu* jumpMenu)
{
    // 1. 分配空间
    UI_Driver* pDirver = pMenu->BaseUI;
    UI_MenuItem* newItem = (UI_MenuItem*)UI_BufferMalloc(pDirver, sizeof(UI_MenuItem));
    assert(newItem != NULL);
    // 2. 赋值相关信息
    newItem->ItemTitle = itemTitle;
    newItem->BaseMenu = pMenu;                    // 基于的菜单
    newItem->DrawDetail = UI_JumpItemDrawDetail;  // 基于的项目详情
    newItem->Enter = UI_JumpItemEnter;            // 基于的项目进入函数
    newItem->Exit = UI_BaseItemExit;              // 基于的项目退出函数
    newItem->Change = UI_BaseItemChange;          // 基于的数值更改函数
    newItem->Union.Jump = jumpMenu;
    // 3. 添加到菜单
    UI_AddMenuItem(pMenu, (UI_MenuItem*)newItem);
}

/**
 * 添加退出项目到指定菜单
 * @param pMenu 添加到的目的菜单
 * @param itemTitle 项目标题
 */
void UI_MenuAddExitItem(UI_Menu* pMenu, const char* itemTitle)
{
    // 1. 分配空间
    UI_Driver* pDirver = pMenu->BaseUI;
    UI_MenuItem* newItem = (UI_MenuItem*)UI_BufferMalloc(pDirver, sizeof(UI_MenuItem));
    assert(newItem != NULL);
    // 2. 赋值相关信息
    newItem->ItemTitle = itemTitle;
    newItem->BaseMenu = pMenu;                    // 基于的菜单
    newItem->DrawDetail = UI_ExitItemDrawDetail;  // 基于的项目详情
    newItem->Enter = UI_ExitItemEnter;            // 基于的项目进入函数
    newItem->Exit = UI_BaseItemExit;              // 基于的项目退出函数
    newItem->Change = UI_BaseItemChange;          // 基于的数值更改函数
    // 3. 添加到菜单
    UI_AddMenuItem(pMenu, (UI_MenuItem*)newItem);
}

/**
 * 添加跳转函数项目到指定菜单
 * @param pMenu 添加到的目的菜单
 * @param itemTitle 项目标题
 * @param jumpFunction 跳转的函数
 */
void UI_MenuAddFuncItem(UI_Menu* pMenu, const char* itemTitle, UI_FuncEvent jumpFunction)
{
    // 1. 分配空间
    UI_Driver* pDirver = pMenu->BaseUI;
    UI_MenuItem* newItem = (UI_MenuItem*)UI_BufferMalloc(pDirver, sizeof(UI_MenuItem));
    assert(newItem != NULL);
    // 2. 赋值相关信息
    newItem->ItemTitle = itemTitle;
    newItem->BaseMenu = pMenu;                    // 基于的菜单
    newItem->DrawDetail = UI_FuncItemDrawDetail;  // 基于的项目详情
    newItem->Enter = UI_FuncItemEnter;            // 基于的项目进入函数
    newItem->Exit = UI_BaseItemExit;              // 基于的项目退出函数
    newItem->Change = UI_BaseItemChange;          // 基于的数值更改函数
    newItem->Union.Func = jumpFunction;
    // 3. 添加到菜单
    UI_AddMenuItem(pMenu, (UI_MenuItem*)newItem);
}

/**
 * 添加修改项目到指定菜单 (int32_t)
 * @param pMenu 添加到的目的菜单
 * @param itemTitle 项目标题
 * @param bindValPtr 绑定的变量
 * @param Min 变量最小值
 * @param Max 变量最大值
 */
void UI_MenuAddValueItem_Int(UI_Menu* pMenu, const char* itemTitle, int32_t* bindValPtr, int32_t Min, int32_t Max)
{
    // 1. 分配空间
    UI_Driver* pDirver = pMenu->BaseUI;
    UI_MenuItem* newItem = (UI_MenuItem*)UI_BufferMalloc(pDirver, sizeof(UI_MenuItem));
    assert(newItem != NULL);
    // 2. 赋值相关信息
    newItem->ItemTitle = itemTitle;
    newItem->BaseMenu = pMenu;                    // 基于的菜单
    newItem->DrawDetail = UI_ValueItemDrawDetail; // 基于的项目详情
    newItem->Enter = UI_ValueItemEnter;           // 基于的项目进入函数
    newItem->Exit = UI_ValueItemExit;             // 基于的项目退出函数
    newItem->Change = UI_ValueItemChange;         // 基于的数值更改函数
    newItem->Union.Value.BindPtr = UI_TypeTranPtr(bindValPtr, uint32_t);
    newItem->Union.Value.Max = UI_TypeTran(Max, uint32_t);
    newItem->Union.Value.Min = UI_TypeTran(Min, uint32_t);
    newItem->Union.Value.Type = UI_TYPE_INT;
    // 3. 添加到菜单
    UI_AddMenuItem(pMenu, (UI_MenuItem*)newItem);
}

/**
 * 添加修改项目到指定菜单 (float)
 * @param pMenu 添加到的目的菜单
 * @param itemTitle 项目标题
 * @param bindValPtr 绑定的变量
 * @param Min 变量最小值
 * @param Max 变量最大值
 */
void UI_MenuAddValueItem_Float(UI_Menu* pMenu, const char* itemTitle, float* bindValPtr, float Min, float Max)
{
    // 1. 分配空间
    UI_Driver* pDirver = pMenu->BaseUI;
    UI_MenuItem* newItem = (UI_MenuItem*)UI_BufferMalloc(pDirver, sizeof(UI_MenuItem));
    assert(newItem != NULL);
    // 2. 赋值相关信息
    newItem->ItemTitle = itemTitle;
    newItem->BaseMenu = pMenu;                     // 基于的菜单
    newItem->DrawDetail = UI_ValueItemDrawDetail;  // 基于的项目详情
    newItem->Enter = UI_ValueItemEnter;            // 基于的项目进入函数
    newItem->Exit = UI_ValueItemExit;              // 基于的项目退出函数
    newItem->Change = UI_ValueItemChange;          // 基于的数值更改函数
    newItem->Union.Value.BindPtr = UI_TypeTranPtr(bindValPtr, uint32_t);
    newItem->Union.Value.Max = UI_TypeTran(Max, uint32_t);
    newItem->Union.Value.Min = UI_TypeTran(Min, uint32_t);
    newItem->Union.Value.Type = UI_TYPE_FLOAT;
    // 3. 添加到菜单
    UI_AddMenuItem(pMenu, (UI_MenuItem*)newItem);
}

/**
 * 添加单选项目到指定菜单
 * @param pMenu 添加到的目的菜单
 * @param itemTitle 项目标题
 * @param bindValPtr 绑定的变量
 * @param Establish 成立的值
 * @note 可以指定多个变量用于单选，单选是基于变量是否为某一个值来决定的
 */
void UI_MenuAddChooseItem(UI_Menu* pMenu, const char* itemTitle, int32_t* bindValPtr, int32_t Establish)
{
    // 1. 分配空间
    UI_Driver* pDirver = pMenu->BaseUI;
    UI_MenuItem* newItem = (UI_MenuItem*)UI_BufferMalloc(pDirver, sizeof(UI_MenuItem));
    assert(newItem != NULL);
    // 2. 赋值相关信息
    newItem->ItemTitle = itemTitle;
    newItem->BaseMenu = pMenu;                      // 基于的菜单
    newItem->DrawDetail = UI_ChooseItemDrawDetail;  // 基于的项目详情
    newItem->Enter = UI_ChooseItemEnter;            // 基于的项目进入函数
    newItem->Exit = UI_ChooseItemExit;              // 基于的项目退出函数
    newItem->Change = UI_BaseItemChange  ;          // 基于的数值更改函数
    newItem->Union.Choose.BindPtr = bindValPtr;
    newItem->Union.Choose.Establish = Establish;
    // 3. 添加到菜单
    UI_AddMenuItem(pMenu, (UI_MenuItem*)newItem);
}

/**
 * 添加多选项目到指定菜单
 * @param pMenu 添加到的目的菜单
 * @param itemTitle 项目标题
 * @param bindValPtr 绑定的变量
 * @note 不可以指定多个变量用于多选，多选基于变量是否设置为ENABLE/DISABLE来确定的
 */
void UI_MenuAddSelectItem(UI_Menu* pMenu, const char* itemTitle, int32_t* bindValPtr)
{
    // 1. 分配空间
    UI_Driver* pDirver = pMenu->BaseUI;
    UI_MenuItem* newItem = (UI_MenuItem*)UI_BufferMalloc(pDirver, sizeof(UI_MenuItem));
    assert(newItem != NULL);
    // 2. 赋值相关信息
    newItem->ItemTitle = itemTitle;
    newItem->BaseMenu = pMenu;                     // 基于的菜单
    newItem->DrawDetail = UI_SelectItemDrawDetail; // 基于的项目详情
    newItem->Enter = UI_SelectItemEnter;           // 基于的项目进入函数
    newItem->Exit = UI_SelectItemExit;             // 基于的项目退出函数
    newItem->Change = UI_BaseItemChange;           // 基于的数值更改函数
    newItem->Union.BindPtr = bindValPtr;
    // 3. 添加到菜单
    UI_AddMenuItem(pMenu, (UI_MenuItem*)newItem);
}

/**
 * 停止菜单内函数的运行
 * @param pDriver 当前UI界面
 */
void UI_StopRunFunc(UI_Driver* pDirver)
{
    pDirver->FunctionRunFlag = DISABLE;
}

/**
 * 获取项目菜单索引
 * @param pMenu 添加到的目的菜单
 * @param itemTitle 项目标题
 * @param jumpFunction 跳转的函数
 */
static uint16_t UI_GetMenuItemIndex(UI_MenuItemListNode* currentItemNode)
{
    UI_Menu* pMenu = currentItemNode->Item->BaseMenu;   // 获取子菜单
    UI_MenuItemListNode* pItem = pMenu->StartItemNode;
    uint16_t itemCounter = 0;    // 项目计数器

    while (pItem != currentItemNode && pItem != NULL)
    {
        itemCounter++;
        pItem = pItem->pNext;
    }

    return itemCounter;
}

/**
 * 显示选项列表
 * @param currentMenu 当前菜单
 */
static void UI_ShowChooseList(UI_Menu* currentMenu)
{
    UI_MenuItemListNode* selectItemNode = currentMenu->SelectItemNode;
    UI_MenuItemListNode* currentItemNode = currentMenu->StartItemNode;

    // 对项目进行计数，并且不能超过项目的最大数目
    for (uint16_t line = 1; line < UI_MAX_CHAR_LINE && currentItemNode != NULL; line++)
    {
        if (currentItemNode == selectItemNode)
        {
            UI_ShowString(0, line * UI_LINE_INTERVAL, UI_MENU_ITEM_CHOOSE_STR);
        }
        else
        {
            UI_ShowString(0, line * UI_LINE_INTERVAL, UI_MENU_ITEM_NOT_CHOOSE_STR);
        }


        // 移动项目
        currentItemNode = currentItemNode->pNext;
    }
}

//**********************************************************************************************
//*******************************************回调函数*******************************************
//**********************************************************************************************

/**
 * 项目修改函数 (父类)
 * @param pthis 当前菜单
 * @param delta 修改的偏移值
 */
void UI_BaseItemChange(UI_MenuItem* pthis, int16_t delta)
{
    return;
}

void UI_BaseItemExit(UI_MenuItem* pthis)
{
    UI_Driver* pDirver = pthis->BaseMenu->BaseUI;
    if (pDirver->StackTop == 0) return;             // 菜单栈无数据，返回

    pDirver->StackTop--;
    pDirver->UpdateAllFlag = 1;

    if (pDirver->StackTop == 0)
    {
        UI_Clear();
        pDirver->RunningFun = pDirver->MainUI;
        pDirver->FunctionRunFlag = ENABLE;          // 开启函数调用
    }
}

// 2. 跳转子菜单
void UI_JumpItemDrawDetail(UI_MenuItem* pthis, int16_t line)
{
    uint16_t x, y;
    x = UI_SIZE_WIDTH - UI_MENU_JUMP_ITEM_STR_LEN * UI_CHAR_SIZE_WIDTH;
    y = line * UI_LINE_INTERVAL;
    UI_ShowString(x, y, UI_MENU_JUMP_ITEM_STR);
}

void UI_JumpItemEnter(UI_MenuItem* pthis)
{
    UI_Driver* pDirver = pthis->BaseMenu->BaseUI;
    UI_Menu* newMenu = pthis->Union.Jump;
    // 设置初始选项
    newMenu->StartItemNode = newMenu->MenuItemList;
    newMenu->SelectItemNode = newMenu->MenuItemList;
    // 赋值到菜单栈
    pDirver->MenuStack[pDirver->StackTop++] = newMenu;
    // 更新标志
    pDirver->UpdateAllFlag = ENABLE;
}

// 3.退出子菜单
void UI_ExitItemDrawDetail(UI_MenuItem* pthis, int16_t line)
{
    uint16_t x, y;
    x = UI_SIZE_WIDTH - UI_MENU_EXIT_ITEM_STR_LEN * UI_CHAR_SIZE_WIDTH;
    y = line * UI_LINE_INTERVAL;
    UI_ShowString(x, y, UI_MENU_EXIT_ITEM_STR);
}

void UI_ExitItemEnter(UI_MenuItem* pthis)
{
    UI_BaseItemExit(pthis);
}

// 4.函数子菜单
void UI_FuncItemDrawDetail(UI_MenuItem* pthis, int16_t line)
{
    uint16_t x, y;
    x = UI_SIZE_WIDTH - UI_MENU_FUNC_ITEM_STR_LEN * UI_CHAR_SIZE_WIDTH;
    y = line * UI_LINE_INTERVAL;
    UI_ShowString(x, y, UI_MENU_FUNC_ITEM_STR);
}

void UI_FuncItemEnter(UI_MenuItem* pthis)
{
    UI_Driver* pDirver = pthis->BaseMenu->BaseUI;

    if (pthis->Union.Func != NULL)
    {
        UI_Clear();
        pDirver->RunningFun = pthis->Union.Func;
        pDirver->FunctionRunFlag = ENABLE;          // 开启函数调用
    }
}

// 5. 值修改菜单
void UI_ValueItemDrawDetail(UI_MenuItem* pthis, int16_t line)
{
    UI_MenuItem* current = (UI_MenuItem*)pthis;
    UI_Driver* pDirver = pthis->BaseMenu->BaseUI;
    uint16_t x, y;
    x = UI_SIZE_WIDTH - UI_MENU_VALUE_LEN * UI_CHAR_SIZE_WIDTH;
    y = line * UI_LINE_INTERVAL;

    // 根据不同的值进行不同的显示
    switch (pthis->Union.Value.Type)
    {
    case UI_TYPE_INT:
    {
        int32_t* bindValPtr = UI_TypeTranPtr(pthis->Union.Value.BindPtr, int32_t);
        UI_ShowInt(x, y, *bindValPtr);
        break;
    }
    case UI_TYPE_FLOAT:
    {
        float* bindValPtr = UI_TypeTranPtr(pthis->Union.Value.BindPtr, float);
        UI_ShowFloat(x, y, *bindValPtr);
        break;
    }
    }
}

void UI_ValueItemEnter(UI_MenuItem* pthis)
{
    UI_MenuItem* current = (UI_MenuItem*)pthis;
    UI_Driver* pDirver = pthis->BaseMenu->BaseUI;

    if (pDirver->ValueFlag == DISABLE)
    {
        // 若没有开启修改模式，进入修改模式
        pDirver->ValueFlag = ENABLE;
        UI_ShowMode(pDirver);

        // 整数限制更新单位
        if (pthis->Union.Value.Type == UI_TYPE_INT && pDirver->ChangeUnit >= UI_UNIT_MC)
        {
            pDirver->ChangeUnit = UI_UNIT_MA;
            UI_ShowUnit(pDirver);
        }
    }
    else
    {
        // 若开启修改模式，仅改变编辑值的单位
        if (pthis->Union.Value.Type == UI_TYPE_FLOAT)
        {
            pDirver->ChangeUnit = (pDirver->ChangeUnit + 1) % 4;
        }
        else
        {
            pDirver->ChangeUnit = (pDirver->ChangeUnit + 1) % 2;
        }
        UI_ShowUnit(pDirver);
    }
}

void UI_ValueItemExit(UI_MenuItem* pthis)
{
    UI_Driver* pDirver = pthis->BaseMenu->BaseUI;

    if (pDirver->ValueFlag == ENABLE)
    {
        // 保存变量
        UI_Save(pthis->Union.Value.BindPtr);
        // 若开启修改模式，退出修改模式
        pDirver->ValueFlag = DISABLE;
        UI_ShowMode(pDirver);
    }
    else
    {
        // 否则调用父类的虚函数
        UI_BaseItemExit(pthis);
    }
}

/**
 * 项目修改函数 (值修改菜单类)
 * @param pthis 当前菜单
 * @param delta 修改的偏移值
 */
void UI_ValueItemChange(UI_MenuItem* pthis, int16_t delta)
{
    UI_Menu* pMenu = pthis->BaseMenu;
    UI_Driver* pDirver = pMenu->BaseUI;

    if (pDirver->ValueFlag == DISABLE)
    {
        // 若没有开启修改模式，开启修改模式并退出 (仅针对6个按键)
        pDirver->ValueFlag = ENABLE;
        UI_ShowMode(pDirver);
        return;
    }

    // 根据不同的值进行不同的修改
    switch (pthis->Union.Value.Type)
    {
    case UI_TYPE_INT:
    {
        int32_t* bindValPtr = UI_TypeTranPtr(pthis->Union.Value.BindPtr, int32_t);
        int32_t changeMin = UI_TypeTran(pthis->Union.Value.Min, int32_t);
        int32_t changeMax = UI_TypeTran(pthis->Union.Value.Max, int32_t);
        *bindValPtr = UI_LimitB(*bindValPtr + (int32_t)round(UI_GetUnit(pDirver) * delta), changeMin, changeMax);
        break;
    }
    case UI_TYPE_FLOAT:
    {
        float* bindValPtr = UI_TypeTranPtr(pthis->Union.Value.BindPtr, float);
        float changeMin = UI_TypeTran(pthis->Union.Value.Min, float);
        float changeMax = UI_TypeTran(pthis->Union.Value.Max, float);
        *bindValPtr = UI_LimitA(*bindValPtr + UI_GetUnit(pDirver) * delta, changeMin, changeMax);
        break;
    }
    }

    UI_ValueItemDrawDetail(pthis, pMenu->CurrentIndex + 1);
}

// 6. 单选菜单
void UI_ChooseItemDrawDetail(UI_MenuItem* pthis, int16_t line)
{
    UI_Driver* pDirver = pthis->BaseMenu->BaseUI;
    uint16_t x, y;
    x = UI_SIZE_WIDTH - UI_MENU_RADIO_STR_LEN * UI_CHAR_SIZE_WIDTH;
    y = line * UI_LINE_INTERVAL;

    // 判断值是否成立
    if (*pthis->Union.Choose.BindPtr == pthis->Union.Choose.Establish)
    {
        UI_ShowString(x, y, UI_MENU_RADIO_SEL_STR);
    }
    else
    {
        UI_ShowString(x, y, UI_MENU_RADIO_NONE_STR);
    }
}

void UI_ChooseItemEnter(UI_MenuItem* pthis)
{
    UI_Driver* pDirver = pthis->BaseMenu->BaseUI;

    *pthis->Union.Choose.BindPtr = pthis->Union.Choose.Establish;
    pDirver->UpdateValueFlag = ENABLE;
}

void UI_ChooseItemExit(UI_MenuItem* pthis)
{
    UI_MenuItem* current = (UI_MenuItem*)pthis;
    UI_Driver* pDirver = pthis->BaseMenu->BaseUI;

    // 保存变量
    UI_Save(pthis->Union.Choose.BindPtr);
    // 调用父类的虚函数
    UI_BaseItemExit(pthis);
}

// 7. 多选菜单
void UI_SelectItemDrawDetail(UI_MenuItem* pthis, int16_t line)
{
    UI_Driver* pDirver = pthis->BaseMenu->BaseUI;
    uint16_t x, y;
    x = UI_SIZE_WIDTH - UI_MENU_CHECK_STR_LEN * UI_CHAR_SIZE_WIDTH;
    y = line * UI_LINE_INTERVAL;

    // 判断值是否成立
    if (*pthis->Union.BindPtr == ENABLE)
    {
        UI_ShowString(x, y, UI_MENU_CHECK_SEL_STR);
    }
    else
    {
        UI_ShowString(x, y, UI_MENU_CHECK_NONE_STR);
    }
}

void UI_SelectItemEnter(UI_MenuItem* pthis)
{
    UI_Menu* pMenu = pthis->BaseMenu;
    UI_Driver* pDirver = pMenu->BaseUI;

    // 更新显示及选项
    if (*pthis->Union.BindPtr == ENABLE)
    {
        *pthis->Union.BindPtr = DISABLE;
    }
    else
    {
        *pthis->Union.BindPtr = ENABLE;
    }
    UI_SelectItemDrawDetail(pthis, pMenu->CurrentIndex + 1);
}

void UI_SelectItemExit(UI_MenuItem* pthis)
{
    UI_MenuItem* current = (UI_MenuItem*)pthis;
    UI_Driver* pDirver = pthis->BaseMenu->BaseUI;

    // 保存变量
    UI_Save(pthis->Union.BindPtr);
    // 调用父类的虚函数
    UI_BaseItemExit(pthis);
}
