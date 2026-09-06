#ifndef __HEX_PROTOCOL_H
#define __HEX_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HEX_MAX_DATA_LEN   64u
#define HEX_FRAME_HEAD1    0x55u
#define HEX_FRAME_HEAD2    0xAAu
#define HEX_FRAME_TAIL1    0xFFu
#define HEX_FRAME_TAIL2    0xFEu

/* 命令回调类型 */
typedef void (*CmdHexCallback_T)(uint16_t comIndex,
                                 uint8_t commandId,
                                 uint8_t *pData,
                                 uint16_t dataLength);

/* 命令表节点 */
typedef struct __CmdHexNode_T
{
    uint8_t commandId;
    CmdHexCallback_T callback;
} CmdHexNode_T;

/* 命令表 */
typedef struct __CmdHexTable_T
{
    uint8_t count;
    CmdHexNode_T *pTable;
} CmdHexTable_T;

/* HEX 接收状态 */
typedef enum __HexParserState_T
{
    HEX_STATE_WAIT_HEAD1 = 0,
    HEX_STATE_WAIT_HEAD2,
    HEX_STATE_WAIT_CMD,
    HEX_STATE_WAIT_LEN,
    HEX_STATE_WAIT_DATA,
    HEX_STATE_WAIT_CHECK,
    HEX_STATE_WAIT_TAIL1,
    HEX_STATE_WAIT_TAIL2
} HexParserState_T;

/* HEX 解析器 */
typedef struct __HexParser_T
{
    HexParserState_T state;
    uint8_t commandId;
    uint8_t data[HEX_MAX_DATA_LEN];
    uint16_t dataLength;
    uint16_t dataIndex;
    uint8_t checkCode;
} HexParser_T;

/* 复位解析器 */
void Hex_ParserReset(HexParser_T *pParser);

/* 向解析器喂一个字节；收到完整帧时查表并调用回调，返回 1 */
uint8_t Hex_ParserFeed(HexParser_T *pParser,
                       uint8_t byte,
                       const CmdHexTable_T *pTable);

/* 在缓冲区中组装一帧 HEX 数据，返回帧长 */
uint16_t Hex_BuildFrame(uint8_t commandId,
                        const uint8_t *pData,
                        uint16_t dataLength,
                        uint8_t *pFrame,
                        uint16_t frameCapacity);

#ifdef __cplusplus
}
#endif

#endif /* __HEX_PROTOCOL_H */
