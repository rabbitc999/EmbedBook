/**
 * @file    win32_page_sim.cpp
 * @brief   页面跳转框架独立 Win32 API（GDI）PC 仿真
 * @details 演示“主页 -> 菜单页 -> 曲线页”的显式页面跳转，
 *          并展示 RGB565 图像绘制与 LCD_DrawPoint 点绘制。
 */

#include <windows.h>
#include <string.h>
#include <math.h>
#include "hardware.h"

/* 模拟按键：与菜单系统保持一致的键盘映射 */
#define PAGE_KEY_ENTER    'Q'
#define PAGE_KEY_EXIT     'A'
#define PAGE_KEY_UP       'W'
#define PAGE_KEY_DOWN     'S'
#define PAGE_KEY_VAL_UP   'E'
#define PAGE_KEY_VAL_DOWN 'D'
#define PAGE_KEY_NONE     0

static HWND s_hWnd = NULL;
static HDC s_hdc = NULL;
static HBRUSH s_whiteBrush = NULL;
static uint8_t s_KeyNumber = PAGE_KEY_NONE;

/* 页面入口类型与导航器 */
typedef void (*PageEntry_T)(void *param);

typedef struct __PageNavigator_T
{
    PageEntry_T currentPage;
    void *currentParam;
} PageNavigator_T;

static PageNavigator_T s_Navigator;
static uint8_t s_Key = PAGE_KEY_NONE;

/**
 * @brief  窗口过程
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

    HFONT font = CreateFontA(PAGE_CHAR_H, 0,
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
    RECT rect = { 0, 0, PAGE_SIM_WIDTH, PAGE_SIM_HEIGHT };
    FillRect(s_hdc, &rect, s_whiteBrush);
}

/**
 * @brief  显示字符串，先填充字符区域再绘制
 */
extern "C" void LCD_ShowString(uint16_t x, uint16_t y, const char *str)
{
    int length = (int)strlen(str);
    RECT rect = {
        (int)x,
        (int)y,
        (int)x + length * PAGE_CHAR_W,
        (int)y + PAGE_CHAR_H
    };
    FillRect(s_hdc, &rect, s_whiteBrush);
    TextOutA(s_hdc, (int)x, (int)y, str, length);
}

/**
 * @brief  RGB565 转 Win32 COLORREF
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
 * @brief  绘制单个点
 */
extern "C" void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t rgb565)
{
    if ((s_hdc == NULL) ||
        (x >= PAGE_SIM_WIDTH) || (y >= PAGE_SIM_HEIGHT)) {
        return;
    }
    SetPixelV(s_hdc, (int)x, (int)y, Rgb565ToColorRef(rgb565));
}

/**
 * @brief  显示 RGB565 图像
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
 * @brief  页面跳转
 */
static void GUI_JumpPage(PageEntry_T page, void *param)
{
    s_Navigator.currentPage = page;
    s_Navigator.currentParam = param;
    page(param);
}

static void GUI_HomePage(void *param);
static void GUI_MenuPage(void *param);
static void GUI_CurvePage(void *param);

/**
 * @brief  主页：RGB565 图像演示
 */
static void GUI_HomePage(void *param)
{
    uint16_t image[8][8];
    uint16_t row = 0;
    uint16_t col = 0;

    (void)param;
    LCD_Clear();
    LCD_ShowString(0, 0, "HOME PAGE");
    LCD_ShowString(0, 24, "Q -> Menu");

    for (row = 0; row < 8; row++) {
        for (col = 0; col < 8; col++) {
            image[row][col] = ((row + col) & 1u)
                              ? 0xF800u  /* 红色 */
                              : 0x001Fu; /* 蓝色 */
        }
    }
    LCD_ShowRGB565Image(80, 60, 8, 8, &image[0][0]);

    if (s_Key == PAGE_KEY_ENTER) {
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

    if (s_Key == PAGE_KEY_ENTER) {
        GUI_JumpPage(GUI_CurvePage, NULL);
    }
    if (s_Key == PAGE_KEY_EXIT) {
        GUI_JumpPage(GUI_HomePage, NULL);
    }
}

/**
 * @brief  曲线页：LCD_DrawPoint 绘制正弦曲线
 */
static void GUI_CurvePage(void *param)
{
    uint16_t x = 0;

    (void)param;
    LCD_Clear();
    LCD_ShowString(0, 0, "CURVE PAGE");

    for (x = 0; x < PAGE_SIM_WIDTH; x += 2) {
        int y = 120 - (int)(60.0 * sin((double)x / 30.0));
        if ((y >= 0) && (y < PAGE_SIM_HEIGHT)) {
            LCD_DrawPoint(x, (uint16_t)y, 0x07E0u); /* 绿色 */
        }
    }

    LCD_ShowString(0, PAGE_SIM_HEIGHT - 24, "A -> Menu");
    if (s_Key == PAGE_KEY_EXIT) {
        GUI_JumpPage(GUI_MenuPage, NULL);
    }
}

/**
 * @brief  按键扫描（松开上报）
 */
static uint8_t Key_Scan(void)
{
    static uint8_t lastState = PAGE_KEY_NONE;
    uint8_t keys[] = {
        PAGE_KEY_ENTER,
        PAGE_KEY_EXIT,
        PAGE_KEY_UP,
        PAGE_KEY_DOWN,
        PAGE_KEY_VAL_UP,
        PAGE_KEY_VAL_DOWN
    };
    uint8_t currentState = PAGE_KEY_NONE;

    for (uint8_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (GetAsyncKeyState((int)keys[i]) & 0x8000) {
            currentState = keys[i];
            break;
        }
    }

    if ((lastState != PAGE_KEY_NONE) && (currentState == PAGE_KEY_NONE)) {
        s_KeyNumber = lastState;
        lastState = PAGE_KEY_NONE;
        return 1;
    }
    lastState = currentState;
    return 0;
}

/**
 * @brief  Win32 页面跳转仿真入口
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
    wc.lpszClassName = "PageNavSimWindow";
    RegisterClassA(&wc);

    s_hWnd = CreateWindowA("PageNavSimWindow", "Page Navigation Sim",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           PAGE_SIM_WIDTH + 16,
                           PAGE_SIM_HEIGHT + 39,
                           NULL, NULL, hInstance, NULL);
    ShowWindow(s_hWnd, nShowCmd);
    UpdateWindow(s_hWnd);

    LCD_Init();
    GUI_JumpPage(GUI_HomePage, NULL);

    while (true) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        s_Key = PAGE_KEY_NONE;
        if (Key_Scan() != 0) {
            s_Key = s_KeyNumber;
            s_KeyNumber = PAGE_KEY_NONE;
        }
        if (s_Navigator.currentPage != NULL) {
            s_Navigator.currentPage(s_Navigator.currentParam);
        }
        Sleep(10);
    }
}
