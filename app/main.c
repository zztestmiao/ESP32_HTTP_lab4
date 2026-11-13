#include "stm32f4xx.h"
#include "app_init.h"
#include "app_timer.h"
#include "at_test.h"
#include "wifi_test.h"
#include "esp_at.h"

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
