#ifndef HTTP_FLASH_TEST_H
#define HTTP_FLASH_TEST_H

/* 仅主循环调用；Watch请求置1后最多尝试一次，不擦除、不自动重试。 */
void http_flash_test_poll(void);
#endif
