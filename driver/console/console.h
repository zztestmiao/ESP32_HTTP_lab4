/* 阅读提示：日志接口：输入以零结尾的文本，不可用它打印固件二进制缓冲区。 */
#ifndef CONSOLE_H
#define CONSOLE_H

/* 命名沿用天气时钟console_write；调用esp_at_init尝试初始化后才可使用。
 * 仅向USART1输出以零结尾的文本，不向ESP发送、不用于二进制固件。
 * 不依赖printf重定向，不可并发/中断调用，失败统计在g_uart_log_failures。
 * 当前串口时钟由共用uart_link初始化，后续迁移时可换原console后端。 */
void console_write(const char str[]);
#endif
