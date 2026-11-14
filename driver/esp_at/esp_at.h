/* 阅读提示：AT接口：bool给成功与否，last_result给具体原因；response仅是普通命令内部文本缓冲。 */
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
    ESP_AT_RESULT_BUSY = 6,
    ESP_AT_RESULT_PARAMETER = 7,
    ESP_AT_RESULT_PROTOCOL = 8
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

/* 配置Station模式，关闭命令回显并禁止本轮参数保存到ESP NVS。
 * 不恢复出厂；成功后才允许发送含密码的连接命令，避免回显泄露。 */
bool esp_at_wifi_init(void);

/* ssid为1～32字节，密码最多63字节，可空表示开放网络。
 * 内部转义逗号、双引号、反斜杠，拒绝控制字符；不打印此命令或应答。
 * 最多等待45秒，仅在普通主循环/任务中串行调用。 */
bool esp_at_connect_wifi(const char *ssid, const char *pwd);

/* 查询CWSTATE，只有已获IPv4状态才true。 */
bool esp_at_is_connected(void);

/* 查询CIPSTA，输出IPv4字符串，ip至少16字节，失败不覆盖输出。 */
bool esp_at_get_ip(char *ip, uint32_t capacity);

/* HTTP小文件到RAM：容量由调用者给定，length仅成功时为有效长度。
 * 仅支持简短http:// URL，先联网并关闭回显，独占AT串口。
 * 按HTTPCGET每段长度收二进制，支持多段；不写Flash、不验证文件内容。
 * 失败缓冲可能部分更新，并要求重新初始化AT后才能再次发命令。 */
bool esp_at_http_get(const char *url, uint8_t *buffer, uint32_t capacity, uint32_t *length);
#endif
