#include "uart_link.h"
#include "stm32f4xx.h"

volatile uint32_t g_uart_log_failures;

uint32_t uart_link_us(void)
{
    return TIM_GetCounter(TIM2);
}

bool uart_link_init(void)
{
    RCC_ClocksTypeDef clocks;
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef uart;
    TIM_TimeBaseInitTypeDef timer;
    uint32_t timer_clock;
    RCC_GetClocksFreq(&clocks);
    timer_clock = clocks.PCLK1_Frequency;
    if (clocks.PCLK1_Frequency != clocks.HCLK_Frequency)
        timer_clock *= 2u;
    if (timer_clock < 1000000u || timer_clock % 1000000u != 0u ||
        timer_clock / 1000000u > 65536u)
        return false;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2 | RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = (uint16_t)(timer_clock / 1000000u - 1u);
    timer.TIM_Period = 0xFFFFFFFFu;
    TIM_TimeBaseInit(TIM2, &timer);
    TIM_SetCounter(TIM2, 0u);
    TIM_Cmd(TIM2, ENABLE);
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_9 | GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);
    USART_DeInit(USART1);
    USART_DeInit(USART2);
    USART_StructInit(&uart);
    uart.USART_BaudRate = 115200u;
    uart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &uart);
    USART_Init(USART2, &uart);
    USART_Cmd(USART1, ENABLE);
    USART_Cmd(USART2, ENABLE);
    return true;
}

static bool uart_send(USART_TypeDef *port, const char *text)
{
    uint32_t started;
    if (text == 0)
        return false;
    while (*text != '\0')
    {
        started = uart_link_us();
        while (USART_GetFlagStatus(port, USART_FLAG_TXE) == RESET)
        {
            if ((uint32_t)(uart_link_us() - started) >= 2000u)
                return false;
        }
        USART_SendData(port, (uint8_t)*text++);
    }
    started = uart_link_us();
    while (USART_GetFlagStatus(port, USART_FLAG_TC) == RESET)
    {
        if ((uint32_t)(uart_link_us() - started) >= 2000u)
            return false;
    }
    return true;
}

bool uart_at_send(const char *text)
{
    return uart_send(USART2, text);
}

int uart_at_receive(uint8_t *out)
{
    uint32_t sr = USART2->SR;
    if (out == 0)
        return -1;
    if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0u)
    {
        /* 按SR后DR顺序清接收错误，当前应答作失败，不能继续当完整文本。 */
        (void)USART2->DR;
        return -1;
    }
    if ((sr & USART_SR_RXNE) == 0u)
        return 0;
    *out = (uint8_t)USART_ReceiveData(USART2);
    return 1;
}

void uart_log(const char *text)
{
    if (!uart_send(USART1, text))
        ++g_uart_log_failures;
}
