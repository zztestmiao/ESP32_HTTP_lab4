/** @file at_test.c
 * 实验层只描述“探测AT→查询版本→记录结果”，不拼接串口或解析应答。
 * 驱动接口风格参考天气时钟，但不引入FreeRTOS依赖或恢复出厂行为。
 */
#include "at_test.h"
#include "esp_at.h"
#include "console.h"
#include "board.h"
#include "led.h"
#include <stdint.h>
#include <string.h>

/* 0未执行/1成功/2超时/3缓冲满/4串口错误/5ERROR/6模块忙。
 * Watch名称保持不变，已有观察步骤仍适用。 */
volatile uint32_t g_at_link_ready;
volatile uint32_t g_at_result;
volatile uint32_t g_gmr_result;
char g_at_response[256];
char g_gmr_response[2048];

/* 测试 AT 通信及版本查询，保存结果和应答供 Watch 查看；LED2 表示通过，LED3 表示失败。 */
void at_test_run(void)
{
    bool success;
    success = esp_at_init();
    g_at_result = (uint32_t)esp_at_get_last_result();
    /* 立刻保存本条应答，下一条命令会覆盖驱动的内部缓冲区。
     * AT诊断快照最多255字节，驱动完整缓冲仍有2048字节。 */
    strncpy(g_at_response, esp_at_get_response(), sizeof(g_at_response) - 1u);
    g_at_response[sizeof(g_at_response) - 1u] = '\0';
    if (!success)
    {
        /* 初始化失败时不依赖日志输出，避免计时器未就绪时误调用发送。 */
        led_on(&led3);
        return;
    }
    g_at_link_ready = 1u;
    console_write("\r\n[LAB4] AT probe OK, USART1 log 115200 8N1\r\n");
    console_write(g_at_response);
    console_write("[TX] AT+GMR\r\n");
    success = esp_at_get_version();
    g_gmr_result = (uint32_t)esp_at_get_last_result();
    strncpy(g_gmr_response, esp_at_get_response(), sizeof(g_gmr_response) - 1u);
    g_gmr_response[sizeof(g_gmr_response) - 1u] = '\0';
    console_write(g_gmr_response);
    if (success)
    {
        console_write("\r\n[PASS] AT and GMR\r\n");
        led_on(&led2);
    }
    else
    {
        console_write("\r\n[FAIL] GMR: inspect Watch result\r\n");
        led_on(&led3);
    }
}
