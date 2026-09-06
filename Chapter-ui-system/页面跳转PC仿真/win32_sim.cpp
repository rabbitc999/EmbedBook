/**
 * @file    win32_sim.cpp
 * @brief   菜单系统 Win32 API（GDI）PC 仿真层
 * @details 用纯 Win32 窗口模拟 240x300 LCD，并实现 hardware.h 中的
 *          LCD/按键接口；菜单核心代码保持与单片机一致。
 */

#include <windows.h>
#include <string.h>
#include <math.h>
#include "hardware.h"
#include "menu.h"

static HWND s_hWnd = NULL;
static HDC s_hdc = NULL;
static HBRUSH s_whiteBrush = NULL;
uint8_t UI_KeyNumber = 0;

/**
 * @brief  窗口过程
 * @param  hWnd   窗口句柄
 * @param  uMsg   消息
 * @param  wParam 消息参数
 * @param  lParam 消息参数
 * @retval LRESULT
 */
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg,
                                WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            FillRect(hdc, &ps.rcPaint, s_whiteBrush);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief  LCD 初始化（创建窗口由 WinMain 完成）
 */
extern "C" void LCD_Init(void)
{
    s_hdc = GetDC(s_hWnd);
    s_whiteBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);

    SetBkMode(s_hdc, TRANSPARENT);
    SetTextColor(s_hdc, RGB(0, 0, 0));

    /* 使用 16 像素宋体模拟 LCD 字符 */
    HFONT font = CreateFontA(UI_CHAR_SIZE_HEIGHT, 0,
                             0, 0, FW_NORMAL, 0, 0, 0,
                             DEFAULT_CHARSET, 0, 0,
                             CLEARTYPE_QUALITY, 0, "宋体");
    SelectObject(s_hdc, font);

    LCD_Clear();
}

/**
 * @brief  LCD 清屏
 */
extern "C" void LCD_Clear(void)
{
    RECT rect = { 0, 0, UI_SIZE_WIDTH, UI_SIZE_HEIGHT };
    FillRect(s_hdc, &rect, s_whiteBrush);
}

/**
 * @brief  LCD 显示字符串
 * @param  x   横坐标
 * @param  y   纵坐标
 * @param  str 字符串
 */
extern "C" void LCD_ShowString(uint16_t x, uint16_t y, const char *str)
{
    int length = (int)strlen(str);
    RECT rect = {
        (int)x,
        (int)y,
        (int)x + length * UI_CHAR_SIZE_WIDTH,
        (int)y + UI_CHAR_SIZE_HEIGHT
    };

    /* 先填充字符区域，避免旧字符残留造成文字重叠 */
    FillRect(s_hdc, &rect, s_whiteBrush);
    TextOutA(s_hdc, (int)x, (int)y, str, length);
}

/**
 * @brief  RGB565 转换为 Win32 COLORREF
 * @param  rgb565 RGB565 像素
 * @retval RGB 颜色
 */
static COLORREF Rgb565ToColorRef(uint16_t rgb565)
{
    uint8_t r = (uint8_t)((rgb565 >> 11) & 0x1F);
    uint8_t g = (uint8_t)((rgb565 >> 5) & 0x3F);
    uint8_t b = (uint8_t)(rgb565 & 0x1F);

    /* 5/6/5 位扩展到 8 位 */
    r = (uint8_t)((r << 3) | (r >> 2));
    g = (uint8_t)((g << 2) | (g >> 4));
    b = (uint8_t)((b << 3) | (b >> 2));
    return RGB(r, g, b);
}

/**
 * @brief  LCD 绘制单点
 * @param  x      横坐标
 * @param  y      纵坐标
 * @param  rgb565 RGB565 颜色
 */
extern "C" void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t rgb565)
{
    if ((s_hdc == NULL) ||
        (x >= UI_SIZE_WIDTH) || (y >= UI_SIZE_HEIGHT)) {
        return;
    }
    SetPixelV(s_hdc, (int)x, (int)y, Rgb565ToColorRef(rgb565));
}

/**
 * @brief  LCD 显示 RGB565 图像
 * @param  x       目标左上角横坐标
 * @param  y       目标左上角纵坐标
 * @param  width   图像宽度
 * @param  height  图像高度
 * @param  pImage  RGB565 像素数组
 */
extern "C" void LCD_ShowRGB565Image(uint16_t x,
                                    uint16_t y,
                                    uint16_t width,
                                    uint16_t height,
                                    const uint16_t *pImage)
{
    uint16_t row = 0;
    uint16_t col = 0;

    if ((pImage == NULL) || (s_hdc == NULL)) {
        return;
    }

    for (row = 0; row < height; row++) {
        for (col = 0; col < width; col++) {
            uint16_t pixel = pImage[row * width + col];
            LCD_DrawPoint((uint16_t)(x + col),
                          (uint16_t)(y + row),
                          pixel);
        }
    }
}

/**
 * @brief  页面跳转演示：简单主页/菜单页/曲线页
 */
typedef void (*UI_PageEntry_T)(void *param);

typedef struct __UI_Navigator_T
{
    UI_PageEntry_T currentPage;
    void *currentParam;
} UI_Navigator_T;

static UI_Navigator_T s_Navigator;
static uint8_t s_PageKey = UI_KEY_NONE;

/**
 * @brief  页面跳转
 * @param  page  目标页面入口
 * @param  param 页面参数
 */
static void GUI_JumpPage(UI_PageEntry_T page, void *param)
{
    s_Navigator.currentPage = page;
    s_Navigator.currentParam = param;
    page(param);
}

static void GUI_HomePage(void *param);
static void GUI_MenuPage(void *param);
static void GUI_CurvePage(void *param);

/**
 * @brief  主页：显示一张 RGB565 小图
 */
static void GUI_HomePage(void *param)
{
    uint16_t image[8][8];
    uint16_t row = 0;
    uint16_t col = 0;

    (void)param;
    LCD_Clear();
    LCD_ShowString(0, 0, "HOME PAGE");
    LCD_ShowString(0, 32, "Q -> Menu");

    /* 生成 8x8 红/蓝棋盘 RGB565 图像 */
    for (row = 0; row < 8; row++) {
        for (col = 0; col < 8; col++) {
            image[row][col] = ((row + col) & 1u)
                              ? 0xF800u   /* 红色 RGB565 */
                              : 0x001Fu;  /* 蓝色 RGB565 */
        }
    }
    LCD_ShowRGB565Image(80, 60, 8, 8, &image[0][0]);

    if (s_PageKey == UI_KEY_ENTER) {
        GUI_JumpPage(GUI_MenuPage, NULL);
    }
}

/**
 * @brief  菜单页
 */
static void GUI_MenuPage(void *param)
{
    (void)param;
    LCD_Clear();
    LCD_ShowString(0, 0, "MENU PAGE");
    LCD_ShowString(0, 24, "Q -> Curve");
    LCD_ShowString(0, 40, "A -> Home");

    if (s_PageKey == UI_KEY_ENTER) {
        GUI_JumpPage(GUI_CurvePage, NULL);
    }
    if (s_PageKey == UI_KEY_EXIT) {
        GUI_JumpPage(GUI_HomePage, NULL);
    }
}

/**
 * @brief  曲线页：使用 LCD_DrawPoint 画正弦曲线
 */
static void GUI_CurvePage(void *param)
{
    uint16_t x = 0;

    (void)param;
    LCD_Clear();
    LCD_ShowString(0, 0, "CURVE PAGE");

    for (x = 0; x < UI_SIZE_WIDTH; x += 2) {
        int y = 120 - (int)(60.0 * sin((double)x / 30.0));
        if ((y >= 0) && (y < UI_SIZE_HEIGHT)) {
            LCD_DrawPoint(x, (uint16_t)y, 0x07E0u);  /* 绿色 */
        }
    }

    LCD_ShowString(0, UI_SIZE_HEIGHT - 24, "A -> Menu");
    if (s_PageKey == UI_KEY_EXIT) {
        GUI_JumpPage(GUI_MenuPage, NULL);
    }
}

/**
 * @brief  页面导航仿真主循环
 */
static void Gui_PageNavSimRun(void)
{
    LCD_Init();
    GUI_JumpPage(GUI_HomePage, NULL);

    while (true) {
        MSG msg = { 0 };
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        s_PageKey = UI_KEY_NONE;
        if (Key_Scan() == ENABLE) {
            s_PageKey = Key_GetNum();
            Key_Clear();
        }
        if (s_Navigator.currentPage != NULL) {
            s_Navigator.currentPage(s_Navigator.currentParam);
        }
        Sleep(10);
    }
}

/**
 * @brief  按键扫描
 * @retval 返回按键码，按下返回 ENABLE，否则 DISABLE
 */
extern "C" uint8_t Key_Scan(void)
{
    static uint8_t lastState = UI_KEY_NONE;
    static uint16_t longPressTime = 0;
    uint8_t currentState = UI_KEY_NONE;
    uint8_t keys[] = {
        UI_KEY_ENTER,
        UI_KEY_EXIT,
        UI_KEY_ITEM_UP,
        UI_KEY_ITEM_DOWN,
        UI_KEY_VAL_UP,
        UI_KEY_VAL_DOWN
    };

    for (uint8_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (GetAsyncKeyState((int)keys[i]) & 0x8000) {
            currentState = keys[i];
            break;
        }
    }

    /* 长按方向键连续触发 */
    if ((currentState == UI_KEY_ITEM_UP) ||
        (currentState == UI_KEY_ITEM_DOWN) ||
        (currentState == UI_KEY_VAL_UP) ||
        (currentState == UI_KEY_VAL_DOWN)) {
        longPressTime++;
        if ((longPressTime >= 30) && ((longPressTime % 5) == 0)) {
            UI_KeyNumber = currentState;
            lastState = currentState;
            return ENABLE;
        }
    }

    /* 松开时上报一次按键 */
    if ((lastState != UI_KEY_NONE) && (currentState == UI_KEY_NONE)) {
        UI_KeyNumber = lastState;
        longPressTime = 0;
        lastState = UI_KEY_NONE;
        return ENABLE;
    }

    lastState = currentState;
    return DISABLE;
}

/**
 * @brief  获取按键码
 */
extern "C" uint8_t Key_GetNum(void)
{
    return UI_KeyNumber;
}

/**
 * @brief  清空按键
 */
extern "C" void Key_Clear(void)
{
    UI_KeyNumber = 0;
}

/**
 * @brief  例程一：原菜单系统演示
 */
static void Gui_MenuSimRun(void)
{
    MSG msg = { 0 };

    LCD_Init();
    UI_Init();

    while (true) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        UI_FlushFlag = ENABLE;
        UI_While();
        Sleep(10);
    }
}

/**
 * @brief  模拟器启动页：选择例程
 * @note   Q = 菜单系统例程；S = 页面跳转例程
 */
static void Gui_DemoSelectRun(void)
{
    LCD_Init();

    while (true) {
        MSG msg = { 0 };

        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        LCD_Clear();
        LCD_ShowString(0, 0, "SELECT DEMO");
        LCD_ShowString(0, 32, "Q Menu System");
        LCD_ShowString(0, 48, "S Page Navigate");

        if (Key_Scan() == ENABLE) {
            uint8_t key = Key_GetNum();
            Key_Clear();
            if (key == UI_KEY_ENTER) {   /* Q */
                Gui_MenuSimRun();
                return;
            }
            if (key == UI_KEY_ITEM_DOWN) { /* S */
                Gui_PageNavSimRun();
                return;
            }
        }
        Sleep(10);
    }
}

/**
 * @brief  Win32 程序入口
 */
int WINAPI WinMain(_In_ HINSTANCE hInstance,
                   _In_opt_ HINSTANCE hPrevInstance,
                   _In_ LPSTR lpCmdLine,
                   _In_ int nShowCmd)
{
    WNDCLASSA wc = { 0 };

    (void)hPrevInstance;
    (void)lpCmdLine;

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "MenuSimWindow";
    RegisterClassA(&wc);

    /* 创建窗口并模拟 LCD */
    s_hWnd = CreateWindowA("MenuSimWindow", "Menu",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           UI_SIZE_WIDTH + 16,
                           UI_SIZE_HEIGHT + 39,
                           NULL, NULL, hInstance, NULL);
    ShowWindow(s_hWnd, nShowCmd);
    UpdateWindow(s_hWnd);

    /* 同一个模拟器：启动后选择要运行的两个例程 */
    Gui_DemoSelectRun();
    return 0;
}
