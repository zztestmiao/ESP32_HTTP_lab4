#include "stm32f4xx.h"
#include "app_init.h"
#include "app_timer.h"
#include "at_test.h"

int main(void)
{
    app_hardware_init();
    at_test_run();
    app_timer_start();
    while (1)
    {
        __NOP();
    }
}
