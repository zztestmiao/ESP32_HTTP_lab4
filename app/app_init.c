#include "app_init.h"
#include "board.h"
#include "led.h"
/* 独立网络实验，先准备LED，不访问外部Flash。UART由通信模块初始化。 */
void app_hardware_init(void)
{
    board_lowlevel_init();
    led_init(&led1);
    led_init(&led2);
    led_init(&led3);
    led_off(&led1);
    led_off(&led2);
    led_off(&led3);
}
