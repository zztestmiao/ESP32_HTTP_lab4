/* Log after receiving the complete AT response to avoid RX overruns. */
#include "console.h"
#include "uart_link.h"
void console_write(const char str[])
{
    uart_log(str);
}
