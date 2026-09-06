#include <stdio.h>
#include <string.h>
#include "serial_fifo.h"
#include "hex_protocol.h"

static int s_FailCount = 0;

static void Check(int ok, const char *pName)
{
    if (ok) {
        printf("[PASS] %s\n", pName);
    } else {
        printf("[FAIL] %s\n", pName);
        s_FailCount++;
    }
}

/* ---------- FIFO 测试 ---------- */
static void Test_Fifo(void)
{
    uint8_t buffer[8];
    uint8_t readBuffer[8];
    SerialFifo_T fifo;
    uint8_t writeData[6] = { 1u, 2u, 3u, 4u, 5u, 6u };
    uint32_t readLen = 0u;

    Serial_FifoInit(&fifo, buffer, sizeof(buffer));

    Check(Serial_GetFifoLength(&fifo) == 0u, "FIFO：初始为空");
    Check(Serial_FifoWriteBuffer(&fifo, writeData, 6u) == 6u,
          "FIFO：写入 6 字节");
    Check(Serial_FifoWriteBuffer(&fifo, writeData, 4u) == 0u,
          "FIFO：按 Size-1 容量计算，空间不足拒绝写入");

    memset(readBuffer, 0, sizeof(readBuffer));
    readLen = Serial_FifoReadBuffer(&fifo, readBuffer, 2u);
    Check((readLen == 2u) && (readBuffer[0] == 1u) && (readBuffer[1] == 2u),
          "FIFO：先读 2 字节");

    /* 剩余容量为 Size-1-used=3，写 3 字节会触发回绕 */
    Check(Serial_FifoWriteBuffer(&fifo, writeData, 3u) == 3u,
          "FIFO：写入触发回绕");

    memset(readBuffer, 0, sizeof(readBuffer));
    readLen = Serial_FifoReadBuffer(&fifo, readBuffer, sizeof(readBuffer));
    Check(readLen == 7u, "FIFO：一次性读满 Size-1 字节");
    Check((readBuffer[0] == 3u) && (readBuffer[6] == 3u),
          "FIFO：回绕后数据顺序正确");
    Check(Serial_GetFifoLength(&fifo) == 0u, "FIFO：读取后为空");
}

/* ---------- HEX 回调 ---------- */
static uint8_t s_LedCount = 0u;
static uint8_t s_SensorCount = 0u;

static void Cmd_LedControl(uint16_t comIndex,
                           uint8_t commandId,
                           uint8_t *pData,
                           uint16_t dataLength)
{
    (void)comIndex;
    (void)commandId;
    printf("HEX：LED 命令 data[0]=%u len=%u\n", pData[0], dataLength);
    s_LedCount++;
}

static void Cmd_ReadSensor(uint16_t comIndex,
                           uint8_t commandId,
                           uint8_t *pData,
                           uint16_t dataLength)
{
    (void)comIndex;
    (void)commandId;
    (void)pData;
    (void)dataLength;
    printf("HEX：读取传感器\n");
    s_SensorCount++;
}

static void Test_Hex(void)
{
    CmdHexNode_T table[] = {
        { 0x01u, Cmd_LedControl },
        { 0x02u, Cmd_ReadSensor }
    };
    CmdHexTable_T hexTable = {
        sizeof(table) / sizeof(table[0]),
        table
    };
    HexParser_T parser;
    uint8_t frame[32];
    uint8_t payload[3] = { 0x01u, 0x02u, 0x03u };
    uint16_t length = 0u;
    uint16_t index = 0u;

    Hex_ParserReset(&parser);

    length = Hex_BuildFrame(0x01u, payload, sizeof(payload),
                            frame, sizeof(frame));
    Check(length == sizeof(payload) + 7u, "HEX：组帧长度正确");
    Check((frame[0] == 0x55u) && (frame[1] == 0xAAu), "HEX：帧头正确");
    Check((frame[length - 2u] == 0xFFu) &&
          (frame[length - 1u] == 0xFEu), "HEX：帧尾正确");

    for (index = 0u; index < length; index++) {
        Hex_ParserFeed(&parser, frame[index], &hexTable);
    }
    Check(s_LedCount == 1u, "HEX：LED 命令回调被调用");

    /* 校验码错误应被丢弃 */
    frame[length - 3u] ^= 0xFFu;
    for (index = 0u; index < length; index++) {
        Hex_ParserFeed(&parser, frame[index], &hexTable);
    }
    Check(s_LedCount == 1u, "HEX：错误校验码不会再次触发回调");
}

int main(void)
{
    Test_Fifo();
    Test_Hex();

    if (s_FailCount == 0) {
        printf("\n第4章代码示例测试全部通过\n");
        return 0;
    }
    printf("\n第4章代码示例测试失败 %d 项\n", s_FailCount);
    return 1;
}
