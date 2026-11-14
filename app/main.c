#include "stm32f4xx.h"
#include "app_init.h"
#include "app_timer.h"
#include "at_test.h"
#include "wifi_test.h"
#include "esp_at.h"

/* 程序入口：初始化 LED，执行 AT 和版本测试，通过后测试 WiFi，最后启动 LED 心跳并循环等待。 */
int main(void)
{
    app_hardware_init();
    at_test_run();

    if (esp_at_get_last_result() == ESP_AT_RESULT_OK)
    {
        wifi_test_run();
    }

    app_timer_start();
    
    while (1)
    {
        __NOP();
    }
}
