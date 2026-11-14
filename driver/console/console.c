#include "console.h"
#include "uart_link.h"
/* 将以零结尾的 str 原样交给 USART1 输出，不自动添加换行。 */
void console_write(const char str[])
{
    uart_log(str);
}
