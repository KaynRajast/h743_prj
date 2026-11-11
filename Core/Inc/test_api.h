/*
 * @Author: zhengyj 1814634121@qq.com
 * @Date: 2026-11-05 10:11:55
 * @LastEditors: zhengyj 1814634121@qq.com
 * @LastEditTime: 2026-11-05 14:29:06
 * @FilePath: \h743_prj\Core\Inc\test_api.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef TEST_API_H
#define TEST_API_H

#include "FreeRTOS.h"
#include "timers.h"
#include "stdint.h"
#include "gpio.h"
#include "stdio.h"

#define LPTIM_COUNTS_PER_TICK   (32000 / configTICK_RATE_HZ)

// 声明两个软件定时器句柄
extern TimerHandle_t xAutoReloadTimer;
extern TimerHandle_t xOneShotTimer;


void TimerCallback_1s(TimerHandle_t xTimer);
void TimerCallBack_3s(TimerHandle_t xTimer);
void PreSleepProcessing(uint32_t ulExpectedIdleTime);
void PostSleepProcessing(uint32_t ulExpectedIdleTime);
void LedTask(void *Parameters);
void prtTask(void *Parameters);

#endif
