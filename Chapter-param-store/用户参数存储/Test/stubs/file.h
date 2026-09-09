#ifndef __FILE_H
#define __FILE_H

#include <stdint.h>

/* PC 测试桩：FatFS 文件读写 */
uint32_t FatFS_ReadFile(const char *pFilePath,
                        uint8_t *pBuffer,
                        uint32_t bufferSize);

uint32_t FatFS_WriteFile(const char *pFilePath,
                         const uint8_t *pBuffer,
                         uint32_t bufferSize);

#endif /* __FILE_H */
