#include "package_stage_test.h"
#include "http_package_test.h"
#include "w25q.h"
#include "console.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* 与实验三Bootloader暂存起点一致；上限避开0xFFFA00的probe数据。 */
#define STAGE_BASE 0xFFF000u
#define STAGE_LIMIT 0xA00u
#define STAGE_POLLS 100000u
volatile uint32_t g_package_stage_request;
/* 0未触发，1新写入并验证，2已有相同包验证通过，3无有效下载包，
 * 4芯片/读取失败，5目标含不同数据，6写失败，7比较失败，8执行中。 */
volatile uint32_t g_package_stage_result;
volatile uint32_t g_package_stage_bytes;
volatile uint32_t g_package_stage_mismatch;
static uint8_t stage_snapshot[STAGE_LIMIT];

void package_stage_test_poll(void)
{
    static bool attempted;
    const uint8_t *data;
    uint32_t length, id, i;
    bool blank = true, same = true;
    char logline[100];
    if (g_package_stage_request != 1u) return;
    g_package_stage_request = 0u;
    if (attempted) return;
    attempted = true;
    g_package_stage_result = 8u;
    if (!http_package_test_view(&data, &length) || length > STAGE_LIMIT)
    {
        g_package_stage_result = 3u;
        goto fail;
    }
    w25q_init();
    if (!w25q_read_jedec_id(1u, &id) || id != 0xEF4018u ||
        !w25q_wait_ready(1u, STAGE_POLLS) ||
        !w25q_read_data(1u, STAGE_BASE, stage_snapshot, length))
    {
        g_package_stage_result = 4u;
        goto fail;
    }
    /* 不能直接擦本扇区：后方还有probe数据，且旧包仍需保留。 */
    for (i = 0u; i < length; ++i)
    {
        if (stage_snapshot[i] != 0xFFu) blank = false;
        if (stage_snapshot[i] != data[i]) same = false;
    }
    if (!same)
    {
        if (!blank)
        {
            g_package_stage_result = 5u;
            goto fail;
        }
        if (!w25q_write_data(1u, STAGE_BASE, data, length, STAGE_POLLS))
        {
            g_package_stage_result = 6u;
            goto fail;
        }
    }
    /* 无论是否新写，都重新读回整个包；与已验CRC的RAM包完全一致才通过。 */
    if (!w25q_read_data(1u, STAGE_BASE, stage_snapshot, length))
    {
        g_package_stage_result = 4u;
        goto fail;
    }
    for (i = 0u; i < length; ++i)
        if (stage_snapshot[i] != data[i]) ++g_package_stage_mismatch;
    if (g_package_stage_mismatch != 0u)
    {
        g_package_stage_result = 7u;
        goto fail;
    }
    g_package_stage_bytes = length;
    g_package_stage_result = same ? 2u : 1u;
    snprintf(logline, sizeof(logline), "[PASS] STAGE: %lu bytes at 0xFFF000, %s, full match\r\n",
             (unsigned long)length, same ? "existing package" : "new package saved");
    console_write(logline);
    return;
fail:
    /* 不写持久状态、不自动重试，失败可能留下部分编程数据。 */
    snprintf(logline, sizeof(logline), "[FAIL] STAGE result=%lu, no erase or retry\r\n",
             (unsigned long)g_package_stage_result);
    console_write(logline);
}
