/* 阅读提示：底层传输：配置USART1/2与TIM2，提供有限等待的发送及非阻塞单字节接收。 */
#include "uart_link.h"
#include "stm32f4xx.h"

/* 读取 TIM2 微秒计数。用无符号减法计算耗时，可处理计数回绕。 */
uint32_t uart_link_us(void)
{
    return TIM_GetCounter(TIM2);
}

/* TIM2每微秒计数一次，供串口与AT应答超时使用。 */

/* 配置 TIM2 每微秒计数一次，供 AT 应答超时使用；时钟无法整分频时返回 false。 */
static bool uart_timer_init(void)
{
    RCC_ClocksTypeDef clocks;
    TIM_TimeBaseInitTypeDef timer;
    uint32_t timer_clock;
    RCC_GetClocksFreq(&clocks);
    timer_clock = clocks.PCLK1_Frequency;
    if (clocks.PCLK1_Frequency != clocks.HCLK_Frequency)
        timer_clock *= 2u;
    if (timer_clock < 1000000u || timer_clock % 1000000u != 0u ||
        timer_clock / 1000000u > 65536u)
        return false;
    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = (uint16_t)(timer_clock / 1000000u - 1u);
    timer.TIM_Period = 0xFFFFFFFFu;
    TIM_TimeBaseInit(TIM2, &timer);
    TIM_SetCounter(TIM2, 0u);
    TIM_Cmd(TIM2, ENABLE);
    return true;
}

/* 配置串口复用引脚：PA9/PA10 用于 USART1 日志，PA2/PA3 用于 USART2 AT 通信。 */
static void uart_io_init(void)
{
    GPIO_InitTypeDef gpio;
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
}

/* USART1日志与USART2 AT统一使用115200、8N1，无硬件流控。 */

/* 初始化两个串口：115200 波特率、8 位数据、无校验、1 位停止位，开启收发。 */
static void uart_usart_init(void)
{
    USART_InitTypeDef uart;
    USART_DeInit(USART1);
    USART_DeInit(USART2);
    USART_StructInit(&uart);
    uart.USART_BaudRate = 115200u;
    uart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &uart);
    USART_Init(USART2, &uart);
    USART_Cmd(USART1, ENABLE);
    USART_Cmd(USART2, ENABLE);
}

/* 开启外设时钟，依次配置计时器、引脚和串口；初始化成功返回 true。 */
bool uart_link_init(void)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2 | RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    if (!uart_timer_init())
        return false;
    uart_io_init();
    uart_usart_init();
    return true;
}

/* 向 port 阻塞发送 text，不发送字符串结尾的零；空指针返回 false。
 * 每字节等待 TXE（发送数据寄存器可写）后写入，硬件异常时可能一直等待。
 * 返回 true 时最后一个字节已写入寄存器，但可能尚未从引脚发送完毕。 */
static bool uart_send(USART_TypeDef *port, const char *text)
{
    if (text == 0)
        return false;
    while (*text != '\0')
    {
        while (USART_GetFlagStatus(port, USART_FLAG_TXE) == RESET)
            ;
        USART_SendData(port, (uint8_t)*text++);
    }
    return true;
}

/* 通过 USART2 发送 AT 文本，调用者自行添加末尾的回车换行；返回底层发送结果。 */
bool uart_at_send(const char *text)
{
    return uart_send(USART2, text);
}

/* 非阻塞接收：返回 1 表示收到字节并写入 out，0 表示暂无数据，-1 表示参数或接收错误。
 * 接收错误会被清除，上层应终止本次命令。 */
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

/* 通过 USART1 阻塞输出日志，应在 AT 应答接收完成后调用，避免耽误接收。 */
void uart_log(const char *text)
{
    (void)uart_send(USART1, text);
}
