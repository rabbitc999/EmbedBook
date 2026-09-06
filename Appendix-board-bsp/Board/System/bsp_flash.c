#include "bsp_system.h"  // Device header

/**
 * @brief FLASH读取一个32位的字
 * @param Address 要读取数据的字地址
 * @return 指定地址下的数据
 */
uint32_t MyFLASH_ReadWord(uint32_t Address)
{
    return *((__IO uint32_t *)(Address));  // 使用指针访问指定地址下的数据并返回
}

/**
 * @brief FLASH读取一个16位的半字
 * @param Address 要读取数据的半字地址
 * @return 指定地址下的数据
 */
uint16_t MyFLASH_ReadHalfWord(uint32_t Address)
{
    return *((__IO uint16_t *)(Address));  // 使用指针访问指定地址下的数据并返回
}

/**
 * @brief FLASH读取一个8位的字节
 * @param Address 要读取数据的字节地址
 * @return 指定地址下的数据
 */
uint8_t MyFLASH_ReadByte(uint32_t Address)
{
    return *((__IO uint8_t *)(Address));  // 使用指针访问指定地址下的数据并返回
}

/**
 * @brief FLASH全擦除
 * @param 无
 * @return 无
 * 说    明：调用此函数后，FLASH的所有页都会被擦除，包括程序文件本身，擦除后，程序将不复存在
 */
void MyFLASH_EraseAllPages(void)
{
    FLASH_EraseInitTypeDef sEraseInit;
    uint32_t PageError = 0;
    sEraseInit.TypeErase = FLASH_TYPEERASE_MASSERASE;  // 全擦除
    sEraseInit.Banks = FLASH_BANK_1;                   // 擦除第1个存储区
    sEraseInit.PageAddress = 0;                        // 起始页地址
    HAL_FLASH_Unlock();                                // 解锁
    HAL_FLASHEx_Erase(&sEraseInit, NULL);              // 全擦除
}

/**
 * @brief FLASH页擦除
 * @param PageAddress 要擦除页的页地址
 * @return 无
 */
void MyFLASH_ErasePage(uint32_t PageAddress)
{
    FLASH_EraseInitTypeDef sEraseInit;
    uint32_t PageError = 0;

    sEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;  // 页擦除
    sEraseInit.Banks = FLASH_BANK_1;               // 擦除第1个存储区
    sEraseInit.PageAddress = PageAddress;          // 要擦除的页地址
    sEraseInit.NbPages = 1;                        // 擦除1页

    HAL_FLASH_Unlock();                          // 解锁
    HAL_FLASHEx_Erase(&sEraseInit, &PageError);  // 执行页擦除
}

/**
 * @brief FLASH编程字
 * @param Address 要写入数据的字地址
 * @param Data 要写入的32位数据
 * @return 无
 */
void MyFLASH_ProgramWord(uint32_t Address, uint32_t Data)
{
    HAL_FLASH_Unlock();                                        // 解锁
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Address, Data);  // 编程字
    HAL_FLASH_Lock();                                          // 加锁
}

/**
 * @brief FLASH编程半字
 * @param Address 要写入数据的半字地址
 * @param Data 要写入的16位数据
 * @return 无
 */
void MyFLASH_ProgramHalfWord(uint32_t Address, uint16_t Data)
{
    HAL_FLASH_Unlock();                                            // 解锁
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, Address, Data);  // 编程半字
    HAL_FLASH_Lock();                                              // 加锁
}
