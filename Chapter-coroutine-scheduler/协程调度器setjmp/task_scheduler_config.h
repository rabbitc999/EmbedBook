/**
  **********************************************************************************************************************
  * @file    task_scheduler_config.h
  * @brief   任务调度器用户配置文件
  * @author  const_zpc (adapted)
  * @date    2026-09-05
  **********************************************************************************************************************
  */

#ifndef __TASK_SCHEDULER_CONFIG_H
#define __TASK_SCHEDULER_CONFIG_H

/* 最大独立栈任务数 */
#define TASK_MAX_UNIQUE_TASK              2

/* 最大共享栈任务数 */
#define TASK_MAX_SHARED_TASK              6

#if (TASK_MAX_SHARED_TASK > 0)
/* 单个共享栈任务栈备份空间大小（字节） */
#define TASK_SHARED_BAK_STACK_SIZE        256

/* 共享栈任务切换预留栈空间大小（字节） */
#define TASK_SHARED_JMP_STACK_SIZE        256
#endif

/* 任务名称最大长度（含终止符） */
#define TASK_NAME_LENGTH                  16

/* 禁止中断（需用户实现） */
#define TASK_DISABLE_IRQ()

/* 使能中断（需用户实现） */
#define TASK_ENABLE_IRQ()

#if (TASK_MAX_UNIQUE_TASK != 0)
/* 设置栈指针（需根据平台实现） */
#define TASK_SET_STACK(p)        __asm__ volatile("mov %0, %%rsp" : : "r" (p) : "memory");
#endif

#endif /* __TASK_SCHEDULER_CONFIG_H */