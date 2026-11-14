#ifndef HTTP_PACKAGE_TEST_H
#define HTTP_PACKAGE_TEST_H
#include <stdbool.h>
#include <stdint.h>
/* 联网成功后单次下载LED教学固件包，仅验证RAM，不写Flash或升级状态。 */
void http_package_test_run(void);
/* 仅验证成功才给出只读视图；缓冲由本模块持有，下一次下载前有效。 */
bool http_package_test_view(const uint8_t **data, uint32_t *length);
#endif
