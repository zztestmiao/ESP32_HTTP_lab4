/* 阅读提示：联网测试接口：主流程用passed查询本轮结果，不直接访问测试模块的全局变量。 */
#ifndef WIFI_TEST_H
#define WIFI_TEST_H
#include <stdbool.h>

/* AT探测通过后单次调用，按本地配置连接WiFi并查询IP，不访问Flash。 */
void wifi_test_run(void);

/* 仅报告本轮联网实测结果，主流程据此决定是否进入HTTP。 */
bool wifi_test_passed(void);
#endif
