/* 阅读提示：传输接口：TIM2计时单位为微秒，接收返回1有字节、0暂无字节、负数为错误。 */
#ifndef UART_LINK_H
#define UART_LINK_H
#include <stdint.h>
#include <stdbool.h>

/* 独占USART1日志、USART2 AT、TIM2微秒计时。115200 8N1，无中断。
 * TIM2仅供本模块超时使用，不复用TIM6。初始化失败不得继续发送。 */
bool uart_link_init(void);
uint32_t uart_link_us(void);
bool uart_at_send(const char *text);

/* 返回1收到字节、0无数据、-1硬件接收错误；out必须有效。 */
int uart_at_receive(uint8_t *out);

/* USART1 阻塞日志输出；返回时最后一个字节已写入发送寄存器。 */
void uart_log(const char *text);
#endif
