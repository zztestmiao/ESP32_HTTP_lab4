/* 阅读提示：下载地址配置：IP填电脑热点地址，端口对应HTTP服务，路径对应测试文件。 */
#ifndef HTTP_CONFIG_H
#define HTTP_CONFIG_H

/* 电脑热点IPv4，切换网络后需更新；不能填ESP32自身的地址。 */
#define HTTP_TEST_URL "http://10.162.203.20:8000/probe.bin"
#define HTTP_PACKAGE_URL "http://10.162.203.20:8000/LED_V2.ota"
#endif
