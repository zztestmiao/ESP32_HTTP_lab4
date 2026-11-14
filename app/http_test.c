/* 阅读提示：下载测试：仅把512字节收进RAM，再检查长度、内容和CRC；不会写W25Q。 */
#include "http_test.h"
#include "http_config.h"
#include "esp_at.h"
#include "console.h"
#include "board.h"
#include "led.h"
#include <stdint.h>
#include <stdio.h>

/* result：0未执行/1通过/2下载失败/3长度不符/4CRC或逐字节比较失败。
 * buffer是二进制，不作为字符串打印；失败后不可将其作为完整固件。
 * ack是AT层结果，8表示控制帧格式问题。 */
volatile uint32_t g_http_result;
volatile uint32_t g_http_bytes;
volatile uint32_t g_http_crc;
volatile uint32_t g_http_mismatch;
volatile uint32_t g_http_ack;
uint8_t g_http_data[512];

/* 与电脑zlib一致的CRC32，用于检验传输内容；不提供来源认证。 */
static uint32_t http_test_crc(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i, bit;
    /* 每个字节参与8轮位运算，CRC与测试图样规则无关。 */
    for (i = 0u; i < length; ++i)
    {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return crc ^ 0xFFFFFFFFu;
}

void http_test_run(void)
{
    /* 文件开头故意包含零、换行、OK和FF，检验接收不会被特殊内容截断。 */
    const uint8_t first[8] = {0x00, 0x0D, 0x0A, 0x4F, 0x4B, 0x0D, 0x0A, 0xFF};
    char logline[80];
    uint32_t length;
    uint32_t i;
    uint8_t expected;
    led_off(&led2);
    console_write("[HTTP] GET ");
    console_write(HTTP_TEST_URL);
    console_write("\r\n");
    /* 先完整接收；失败时buffer可能只有部分数据，不进入比较。 */
    if (!esp_at_http_get(HTTP_TEST_URL, g_http_data, sizeof(g_http_data), &length))
    {
        g_http_ack = (uint32_t)esp_at_get_last_result();
        g_http_result = 2u;
        goto fail;
    }
    g_http_ack = (uint32_t)esp_at_get_last_result();
    g_http_bytes = length;
    /* AT成功不等于文件正确，先严格匹配已知测试文件长度。 */
    if (length != sizeof(g_http_data))
    {
        g_http_result = 3u;
        goto fail;
    }
    for (i = 0u; i < length; ++i)
    {
        expected = (i < sizeof(first)) ? first[i] : (uint8_t)i;
        if (g_http_data[i] != expected)
            ++g_http_mismatch;
    }
    /* 长度正确后再算CRC；所有检查通过才点亮成功灯。 */
    g_http_crc = http_test_crc(g_http_data, length);
    if (g_http_crc != 0xD8A9F46Fu || g_http_mismatch != 0u)
    {
        g_http_result = 4u;
        goto fail;
    }
    g_http_result = 1u;
    console_write("[PASS] HTTP: 512 bytes, CRC32=D8A9F46F, binary match\r\n");
    led_on(&led2);
    return;

/* 只报告阶段结果及AT结果，不输出二进制内容，不自动重试。 */
fail:
    snprintf(logline, sizeof(logline), "[FAIL] HTTP result=%lu AT=%lu\r\n", (unsigned long)g_http_result, (unsigned long)g_http_ack);
    console_write(logline);
    led_on(&led3);
}
