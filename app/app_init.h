/* 阅读提示：初始化接口：启动时单次调用，具体板级配置在同名c文件。 */
#ifndef APP_INIT_H
#define APP_INIT_H

/* 上电单次初始化，不擦写Flash。 */
void app_hardware_init(void);
#endif
