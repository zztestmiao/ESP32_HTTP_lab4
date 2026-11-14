/* 阅读提示：下载测试接口：仅在WiFi通过后单次调用，失败不自动重试。 */
#ifndef HTTP_TEST_H
#define HTTP_TEST_H

/* WiFi成功后单次下载512字节测试文件到RAM，不写Flash。 */
void http_test_run(void);
#endif
