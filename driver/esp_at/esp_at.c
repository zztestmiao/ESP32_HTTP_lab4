/** @file esp_at.c
 * 分层：at_test组织实验 → 本模块组织AT命令/完整行应答 → uart_link单字节收发。
 * USART2 PA2 TX→ESP IO6 RX，ESP IO7 TX→PA3 RX，115200 8N1。
 * 当前无需FreeRTOS，后续接正式项目时替换传输和等待后端，保持接口职责。
 * 共用一份接收缓冲区，调用者必须串行访问。文本匹配只用于AT应答，
 * 未来HTTP固件正文必须按长度接收，不能把二进制中的OK当结束。
 */
#include "esp_at.h"
#include "uart_link.h"
#include <string.h>

#define ESP_AT_RESPONSE_SIZE 2048u
#define ESP_AT_COMMAND_TIMEOUT_US 3000000u

static char rxbuf[ESP_AT_RESPONSE_SIZE];
static esp_at_result_t rxresult;
static bool initialized;

/* 完整行匹配，避免模块回显中的部分字符串被误当终止应答。
 * 普通版本行不匹配，仍保留在rxbuf供上层查看。 */
static esp_at_result_t esp_at_match_ack(const char *line)
{
    if (strcmp(line, "OK\r\n") == 0)
        return ESP_AT_RESULT_OK;
    if (strcmp(line, "ERROR\r\n") == 0)
        return ESP_AT_RESULT_ERROR;
    if (strncmp(line, "busy ", 5u) == 0)
        return ESP_AT_RESULT_BUSY;
    return ESP_AT_RESULT_NONE;
}

/* 先清接收状态再发送命令，命令必须包含CRLF；不发送字符串结尾零。
 * timeout_us为真实微秒期限，TIM2提供计时。发送也有底层有限等待。
 * 每收到一个完整行尝试匹配，溢出/串口错误立即失败，不截断后冒充成功。
 * 返回bool便于业务判断，具体失败原因通过last_result查询。 */
static bool esp_at_write_command(const char *command, uint32_t timeout_us)
{
    uint32_t started;
    uint32_t rxlen = 0u;
    uint32_t line_start = 0u;
    uint8_t byte;
    int received;
    esp_at_result_t ack;
    rxbuf[0] = '\0';
    rxresult = ESP_AT_RESULT_NONE;
    if (command == 0 || timeout_us == 0u || !uart_at_send(command))
    {
        rxresult = ESP_AT_RESULT_UART_ERROR;
        return false;
    }
    started = uart_link_us();
    while ((uint32_t)(uart_link_us() - started) < timeout_us)
    {
        received = uart_at_receive(&byte);
        if (received < 0)
        {
            rxresult = ESP_AT_RESULT_UART_ERROR;
            return false;
        }
        if (received == 0)
            continue;
        if (rxlen + 1u >= sizeof(rxbuf))
        {
            rxresult = ESP_AT_RESULT_OVERFLOW;
            return false;
        }
        rxbuf[rxlen++] = (char)byte;
        rxbuf[rxlen] = '\0';
        if (byte == '\n')
        {
            ack = esp_at_match_ack(&rxbuf[line_start]);
            if (ack != ESP_AT_RESULT_NONE)
            {
                rxresult = ack;
                return ack == ESP_AT_RESULT_OK;
            }
            line_start = rxlen;
        }
    }
    rxresult = ESP_AT_RESULT_TIMEOUT;
    return false;
}

bool esp_at_init(void)
{
    uint32_t started;
    uint8_t discarded;
    initialized = false;
    rxbuf[0] = '\0';
    if (!uart_link_init())
    {
        rxresult = ESP_AT_RESULT_UART_ERROR;
        return false;
    }
    /* 给模块启动留1.5秒，并持续读走启动信息，避免旧数据和接收溢出。
     * 不发送AT+RESTORE，不清WiFi参数，不改AT波特率或ESP固件。 */
    started = uart_link_us();
    while ((uint32_t)(uart_link_us() - started) < 1500000u)
        (void)uart_at_receive(&discarded);
    initialized = esp_at_write_command("AT\r\n", ESP_AT_COMMAND_TIMEOUT_US);
    return initialized;
}

bool esp_at_get_version(void)
{
    if (!initialized)
    {
        rxresult = ESP_AT_RESULT_UART_ERROR;
        return false;
    }
    return esp_at_write_command("AT+GMR\r\n", ESP_AT_COMMAND_TIMEOUT_US);
}

esp_at_result_t esp_at_get_last_result(void)
{
    return rxresult;
}

const char *esp_at_get_response(void)
{
    return rxbuf;
}
