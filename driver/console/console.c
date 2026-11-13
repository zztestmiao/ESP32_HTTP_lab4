/* 控制台适配：实验层使用原项目熟悉的接口，低层仍负责发送超时。
 * 只在完整应答接收后输出，避免轮询USART2时被日志阻塞。 */
#include "console.h"
#include "uart_link.h"
void console_write(const char str[])
{
    uart_log(str);
}
