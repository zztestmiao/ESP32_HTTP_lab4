#include "http_flash_test.h"
#include "w25q.h"
#include "console.h"
#include <stdint.h>
#include <stdbool.h>

/* 只使用实验三2396字节包后方的512字节，不擦整个扇区以保护固件包。 */
#define TEST_ADDRESS 0xFFFA00u
#define TEST_SIZE 512u
#define TEST_POLLS 100000u

/* HTTP测试模块拥有下载缓冲，本模块只读取；后续正式下载改为独立接口。 */
extern volatile uint32_t g_http_result;
extern uint8_t g_http_data[TEST_SIZE];
volatile uint32_t g_http_flash_request;
/* 0未触发，1写入并验证成功，2已有相同数据，3HTTP未通过，
 * 4芯片或读取失败，5目标被其他内容占用，6写入失败，7读回不符。 */
volatile uint32_t g_http_flash_result;
volatile uint32_t g_http_flash_mismatch;
uint8_t g_http_flash_readback[TEST_SIZE];

void http_flash_test_poll(void)
{
    static bool attempted;
    uint32_t id, i;
    bool blank = true;
    bool same = true;
    if (g_http_flash_request != 1u)
        return;
    g_http_flash_request = 0u;
    if (attempted)
        return;
    attempted = true;
    if (g_http_result != 1u)
    {
        g_http_flash_result = 3u;
        goto fail;
    }
    w25q_init();
    if (!w25q_read_jedec_id(1u, &id) || id != 0xEF4018u ||
        !w25q_wait_ready(1u, TEST_POLLS) ||
        !w25q_read_data(1u, TEST_ADDRESS, g_http_flash_readback, TEST_SIZE))
    {
        g_http_flash_result = 4u;
        goto fail;
    }
    /* 先读后判定，非空且不同的数据绝不覆盖；完全一致时无需重复编程。 */
    for (i = 0u; i < TEST_SIZE; ++i)
    {
        if (g_http_flash_readback[i] != 0xFFu) blank = false;
        if (g_http_flash_readback[i] != g_http_data[i]) same = false;
    }
    if (same)
    {
        g_http_flash_result = 2u;
        console_write("[PASS] W25Q: existing 512 bytes match HTTP RAM\r\n");
        return;
    }
    if (!blank)
    {
        g_http_flash_result = 5u;
        goto fail;
    }
    /* HTTP接收已经结束，写Flash期间没有网络串口数据持续到来。 */
    if (!w25q_write_data(1u, TEST_ADDRESS, g_http_data, TEST_SIZE, TEST_POLLS))
    {
        g_http_flash_result = 6u;
        goto fail;
    }
    if (!w25q_read_data(1u, TEST_ADDRESS, g_http_flash_readback, TEST_SIZE))
    {
        g_http_flash_result = 4u;
        goto fail;
    }
    for (i = 0u; i < TEST_SIZE; ++i)
        if (g_http_flash_readback[i] != g_http_data[i]) ++g_http_flash_mismatch;
    if (g_http_flash_mismatch != 0u)
    {
        g_http_flash_result = 7u;
        goto fail;
    }
    g_http_flash_result = 1u;
    console_write("[PASS] W25Q: HTTP 512 bytes saved at 0xFFFA00, readback match\r\n");
    return;
fail:
    /* 失败可能已部分写入，保留现场；不擦除也不盲目重试。 */
    console_write("[FAIL] W25Q: inspect g_http_flash_result, no automatic retry\r\n");
}
