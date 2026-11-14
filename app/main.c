/* 阅读提示：主流程：初始化→AT探测→联网→HTTP下载检查→LED心跳；失败的阶段不继续下一步。 */
#include "stm32f4xx.h"
#include "app_init.h"
#include "app_timer.h"
#include "at_test.h"
#include "wifi_test.h"
#include "esp_at.h"
#include "http_test.h"
#include "http_flash_test.h"

/* 程序入口：初始化 LED，执行 AT 和版本测试，通过后测试 WiFi，最后启动 LED 心跳并循环等待。 */
int main(void)
{
    app_hardware_init();
    at_test_run();
    if (esp_at_get_last_result() == ESP_AT_RESULT_OK)
    {
        wifi_test_run();
    }
    /* 上条AT成功不等于已联网，只有本轮IP检查通过才进入HTTP。 */
    if (wifi_test_passed())
    {
        http_test_run();
    }
    /* 失败也会启动LED1心跳；具体结果看日志及LED2/LED3。 */
    app_timer_start();
    while (1)
    {
        http_flash_test_poll();
    }
}
