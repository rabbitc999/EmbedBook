#ifndef __CMD_H
#define __CMD_H

#include <stdint.h>

#define COM_COUNT 8
#define COM1 0
#define COM2 1
#define COM3 2
#define COM4 3
#define COM5 4
#define COM6 5
#define COM7 6

typedef struct __CmdControl_T
{
    uint8_t comBoardCast;
    uint8_t hexStrictMode;
    uint8_t receiveMode;
    uint16_t comBoardCastDeltaTime[COM_COUNT];
} CmdControl_T;

extern CmdControl_T g_CMD;

#endif /* __CMD_H */
