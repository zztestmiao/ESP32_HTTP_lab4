/* 阅读提示：定时器接口：AT/WiFi/HTTP阶段结束后启动心跳，TIM6不承担串口超时。 */
#ifndef APP_TIMER_H
#define APP_TIMER_H

/* 初始化LED后单次调用，开启TIM6心跳。该模块独占TIM6及其中断。 */
void app_timer_start(void);
#endif
