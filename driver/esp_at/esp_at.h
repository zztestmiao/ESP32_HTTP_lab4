/** @file esp_at.h
 * ESP-AT命令接口，命名参考天气时钟项目，当前为无RTOS轮询教学实现。
 * 单次初始化后串行调用，不可从中断调用，不连接WiFi、不恢复出厂设置。
 */
#ifndef ESP_AT_H
#define ESP_AT_H
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    ESP_AT_RESULT_NONE = 0,
    ESP_AT_RESULT_OK = 1,
    ESP_AT_RESULT_TIMEOUT = 2,
    ESP_AT_RESULT_OVERFLOW = 3,
    ESP_AT_RESULT_UART_ERROR = 4,
    ESP_AT_RESULT_ERROR = 5,
    ESP_AT_RESULT_BUSY = 6
} esp_at_result_t;

/* 初始化串口/计时，等启动稳定并用AT探测。失败见last_result。
 * 同时准备USART1日志接口；禁止重复调用或与其他USART2业务并发。 */
bool esp_at_init(void);
/* 查询AT+GMR固件版本，完整应答由get_response读取，不改变ESP配置。 */
bool esp_at_get_version(void);
/* 返回最近一次命令结果，不表示当前WiFi已连接。 */
esp_at_result_t esp_at_get_last_result(void);
/* 返回内部静态字符串，下一条命令覆盖；立即使用或复制，不可修改。
 * 失败时可保留部分应答，仅用于诊断，不应当成完整有效响应。 */
const char *esp_at_get_response(void);
#endif
