#include "wifi_test.h"
#include "wifi_config.h"
#include "esp_at.h"
#include "console.h"
#include "board.h"
#include "led.h"
#include <stdint.h>

/* result：0未启用/1通过/2模式配置失败/3连接失败/4未获IP状态/5IP查询失败。
 * step：1配置/2连接/3状态检查/4查IP/5完成；last_ack保留失败时AT结果。 */
volatile uint32_t g_wifi_result;
volatile uint32_t g_wifi_step;
volatile uint32_t g_wifi_last_ack;
char g_wifi_ip[16];

/* 依次配置 WiFi、连接、检查状态和查询 IP，记录失败步骤并输出提示；未启用测试时直接跳过。 */
void wifi_test_run(void)
{
#if WIFI_TEST_ENABLE
    led_off(&led2);
    g_wifi_step = 1u;
    console_write("[WiFi] Configure station, credentials hidden\r\n");
    if (!esp_at_wifi_init())
    {
        g_wifi_result = 2u;
        goto fail;
    }
    g_wifi_step = 2u;
    console_write("[WiFi] Connecting (up to 45s)...\r\n");
    if (!esp_at_connect_wifi(WIFI_SSID, WIFI_PASSWORD))
    {
        g_wifi_result = 3u;
        goto fail;
    }
    g_wifi_step = 3u;
    if (!esp_at_is_connected())
    {
        g_wifi_result = 4u;
        goto fail;
    }
    g_wifi_step = 4u;
    if (!esp_at_get_ip(g_wifi_ip, sizeof(g_wifi_ip)))
    {
        g_wifi_result = 5u;
        goto fail;
    }
    g_wifi_result = 1u;
    g_wifi_step = 5u;
    console_write("[PASS] WiFi IP: ");
    console_write(g_wifi_ip);
    console_write("\r\n");
    led_on(&led2);
    return;
fail:
    g_wifi_last_ack = (uint32_t)esp_at_get_last_result();
    console_write("[FAIL] WiFi: inspect g_wifi_result / step / last_ack\r\n");
    led_on(&led3);
#else
    console_write("[SKIP] WiFi: configure app/wifi_config.h locally\r\n");
#endif
}
