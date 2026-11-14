/* 阅读提示：基础测试接口：启动时执行一次，结果通过日志、LED及Watch观察。 */
#ifndef AT_TEST_H
#define AT_TEST_H

/* 上电单次AT和AT+GMR探测，不连WiFi、不修改ESP固件、不访问Flash。
 * UART轮询仅适合短文本初测，后续HTTP二进制必须独立按长度接收。 */
void at_test_run(void);
#endif
