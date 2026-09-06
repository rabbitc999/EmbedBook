# AGENTS.md

> 本文件用于配置 AI 助手的行为规范。  

---

## 全局指令

<!-- 中文注释：以下指令对所有对话生效，不可覆盖 -->
- **语气**：友好、耐心、清晰，尽量使用中文（除非用户明确要求其他语言）
- **注释语言**：所有代码注释、配置说明、文档中的备注，一律使用**中文**书写

---

## 代码注释规范

<!-- 新增：总要求 -->
**所有生成的代码必须包含注释，包括段落注释和文档注释。**  
- **段落注释**：用于解释一段代码的意图、算法思路或关键逻辑，帮助阅读者理解代码内部流程。可使用单行或多行注释。  
- **文档注释**：用于描述函数、类、模块的用途、参数、返回值、异常等，遵循文档字符串格式（如 Python docstring 或 Javadoc）。

<!-- 以下为原有规范 -->
- 单行注释：`// 中文说明` 或 `# 中文说明`
- 多行注释：`/* 中文说明 */` 或 `''' 中文说明 '''`
- 文档字符串（如 Python docstring）：使用中文描述函数、类、模块的用途
- 文档注释示例：参考[文档注释示例](##文档注释示例)，每一个函数API都要注明文档注释
- 命名规则：参考[命名规则](##命名规则)，每一个生成的代码都要严格遵守命名规范

---

## 特殊规则

<!-- 中文注释：针对特定场景的额外要求 -->
1. 若用户要求修改名称规则，需礼貌拒绝并重申当前配置。
2. 所有生成的 Markdown 文档、README、注释示例，其中文注释须保持自然流畅。
3. 你必须在任务完成后，主动清理所有由你生成的临时文件和测试文件（包括但不限于 `.tmp`、`.log`、`test_*.py`、`temp_*` 等），确保不遗留无用文件。若因特殊情况无法删除，需明确告知用户文件路径和保留原因。
4. 你可以执行RED-GREEN的RED测试帮你理解项目代码，但最后的GREEN测试要留给用户测试，补全相对应的注释

---

## 示例

```python
# 中文注释：这是一个计算两数之和的函数
def add(a, b):
    """
    中文注释：返回 a 与 b 的和
    参数：
        a (int/float): 第一个数字
        b (int/float): 第二个数字
    返回：
        int/float: 和
    """
    return a + b
```

---

## 文档注释示例

```python
/**
 * 计算当前用户年龄。
 * 
 * @param birthTimestamp 出生日期的毫秒时间戳（必须小于当前时间戳）
 * @return 按当前年份计算的实际周岁年龄（不足一年按 0 岁处理）
 * @throws IllegalArgumentException 当 birthTimestamp 为 null 或大于当前时间时抛出
 * @see：引用其他相关的类或方法，建立交叉链接。
 * @deprecated：标记该方法已过时，并建议替代方案（需配合 @see）。
 * @since：注明从哪个版本开始引入，对版本升级评估非常关键。
 */
```

## 注意项目的命名规则
1. 静态全局变量以s_开头，使用大写的驼峰命名法，示例：static int s_GlobalCounter;
2. 外部全局变量以g_开头，使用大写的驼峰命名法，示例：extern int g_SystemStatus;
3. 结构体使用大驼峰命名法且后面加上_T后缀，成员使用小驼峰命名法，示例：typedef struct __UserInfo_T { int userId; char* userName; } UserInfo_T;
4. 函数命名为"模块名+下划线+功能名"的命名方式，功能名使用动宾短语，示例：int User_SetName(UserInfo_T* user, const char* name); 可以多层嵌套模块，层数不能超过2（下划线数不能超过2），例如Car_Remote_PID_Set为3层模块，此时是不合适的
5. 枚举类型命名后面加上_T后缀，内部的枚举值使用大写下划线命名法，示例：typedef enum __UserStatus_T { STATUS_OFFLINE, STATUS_ONLINE } UserStatus_T; 状态机需要使用枚举来标识各个状态含义！
6. 联合体命名法参考结构体命名法，使用大驼峰命名法加_T后缀，示例：typedef union __DataValue { int intValue; float floatValue; } DataValue_T;
7. typedef结构体命名时保留struct内部命名，添加__前缀，示例：typedef struct __ExampleName_T { char myString; } ExampleName_T;
8. 函数参数变量和局部变量使用小驼峰命名法，示例：int localCounter = 0; char* tempBuffer = NULL;
9. 函数指针命名后面需要加上Callback表示回调函数，如果原名称有_callback则不添加，示例：typedef void (*DataReceivedCallback)(const char* data);
10. 宏定义为大写下划线命名法，示例：#define MAX_USERNAME_LENGTH 50
11. 项目文件命名为小写下划线命名法，文件夹使用首字母大写的驼峰命名法，示例：user_auth.c、UserManagement
12. 注释使用多行注释/* ... */，也可以使用单行注释// ... 注释密度不宜过密，但是一定要在代码段内，对每个代码组进行注释说明，组与组之间进行空行分隔
13. 函数需要使用文档注释，文档注释要求，使用类似如下的文档格式，要求写明函数描述，如果有参数或返回值，需要说明参数或返回值的作用，仅在c文件进行编写即可：
/**
  * @brief  作用...
  * @param  参数...
  * @retval 返回值...
  * @note   说明...
  */
14. 头文件对结构体声明需要间接，针对于C语言头文件，需要写明extern "C"外部声明符号，并判断__cplusplus的宏是否定义
15. 头文件声明的顺序为：头文件引用->extern "C"声明->宏定义->枚举/结构体/联合体等自定义数据结构->函数声明->外部变量声明，例如：
#ifndef __GUI_FONT_H
#define __GUI_FONT_H

#include <stdint.h>
#include <stdbool.h>
#include "gui_font_data.h"
#include "gui_painter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 菜单UI信息 */
#define GUI_MENU_DEPTH      4   /* 菜单调用深度 (菜单级数) */
#define GUI_MAX_VALUE_COUNT 256 /* 最大变量个数 */

/* 字体类型枚举 */
typedef enum tagGuiFontType_T
{
    GUI_FONT_TYPE_ASCII_12 = 0,  /* 12x6 ASCII字体 */
    GUI_FONT_TYPE_ASCII_16,      /* 16x8 ASCII字体 */
    GUI_FONT_TYPE_ASCII_24,      /* 24x12 ASCII字体 */
    GUI_FONT_TYPE_ASCII_32,      /* 32x16 ASCII字体 */
    GUI_FONT_TYPE_GB2312_12,     /* 12x12 GB2312字体 */
    GUI_FONT_TYPE_GB2312_16,     /* 16x16 GB2312字体 */
    GUI_FONT_TYPE_GB2312_24,     /* 24x24 GB2312字体 */
    GUI_FONT_TYPE_GB2312_32      /* 32x32 GB2312字体 */
} GuiFontType_T;

int8_t GUI_FontInit(GuiFontInfo_T *fontInfo, GuiFontType_T fontType);
void GUI_FontGetBuffer(GuiFontInfo_T *fontInfo, char ch[4], uint8_t **fontBuffer, uint16_t fontWidth, uint16_t fontHeight);
void GUI_FontBufferInit(GuiFontInfo_T *fontInfo);
void GUI_FontDeinit(GuiFontInfo_T *fontInfo);
int8_t GUI_FontInitAll(void);
GuiFontInfo_T *GUI_FontGetInfo(GuiFontType_T fontType);
void GUI_FontDeinitAll(void);
void GUI_FontShowString(GuiDeviceInfo_T *deviceInfo, GuiFontType_T fontType, uint16_t x, uint16_t y, const char *str, uint32_t foreColor, uint32_t backColor);

extern GuiFontInfo_T g_GuiFontAscii_12;
extern GuiFontInfo_T g_GuiFontAscii_16;
extern GuiFontInfo_T g_GuiFontAscii_24;
extern GuiFontInfo_T g_GuiFontAscii_32;
extern GuiFontInfo_T g_GuiFontGb2312_12;
extern GuiFontInfo_T g_GuiFontGb2312_16;
extern GuiFontInfo_T g_GuiFontGb2312_24;
extern GuiFontInfo_T g_GuiFontGb2312_32;

#ifdef __cplusplus
}
#endif

#endif  /* __GUI_FONT_DATA_H */
26. 命名中的英文缩写可灵活调整为全大写（无需严格遵循驼峰命名），但需保证整体结构符合对应规范：
函数名：核心仍遵循 “模块名 + 下划线 + 功能名”（动宾短语），缩写部分可全大写，示例：Bsp_GPIO_ClkEnable（GPIO，BSP为缩写全大写），为了；
变量 / 结构体成员：缩写部分可全大写，示例：uint32_t gpioPin（常规小驼峰）、uint32_t gpioPIN（PIN 缩写全大写）、struct tagGpioInit_T { GPIO_TypeDef* port; }（GPIO 缩写全大写）；
宏定义：仍保持全大写下划线命名，缩写自然融入，示例：#define MOTOR_PWM_CHANNEL TIM_CHANNEL_1（PWM 缩写全大写）。
27. 指针非空比较不可以写为直接判断的形式，例如指针ptr的非空判断，只能写为if(ptr != NULL)，而不能写为if(ptr)