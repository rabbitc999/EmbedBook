/**
 * @file    win32_sim.cpp
 * @brief   菜单系统 Win32 API（GDI）PC 仿真层
 * @details 用纯 Win32 窗口模拟 240x300 LCD，并实现 hardware.h 中的
 *          LCD/按键接口；菜单核心代码保持与单片机一致。
 *          本文件只负责“菜单系统”例程，页面跳转仿真见独立目录。
 */

#include <windows.h>
#include <string.h>
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
 * @brief  LCD 初始化
 */
extern "C" void LCD_Init(void)
{
    s_hdc = GetDC(s_hWnd);
    s_whiteBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);

    SetBkMode(s_hdc, TRANSPARENT);
    SetTextColor(s_hdc, RGB(0, 0, 0));

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
            LCD_DrawPoint((uint16_t)(x + col),
                          (uint16_t)(y + row),
                          pImage[row * width + col]);
        }
    }
}

/**
 * @brief  按键扫描
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
 * @brief  Win32 程序入口
 */
int WINAPI WinMain(_In_ HINSTANCE hInstance,
                   _In_opt_ HINSTANCE hPrevInstance,
                   _In_ LPSTR lpCmdLine,
                   _In_ int nShowCmd)
{
    WNDCLASSA wc = { 0 };
    MSG msg = { 0 };

    (void)hPrevInstance;
    (void)lpCmdLine;

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "MenuSimWindow";
    RegisterClassA(&wc);

    s_hWnd = CreateWindowA("MenuSimWindow", "Menu System Sim",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           UI_SIZE_WIDTH + 16,
                           UI_SIZE_HEIGHT + 39,
                           NULL, NULL, hInstance, NULL);
    ShowWindow(s_hWnd, nShowCmd);
    UpdateWindow(s_hWnd);

    /* 菜单系统主循环 */
    LCD_Init();
    UI_Init();

    while (true) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        UI_FlushFlag = ENABLE;
        UI_While();
        Sleep(10);
    }
}
