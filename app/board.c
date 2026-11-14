#include "stm32f4xx.h"
#include "led.h"
#include <stdbool.h>	

//LED1:PB0
//LED2:PB1
//LED3:PE9

/* 定义led的结构体变量 */
struct led_desc led1 = {GPIOB, GPIO_Pin_0, Bit_RESET, Bit_SET};
struct led_desc led2 = {GPIOB, GPIO_Pin_1, Bit_RESET, Bit_SET};
struct led_desc led3 = {GPIOE, GPIO_Pin_9, Bit_RESET, Bit_SET};

/* 板上初始化函数 */		
/* 开启 GPIOB 和 GPIOE 时钟，为 LED 引脚配置做准备。 */
void board_lowlevel_init()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);	
}
