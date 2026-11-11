#include "test_api.h"
#include "timers.h"
#include "stdint.h"
#include "gpio.h"
#include "main.h"
#include "stdio.h"
#include "lptim.h"
#include "usart.h"
#include "tim.h"


// 声明两个软件定时器句柄
TimerHandle_t xAutoReloadTimer = NULL;  // 周期定时器
TimerHandle_t xOneShotTimer = NULL;     // 单次定时器

// 演示用变量
volatile uint32_t auto_reload_cnt = 0;
volatile uint32_t one_reload_cnt = 0;

extern TIM_HandleTypeDef        htim7;

void TimerCallback_1s(TimerHandle_t xTimer)
{
    auto_reload_cnt++;
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    printf("[INFO] auto_reload_cnt:%d.\r\n", auto_reload_cnt);
}

void TimerCallBack_3s(TimerHandle_t xTimer)
{
    one_reload_cnt++;
    printf("[INFO] one_reload_cnt:%d.\r\n", one_reload_cnt);
}

extern void SystemClock_Config(void);
volatile uint32_t actual_sleep_ticks_record = 0;

#if configUSE_TICKLESS_IDLE == 1

/* 未满1个os tick的lptim余数，避免累计误差 */
static uint32_t s_ulLptimResidual = 0U;

/* 进入stop前；清除唤醒标志，确保不会被一六事件立刻唤醒 */
static void prvClearWakeupFlags(void)
{
    /* 清除lptim的比较匹配、自动重装匹配标志 */
    __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_CMPM | LPTIM_FLAG_ARRM);
    /* 清nvic挂起位 */
    HAL_NVIC_ClearPendingIRQ(LPTIM1_IRQn);
}

void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
{
    uint32_t        ulLptimTicks, ulCntBefore, ulCntAfter, ulActualCnt, ulTotal;
    TickType_t      xSleptTicks;
    eSleepModeStatus eSleepStatus;

    /* 1.把os tick换算成LPTIM计数值 */
    ulLptimTicks = (uint32_t)xExpectedIdleTime * LPTIM_COUNTS_PER_TICK;
    ulLptimTicks += s_ulLptimResidual; /* 加上上一次未满一个tick的计数 */

    /* 超出的截断 */
    if (ulLptimTicks > 0xFF00)
    {
        ulLptimTicks = 0xFF00;
    }

    /* 空闲时间太短就不进入stop模式 */
    if (ulLptimTicks < (configEXPECTED_IDLE_TIME_BEFORE_SLEEP * LPTIM_COUNTS_PER_TICK))
    {
        return;
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)"S\n", 2, 10);

    /* 关中断 */
    __disable_irq();
    __DSB();
    __ISB();

    /* 询问是否真的可以睡 */
    eSleepStatus = eTaskConfirmSleepModeStatus();
    if (eSleepStatus == eAbortSleep)
    {
        __enable_irq();
        return;
    }

    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;


    /* 2.挂起systick，关闭周期中断 */
    HAL_SuspendTick();

    /* 3.情调遗留的唤醒标志 */
    prvClearWakeupFlags();

    ulCntBefore = HAL_LPTIM_ReadCounter(&hlptim1);


    /* 4.启动lptim1中断 */
    HAL_LPTIM_TimeOut_Start_IT(&hlptim1, 0xFFFF, ulLptimTicks);

    __enable_irq();

    /* 5.进入stop模式 */
    HAL_PWREx_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI, PWR_D1_DOMAIN);

    /* 唤醒后从这里开始 */

    __disable_irq();

    /* 6.停止lptim中断，读取实际的计数值 */
    ulCntAfter = HAL_LPTIM_ReadCounter(&hlptim1);
    HAL_LPTIM_TimeOut_Stop_IT(&hlptim1);

    ulActualCnt = (ulCntAfter - ulCntBefore) & 0xFFFFU;

    HAL_ResumeTick();

    /* 恢复正常的工作主频 */
    SystemClock_Config();

    SysTick->VAL = 0U;
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;

    __enable_irq();


    /* 修正tick */
    ulTotal = ulActualCnt + s_ulLptimResidual;
    xSleptTicks = ulTotal / LPTIM_COUNTS_PER_TICK;
    s_ulLptimResidual = ulTotal % LPTIM_COUNTS_PER_TICK; /* 保存余数 */
    
   /* 如果实际休眠比预计多，截断到预期值 */
   if (xSleptTicks > xExpectedIdleTime)
   {
        xSleptTicks = xExpectedIdleTime;
   }

   if (xSleptTicks > 0)
   {
    vTaskStepTick(xSleptTicks);
   }
   HAL_UART_Transmit(&huart2, (uint8_t*)"W\n", 2, 10);
}

#if 0
/* 进入睡眠前的预处理 */
void PreSleepProcessing(uint32_t ulExpectedIdleTime)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)"S\n", 2, 10);
    // 挂起systick
    HAL_SuspendTick();
    
    // 启动lptm1的超时中断
    uint32_t timeout_cnt = ulExpectedIdleTime * LPTIM_COUNTS_PER_TICK;
    if (timeout_cnt > 0xFFFF)
    {
        timeout_cnt = 0xFFFF;
    }

    hlptim1.Instance->CNT = 0;
    actual_sleep_ticks_record = timeout_cnt / LPTIM_COUNTS_PER_TICK;
    HAL_LPTIM_TimeOut_Start_IT(&hlptim1, 0xFFFF, timeout_cnt);

    /* 进入stop模式 */
    HAL_PWREx_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI, PWR_D1_DOMAIN);
    HAL_LPTIM_TimeOut_Stop_IT(&hlptim1);

    // 以下代码唤醒后才会执行
    // SystemClock_Config();

}

/* 从睡眠中唤醒后的处理 */
void PostSleepProcessing(uint32_t ulExpectedIdleTime)
{
    // 以下代码唤醒后才会执行
    // SystemClock_Config();

    // 停止lptime超时中断
    HAL_LPTIM_TimeOut_Stop_IT(&hlptim1);

    // 读取实际睡眠计数值
    uint32_t actual_sleep_cnt = HAL_LPTIM_ReadCounter(&hlptim1);

    // 换算成ticks
    uint32_t actual_sleep_ticks = actual_sleep_cnt / LPTIM_COUNTS_PER_TICK;
    if (actual_sleep_ticks > 0 && actual_sleep_ticks <=  ulExpectedIdleTime)
    {
        vTaskStepTick(actual_sleep_ticks);
    }
    else if (actual_sleep_ticks > ulExpectedIdleTime)
    {
        vTaskStepTick(ulExpectedIdleTime);
    }

    // 恢复systick
    HAL_ResumeTick();
    HAL_UART_Transmit(&huart2, (uint8_t*)"W\n", 2, 10);
}
#endif
#endif

/* led闪烁 */
void LedTask(void *Parameters)
{
    for (;;)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        printf("Ticks:%d\r\n", xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void prtTask(void *Parameters)
{
    static uint32_t cnt = 0;
    for (;;)
    {
        cnt++;
        printf("CNT: %u\r\n", cnt);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
