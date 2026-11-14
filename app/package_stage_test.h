#ifndef PACKAGE_STAGE_TEST_H
#define PACKAGE_STAGE_TEST_H
/* 主循环手动请求接口；只操作外部暂存区，不写READY和内部App。 */
void package_stage_test_poll(void);
#endif
