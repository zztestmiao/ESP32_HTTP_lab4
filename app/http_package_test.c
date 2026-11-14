#include "http_package_test.h"
#include "http_config.h"
#include "esp_at.h"
#include "console.h"
#include <stdint.h>
#include <stdio.h>

/* 这里只接收小型LED包，不能用于约300KiB的正式天气时钟整包接收。 */
uint8_t g_package_data[4096];
/* result：0未执行，1通过，2下载失败，3格式/长度错误，4CRC错误，5入口错误。 */
volatile uint32_t g_package_result;
volatile uint32_t g_package_bytes;
volatile uint32_t g_package_version;
volatile uint32_t g_package_image_size;
volatile uint32_t g_package_expected_crc;
volatile uint32_t g_package_actual_crc;
volatile uint32_t g_package_ack;

/* 按小端逐字节解码，避免未对齐强转和结构体填充问题。 */
static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t package_crc(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu, i, bit;
    for (i = 0u; i < length; ++i)
    {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return crc ^ 0xFFFFFFFFu;
}

void http_package_test_run(void)
{
    uint32_t length, size, sp, entry;
    char logline[112];
    g_package_result = 0u;
    console_write("[PACKAGE] GET " HTTP_PACKAGE_URL "\r\n");
    if (!esp_at_http_get(HTTP_PACKAGE_URL, g_package_data, sizeof(g_package_data), &length))
    {
        g_package_ack = (uint32_t)esp_at_get_last_result();
        g_package_result = 2u;
        goto fail;
    }
    g_package_ack = (uint32_t)esp_at_get_last_result();
    g_package_bytes = length;
    /* 最少32字节包头加8字节向量；先检查长度再解码，避免越界访问。 */
    if (length < 40u || read_u32(g_package_data) != 0x3141544Fu ||
        read_u32(g_package_data + 4) != 1u ||
        read_u32(g_package_data + 12) != 0x08010000u ||
        read_u32(g_package_data + 24) != 0u)
    {
        g_package_result = 3u;
        goto fail;
    }
    size = read_u32(g_package_data + 16);
    /* 当前安装驱动只支持S4；用减法比较，避免长度加法溢出。 */
    if (size != length - 32u || size < 8u || size > 0x10000u)
    {
        g_package_result = 3u;
        goto fail;
    }
    g_package_version = read_u32(g_package_data + 8);
    g_package_image_size = size;
    g_package_expected_crc = read_u32(g_package_data + 20);
    g_package_actual_crc = package_crc(g_package_data + 32, size);
    if (package_crc(g_package_data, 28u) != read_u32(g_package_data + 28) ||
        g_package_actual_crc != g_package_expected_crc)
    {
        g_package_result = 4u;
        goto fail;
    }
    sp = read_u32(g_package_data + 32);
    entry = read_u32(g_package_data + 36);
    /* 初始栈在SRAM且8字节对齐；复位入口须为Thumb并落在固件正文内。 */
    if (sp <= 0x20000000u || sp > 0x20020000u || (sp & 7u) != 0u ||
        (entry & 1u) == 0u || (entry & ~1u) < 0x08010000u ||
        (entry & ~1u) >= 0x08010000u + size)
    {
        g_package_result = 5u;
        goto fail;
    }
    g_package_result = 1u;
    snprintf(logline, sizeof(logline), "[PASS] PACKAGE: version=%lu image=%lu bytes CRC32=%08lX\r\n",
             (unsigned long)g_package_version, (unsigned long)size,
             (unsigned long)g_package_actual_crc);
    console_write(logline);
    return;
fail:
    snprintf(logline, sizeof(logline), "[FAIL] PACKAGE result=%lu AT=%lu\r\n",
             (unsigned long)g_package_result, (unsigned long)g_package_ack);
    console_write(logline);
}

/* 不让暂存模块依赖Watch全局变量，失败时不发布部分下载数据。 */
bool http_package_test_view(const uint8_t **data, uint32_t *length)
{
    if (data == 0 || length == 0 || g_package_result != 1u)
        return false;
    *data = g_package_data;
    *length = g_package_bytes;
    return true;
}
