/* 阅读提示：AT驱动：普通命令按完整行识别应答；HTTP正文按声明长度接收，不能混用字符串解析。 */
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
#include <stdio.h>

#define ESP_AT_RESPONSE_SIZE 2048u
#define ESP_AT_COMMAND_TIMEOUT_US 3000000u
static char rxbuf[ESP_AT_RESPONSE_SIZE];
static esp_at_result_t rxresult;
static bool initialized;

/* 完整行匹配，避免模块回显中的部分字符串被误当终止应答。
 * 普通版本行不匹配，仍保留在rxbuf供上层查看。 */

/* 识别完整响应行中的 OK、ERROR 和 busy；其他行返回 NONE，继续接收。 */
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
 * timeout_us为真实微秒期限，TIM2提供计时。底层发送采用阻塞方式。
 * 每收到一个完整行尝试匹配，溢出/串口错误立即失败，不截断后冒充成功。
 * 返回bool便于业务判断，具体失败原因通过last_result查询。 */

/* 发送 AT 命令并轮询接收，command 必须包含末尾的回车换行；timeout_us 单位为微秒。
 * 发送结束后开始应答计时；每收到完整行就检查结果，只有 OK 返回 true。
 * 超时、模块报错、接收错误或缓冲满均返回 false；rxbuf 和 rxresult 保存本次应答及结果。 */
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

/* 初始化串口和计时器，等待 ESP32 启动 1.5 秒并读走启动信息，再发送 AT 探测。
 * 探测成功才标记为已初始化并返回 true；不会恢复模块出厂设置。 */
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

/* 返回最近一次 AT 操作的结果码，用于查询失败原因；不发送命令。 */
esp_at_result_t esp_at_get_last_result(void)
{
    return rxresult;
}

/* 返回内部应答缓冲区的只读指针；下次命令会覆盖内容，长期保留时需复制。 */
const char *esp_at_get_response(void)
{
    return rxbuf;
}

/* 发送之前先确认基础AT探测完成，避免尚未初始化TIM2就调用等待。 */

/* 检查 AT 探测是否成功；未就绪时设置 UART_ERROR 并返回 false。 */
static bool esp_at_command_ready(void)
{
    if (!initialized)
    {
        rxresult = ESP_AT_RESULT_UART_ERROR;
        return false;
    }
    return true;
}

/* 发送 AT+GMR 查询固件版本；成功返回 true，版本文本从应答缓冲区获取。 */
bool esp_at_get_version(void)
{
    return esp_at_command_ready() &&
           esp_at_write_command("AT+GMR\r\n", ESP_AT_COMMAND_TIMEOUT_US);
}

/* 依次关闭回显、关闭参数持久保存、设置 Station 模式；任一命令失败立即返回 false。 */
bool esp_at_wifi_init(void)
{
    if (!esp_at_command_ready())
        return false;
    /* 每条命令单独等OK；任一失败均不发送含密码的下一条命令。 */
    return esp_at_write_command("ATE0\r\n", 3000000u) &&
           esp_at_write_command("AT+SYSSTORE=0\r\n", 3000000u) &&
           esp_at_write_command("AT+CWMODE=1,0\r\n", 3000000u);
}

/* 返回转义后的字节数，非法或空间不足返回0xFFFFFFFF。
 * 控制字符会破坏AT文本命令边界，因此拒绝，而不是直接拼接。 */

/* 转义 WiFi 参数中的逗号、双引号和反斜杠，拒绝控制字符。
 * capacity 包含结尾零的空间；返回输出字节数，非法字符或空间不足返回 0xFFFFFFFF。 */
static uint32_t esp_at_escape(char *out, uint32_t capacity, const char *text)
{
    uint32_t used = 0u;
    uint8_t c;
    while (*text != '\0')
    {
        c = (uint8_t)*text++;
        if (c < 32u || c == 127u)
            return 0xFFFFFFFFu;
        if (c == ',' || c == '"' || c == '\\')
        {
            if (used + 1u >= capacity)
                return 0xFFFFFFFFu;
            out[used++] = '\\';
        }
        if (used + 1u >= capacity)
            return 0xFFFFFFFFu;
        out[used++] = (char)c;
    }
    out[used] = '\0';
    return used;
}

/* 检查并转义 ssid 和 pwd，发送连接命令，最多等待 45 秒应答；成功返回 true。
 * 完成后清空含密码的临时数组和应答缓冲区，保留结果码供查询。 */
bool esp_at_connect_wifi(const char *ssid, const char *pwd)
{
    char escaped_ssid[65];
    char escaped_pwd[127];
    char command[224];
    bool success;
    if (!esp_at_command_ready())
        return false;
    if (ssid == 0 || pwd == 0 || strlen(ssid) == 0u || strlen(ssid) > 32u ||
        strlen(pwd) > 63u || esp_at_escape(escaped_ssid, sizeof(escaped_ssid), ssid) == 0xFFFFFFFFu ||
        esp_at_escape(escaped_pwd, sizeof(escaped_pwd), pwd) == 0xFFFFFFFFu)
    {
        rxresult = ESP_AT_RESULT_PARAMETER;
        return false;
    }
    snprintf(command, sizeof(command), "AT+CWJAP=\"%s\",\"%s\"\r\n", escaped_ssid, escaped_pwd);
    success = esp_at_write_command(command, 45000000u);
    /* 连接命令不记录日志；隐藏响应缓冲，避免配置或异常回显暴露密码。
     * 最近一次结果枚举保留，失败时仅用结果码排查。 */
    memset(command, 0, sizeof(command));
    memset(escaped_pwd, 0, sizeof(escaped_pwd));
    memset(rxbuf, 0, sizeof(rxbuf));
    return success;
}

/* 查询 WiFi 状态；命令成功且状态为 2（已连接并取得 IP）时返回 true。 */
bool esp_at_is_connected(void)
{
    const char *line;
    unsigned int state;
    if (!esp_at_command_ready() || !esp_at_write_command("AT+CWSTATE?\r\n", 3000000u))
        return false;
    line = strstr(rxbuf, "+CWSTATE:");
    return line != 0 && sscanf(line, "+CWSTATE:%u", &state) == 1 && state == 2u;
}

/* 查询并解析 Station 的 IPv4 地址，写入 ip；capacity 至少为 16 字节。
 * 成功输出以零结尾的点分十进制地址并返回 true；参数、命令或地址无效时返回 false。 */
bool esp_at_get_ip(char *ip, uint32_t capacity)
{
    const char *line;
    unsigned int a, b, c, d;
    if (ip == 0 || capacity < 16u)
    {
        rxresult = ESP_AT_RESULT_PARAMETER;
        return false;
    }
    if (!esp_at_command_ready() || !esp_at_write_command("AT+CIPSTA?\r\n", 3000000u))
        return false;
    line = strstr(rxbuf, "+CIPSTA:ip:\"");
    if (line == 0 || sscanf(line, "+CIPSTA:ip:\"%u.%u.%u.%u", &a, &b, &c, &d) != 4 ||
        a > 255u || b > 255u || c > 255u || d > 255u || (a | b | c | d) == 0u)
        return false;
    snprintf(ip, capacity, "%u.%u.%u.%u", a, b, c, d);
    return true;
}

/* HTTP应答中只在控制文本区找终止行，数据区严格消耗声明字节数。
 * 当前用于已知小文件初测，HTTP状态码由ESP处理，文件正确性由上层CRC确认。
 * 正式下载需明确状态码、长度、重定向及流控，不能直接边推送边阻塞擦Flash。 */
bool esp_at_http_get(const char *url, uint8_t *buffer, uint32_t capacity, uint32_t *length)
{
    /* command发送AT文本；line只收控制帧，不保存二进制正文。 */
    char command[224];
    char line[96];
    const char prefix[] = "+HTTPCGET:";
    /* used是正文总字节数；remaining是当前分段还剩的字节数。 */
    uint32_t used = 0u;
    uint32_t line_length = 0u;
    uint32_t remaining = 0u;
    uint32_t chunk;
    uint32_t i;
    uint32_t started;
    uint8_t byte;
    int received;
    bool seen_data = false;
    if (!esp_at_command_ready())
        return false;
    if (url == 0 || buffer == 0 || length == 0 || capacity == 0u ||
        strlen(url) > 160u || strncmp(url, "http://", 7u) != 0)
    {
        rxresult = ESP_AT_RESULT_PARAMETER;
        return false;
    }
    for (i = 0u; url[i] != '\0'; ++i)
    {
        if ((uint8_t)url[i] <= 32u || url[i] == '"' || url[i] == ',' || url[i] == '\\')
        {
            rxresult = ESP_AT_RESULT_PARAMETER;
            return false;
        }
    }
    *length = 0u;
    rxbuf[0] = '\0';
    /* ESP网络超时10000毫秒，下方STM32总期限为20秒，注意单位区别。 */
    snprintf(command, sizeof(command), "AT+HTTPCGET=\"%s\",2048,512,10000\r\n", url);
    rxresult = ESP_AT_RESULT_NONE;
    if (!uart_at_send(command))
    {
        rxresult = ESP_AT_RESULT_UART_ERROR;
        goto fail;
    }
    started = uart_link_us();
    while ((uint32_t)(uart_link_us() - started) < 20000000u)
    {
        received = uart_at_receive(&byte);
        if (received < 0)
        {
            rxresult = ESP_AT_RESULT_UART_ERROR;
            goto fail;
        }
        if (received == 0)
            continue;
        /* 正文状态：00、OK和换行都只当数据，按数量收完才切回控制状态。 */
        if (remaining != 0u)
        {
            buffer[used++] = byte;
            --remaining;
            continue;
        }
        /* 控制状态：只在这里识别帧头及终止行，并给字符串结尾留空间。 */
        if (line_length + 1u >= sizeof(line))
        {
            rxresult = ESP_AT_RESULT_PROTOCOL;
            goto fail;
        }
        line[line_length++] = (char)byte;
        line[line_length] = '\0';
        /* 识别+HTTPCGET:长度,，逗号后严格接收该长度的原始数据。 */
        if (byte == ',' && line_length > sizeof(prefix) - 1u &&
            strncmp(line, prefix, sizeof(prefix) - 1u) == 0)
        {
            /* 十进制转长度，乘10前检查溢出，防止错误值绕过容量限制。 */
            chunk = 0u;
            for (i = sizeof(prefix) - 1u; i + 1u < line_length; ++i)
            {
                if (line[i] < '0' || line[i] > '9' ||
                    chunk > (0xFFFFFFFFu - (uint32_t)(line[i] - '0')) / 10u)
                {
                    rxresult = ESP_AT_RESULT_PROTOCOL;
                    goto fail;
                }
                chunk = chunk * 10u + (uint32_t)(line[i] - '0');
            }
            if (i == sizeof(prefix) - 1u || chunk > capacity - used)
            {
                rxresult = ESP_AT_RESULT_OVERFLOW;
                goto fail;
            }
            /* 确认本段放得下后进入正文状态，多段共享used连续追加。 */
            remaining = chunk;
            seen_data = true;
            line_length = 0u;
            continue;
        }
        if (byte == '\n')
        {
            if (strcmp(line, "OK\r\n") == 0)
            {
                if (!seen_data || remaining != 0u)
                {
                    rxresult = ESP_AT_RESULT_PROTOCOL;
                    goto fail;
                }
                /* 只有完整应答成功才发布有效长度，失败数据不能用于升级。 */
                *length = used;
                rxresult = ESP_AT_RESULT_OK;
                return true;
            }
            if (strcmp(line, "ERROR\r\n") == 0 || strncmp(line, "busy ", 5u) == 0)
            {
                rxresult = ESP_AT_RESULT_ERROR;
                goto fail;
            }
            line_length = 0u;
        }
    }
    rxresult = ESP_AT_RESULT_TIMEOUT;
fail:
    /* 失败后串口可能还有未消费的二进制，禁止继续按普通文本命令解析。 */
    initialized = false;
    return false;
}
