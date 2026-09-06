/**
 * @file    hex_protocol.c
 * @brief   HEX 数据包协议：组帧、接收状态机、表驱动命令分发
 * @details 帧格式：
 *          0x55 0xAA CMD LEN DATA... CHECK 0xFF 0xFE
 *          校验码 = CMD + LEN + DATA 各字节之和（取低 8 位）。
 */

#include <string.h>
#include "hex_protocol.h"

/**
 * @brief  复位解析器
 * @param  pParser 解析器句柄
 */
void Hex_ParserReset(HexParser_T *pParser)
{
    if (pParser == NULL) {
        return;
    }
    memset(pParser, 0, sizeof(HexParser_T));
    pParser->state = HEX_STATE_WAIT_HEAD1;
}

/**
 * @brief  在命令表中查找回调
 * @param  commandId 命令 ID
 * @param  pTable    命令表
 * @retval 回调函数；未找到返回 NULL
 */
static CmdHexCallback_T Hex_FindCallback(uint8_t commandId,
                                         const CmdHexTable_T *pTable)
{
    uint8_t index = 0;

    if ((pTable == NULL) || (pTable->pTable == NULL)) {
        return NULL;
    }
    for (index = 0; index < pTable->count; index++) {
        if (pTable->pTable[index].commandId == commandId) {
            return pTable->pTable[index].callback;
        }
    }
    return NULL;
}

/**
 * @brief  向解析器喂入一个字节
 * @param  pParser 解析器
 * @param  byte    新字节
 * @param  pTable  命令表
 * @retval 1-本次收到完整帧并已分发；0-尚未收完
 */
uint8_t Hex_ParserFeed(HexParser_T *pParser,
                       uint8_t byte,
                       const CmdHexTable_T *pTable)
{
    CmdHexCallback_T callback = NULL;

    if (pParser == NULL) {
        return 0u;
    }

    switch (pParser->state) {
        case HEX_STATE_WAIT_HEAD1:
            if (byte == HEX_FRAME_HEAD1) {
                pParser->state = HEX_STATE_WAIT_HEAD2;
            }
            break;

        case HEX_STATE_WAIT_HEAD2:
            if (byte == HEX_FRAME_HEAD2) {
                pParser->state = HEX_STATE_WAIT_CMD;
            } else {
                Hex_ParserReset(pParser);
            }
            break;

        case HEX_STATE_WAIT_CMD:
            pParser->commandId = byte;
            pParser->state = HEX_STATE_WAIT_LEN;
            break;

        case HEX_STATE_WAIT_LEN:
            pParser->dataLength = byte;
            pParser->dataIndex = 0u;
            if (byte > HEX_MAX_DATA_LEN) {
                Hex_ParserReset(pParser);   /* 长度非法 */
            } else if (byte == 0u) {
                pParser->state = HEX_STATE_WAIT_CHECK;
            } else {
                pParser->state = HEX_STATE_WAIT_DATA;
            }
            break;

        case HEX_STATE_WAIT_DATA:
            pParser->data[pParser->dataIndex++] = byte;
            if (pParser->dataIndex >= pParser->dataLength) {
                pParser->state = HEX_STATE_WAIT_CHECK;
            }
            break;

        case HEX_STATE_WAIT_CHECK:
            pParser->checkCode = byte;
            pParser->state = HEX_STATE_WAIT_TAIL1;
            break;

        case HEX_STATE_WAIT_TAIL1:
            if (byte == HEX_FRAME_TAIL1) {
                pParser->state = HEX_STATE_WAIT_TAIL2;
            } else {
                Hex_ParserReset(pParser);
            }
            break;

        case HEX_STATE_WAIT_TAIL2:
            if (byte == HEX_FRAME_TAIL2) {
                uint8_t calcCheck = pParser->commandId + pParser->dataLength;
                uint16_t index = 0;

                for (index = 0; index < pParser->dataLength; index++) {
                    calcCheck = (uint8_t)(calcCheck + pParser->data[index]);
                }

                if (calcCheck == pParser->checkCode) {
                    callback = Hex_FindCallback(pParser->commandId, pTable);
                    if (callback != NULL) {
                        callback(0u,
                                 pParser->commandId,
                                 pParser->data,
                                 pParser->dataLength);
                    }
                }
                Hex_ParserReset(pParser);
                return 1u;
            }
            Hex_ParserReset(pParser);
            break;

        default:
            Hex_ParserReset(pParser);
            break;
    }
    return 0u;
}

/**
 * @brief  组装一帧 HEX 数据
 * @param  commandId     命令 ID
 * @param  pData         数据
 * @param  dataLength    数据长度
 * @param  pFrame        输出帧缓冲
 * @param  frameCapacity 输出缓冲容量
 * @retval 帧长；空间不足返回 0
 */
uint16_t Hex_BuildFrame(uint8_t commandId,
                        const uint8_t *pData,
                        uint16_t dataLength,
                        uint8_t *pFrame,
                        uint16_t frameCapacity)
{
    uint16_t length = 0u;
    uint8_t check = 0u;
    uint16_t index = 0u;

    if ((pData == NULL) && (dataLength != 0u)) {
        return 0u;
    }
    if (frameCapacity < dataLength + 7u) {
        return 0u;
    }

    pFrame[length++] = HEX_FRAME_HEAD1;
    pFrame[length++] = HEX_FRAME_HEAD2;
    pFrame[length++] = commandId;
    pFrame[length++] = (uint8_t)dataLength;

    check = (uint8_t)(commandId + dataLength);
    for (index = 0u; index < dataLength; index++) {
        pFrame[length++] = pData[index];
        check = (uint8_t)(check + pData[index]);
    }

    pFrame[length++] = check;
    pFrame[length++] = HEX_FRAME_TAIL1;
    pFrame[length++] = HEX_FRAME_TAIL2;
    return length;
}
