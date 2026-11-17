# 实验四：ESP32 HTTP 下载与 W25Q 暂存验证

本实验是天气时钟 OTA 学习路线的网络下载阶段。使用 STM32F407VET6、ESP32-C3 AT 固件及板载 W25Q128，完成二进制文件下载、RAM 校验、外部 Flash 保存与读回验证。

**阶段验收已完成（2026-11-17）。**真实固件包暂存测试命中“已有相同包”分支，未重新写入真实固件包；512字节测试文件已完成实际编程及读回。完整的“下载→提交 READY→复位→Bootloader 安装→新版运行”在后续实验整合。

## 1. 学习目标与数据路径

- 用 AT 命令控制 ESP32 联网、发起 HTTP 请求。
- 区分 AT 控制响应与二进制正文，按声明长度接收，不能以零字节、换行或 `OK` 判断文件结束。
- 检查长度、CRC32、固件包格式和启动入口。
- 将已验证的数据写入 W25Q，再读回比较；不把驱动返回成功当成数据正确的唯一证据。
- 用手动请求和单次执行锁控制写操作，保护已有数据。

```text
电脑 HTTP 服务（手机热点内）
        ↓ HTTP
ESP32-C3
        ↓ USART2 / AT 通道
STM32 RAM：接收、长度与内容/CRC检查
        ↓ Watch 手动请求
第一颗 W25Q：先读原内容，再决定是否写入，最后读回比较
```

电脑和 ESP32 在本实验中连接同一手机热点。原理上只需网络可达，但本实验不依赖公网服务器或隧道。

## 2. 硬件与工具

| 项目 | 配置 |
| --- | --- |
| 主控 | STM32F407VET6，内部 Flash 512KiB |
| 无线模块 | ESP32-C3，实测 ESP-AT 4.1.0.0（MINI-1） |
| AT 串口 | STM32 USART2 PA2(TX)→ESP IO6(RX)，ESP IO7(TX)→PA3(RX) |
| STM32 日志串口 | USART1 PA9(TX)/PA10(RX)，经板载 USB 串口连接电脑 |
| ESP 自身日志/下载 | UART0 GPIO20/21；与 AT 通道不同 |
| 串口参数 | 115200，8数据位，1停止位，无校验 |
| 外部 Flash | SPI1：PA5 SCK、PA6 MISO、PA7 MOSI；第一颗 CS PE13，第二颗 CS PE14 |
| 开发工具 | Keil µVision，ARM Compiler 5.06 update 7，标准外设库与 CMSIS |
| HTTP 服务 | Python 3，标准库，无额外 Python 依赖 |

STM32 日志端口应出现 `[LAB4]`。若看到 ESP 的 `at-wifi` 重连日志，通常打开的是 ESP 自身日志端口，应换端口确认。

## 3. 工程与模块分工

工程入口：[mdk/stm32f407.uvprojx](mdk/stm32f407.uvprojx)。

| 文件/目录 | 职责 |
| --- | --- |
| `app/main.c` | 组织启动测试及主循环轮询，不实现 Flash 命令 |
| `app/app_init.c` | 板级和 LED 初始化 |
| `app/app_timer.c` | TIM6 中断控制 LED1 心跳 |
| `app/at_test.c` | AT 探测和版本查询 |
| `app/wifi_test.c` | Wi-Fi 配置、连接及 IP 检查 |
| `app/http_test.c` | 下载512字节 probe，检查图样和 CRC |
| `app/http_flash_test.c` | 手动将 probe 保存到 W25Q 并比较 |
| `app/http_package_test.c` | 下载并验证 LED 固件包；通过只读接口发布有效数据 |
| `app/package_stage_test.c` | 手动验证/写入真实包的 W25Q 暂存位置 |
| `app/http_config.h` | 电脑 HTTP 下载地址 |
| `app/wifi_config.example.h` | Wi-Fi 配置模板；实际 `wifi_config.h` 仅在本地填写 |
| `driver/uart_link.c` | 串口及通信超时计时底层 |
| `driver/esp_at/` | AT 命令、响应及 HTTP 二进制接收解析 |
| `driver/console/` | STM32 日志输出 |
| `driver/w25q.c/.h` | SPI Flash 识别、状态、读取、编程和擦除接口 |
| `http_files/` | 允许下载的测试文件 |
| `tools/http_test_server.py` | 仅提供两个白名单文件的 HTTP 服务 |

当前为轮询式教学程序，未接 FreeRTOS。主流程：

```text
硬件初始化 → AT/GMR → Wi-Fi → probe 下载 → LED_V2.ota 下载
                                            ↓
                       启动 TIM6 → 主循环轮询两个手动 Flash 实验
```

各下载阶段受前置条件控制，失败不会无条件继续。

## 4. 存储布局与文件格式

### 内部 Flash

本独立工程链接起点为 `0x08000000`，链接容量 `0x8000`，向量表偏移为0。因此它暂时占用此前实验三 Bootloader 的位置。烧录本工程不是恢复 Bootloader，后续实验五负责地址迁移和启动链整合。

下载必须选择 **Erase Sectors、Program、Verify**，不要整片擦除，以保留 S2/S3 状态及 S4 App。

### 第一颗 W25Q

| 地址范围 | 用途 |
| --- | --- |
| `0xFFF000～0xFFF95B` | 当前2396字节 LED V2 固件包，与实验三暂存起点相同 |
| `0xFFFA00～0xFFFBFF` | 512字节 probe 测试数据 |

两处位于同一个4KiB扇区。本实验的两个手动流程均**不调用擦除**，不能为重测其中一处而直接擦整个扇区。数据不同时停止，后续升级版本需另行处理备份、扇区布局和擦除策略。

### 测试文件

- `probe.bin`：512字节，CRC32 `D8A9F46F`。前8字节为 `00 0D 0A 4F 4B 0D 0A FF`，其余为下标低8位，用于检查零、换行及 `OK` 不会截断二进制接收。
- `LED_V2.ota`：2396字节，包含32字节小端包头及2364字节固件正文，版本2，目标 `0x08010000`，正文 CRC32 `4393486E`。

包头8个32位字段依次为：magic `3141544F`、格式1、版本、目标地址、正文长度、正文 CRC、保留0、前28字节包头 CRC。安装时应跳过包头；CRC用于检错，不提供来源认证。

## 5. 复现实验

### 准备网络和服务

1. 电脑与 ESP32 连接同一个可互访的2.4GHz手机热点。
2. 根据 `app/wifi_config.example.h` 在本地准备 `wifi_config.h`，设置 `WIFI_TEST_ENABLE=1` 并填写凭据。不要提交或分享密码；`.gitignore` 已排除此文件。
3. 查询电脑 WLAN IPv4，修改 `app/http_config.h` 中两个 URL。当前实测地址为 `10.162.203.20`，网络重连后应重新确认，不能填写 ESP 自己的 IP。
4. 从实验四根目录运行：

   ```powershell
   python .\tools\http_test_server.py --port 8000
   ```

   如果8000端口已有本实验服务，不重复启动。服务只提供 `/probe.bin` 和 `/LED_V2.ota`，无目录列表；终端方式启动时需保持运行。
5. 浏览器访问两个文件，确认电脑服务可用。本机可下载尚不能代替 ESP 可达性验证。
6. 如防火墙拦截，使用限定程序、Private 网络、热点接口、TCP8000及实际 ESP 地址的规则，不关闭整个防火墙。此前规则名称为 `STM32_OTA_Lab4_HTTP_ESP`，电脑或 ESP 地址变化后原规则可能不适用。

### 编译与自动下载检查

1. 打开 Keil 工程，Rebuild，再 Download；不能只编译而不下载。
2. 打开 STM32 日志串口，运行或复位。
3. 观察依次通过 AT/GMR、Wi-Fi，随后出现：

   ```text
   [PASS] HTTP: 512 bytes, CRC32=D8A9F46F, binary match
   [PASS] PACKAGE: version=2 image=2364 bytes CRC32=4393486E
   ```

4. 若失败，先记录串口末尾和结果，不继续触发 Flash 实验。

### 手动 probe 保存

在调试暂停时加入以下 Watch 变量，将 `g_http_flash_request` 写1，再 Run 约1秒后暂停：

| 变量 | 成功值 |
| --- | --- |
| `g_http_flash_request` | 0，已消费请求 |
| `g_http_flash_result` | 1：新编程并验证；2：原有相同数据 |
| `g_http_flash_mismatch` | 0 |
| `g_http_flash_readback` | 512字节读回数组 |

该流程要求 HTTP probe 已通过，先识别芯片并读取目标区域：相同则跳过编程，全FF才写，不同且非空则拒绝。

### 手动真实包暂存验证

等到 `PASS PACKAGE` 后，暂停并加入下列 Watch 变量，只将 `g_package_stage_request` 写1，再 Run 后暂停：

| 变量 | 成功值 |
| --- | --- |
| `g_package_stage_request` | 0 |
| `g_package_stage_result` | 1：新编程验证；2：已有相同包验证 |
| `g_package_stage_bytes` | 2396，即 `0x95C` |
| `g_package_stage_mismatch` | 0 |

该流程当前最多接受 `0xA00` 字节包，避免覆盖后方 probe。两个手动请求不要同时触发。各自使用 `static attempted`，每次启动最多尝试一次；失败也不自动重试。失败可能已部分编程，保留现场后判断。

退出 Keil 调试前清除全部断点，这是此前退出卡死的有效规避方法。

## 6. 结果码速查

| 结果变量 | 取值 |
| --- | --- |
| `g_http_result` | 0未执行，1通过，2下载失败，3长度错，4内容或CRC错 |
| `g_package_result` | 0未执行，1通过，2下载失败，3格式/长度错，4CRC错，5入口错 |
| `g_http_flash_result` | 0未触发，1新写通过，2已有相同数据，3HTTP未通过，4识别/读取等失败，5目标非空不同，6写失败，7比较失败 |
| `g_package_stage_result` | 0未触发，1新写通过，2已有相同包，3无有效包或超限，4通信/读取失败，5目标非空不同，6写失败，7比较失败，8执行中 |

AT 层结果另查 `g_http_ack` 或 `g_package_ack`：1成功、2超时、3缓冲溢出、4串口错误、5ESP返回ERROR、6忙、7参数错误、8协议格式错误。`HTTP result=2 AT=5` 不是 CRC 失败，应先查网络和服务；本实验曾在限定防火墙规则放行后恢复。

LED1为程序心跳。LED2/LED3在现有代码主要表示 probe 测试成功/失败，不能仅凭灯光判定真实包暂存或整体 OTA 完成。

## 7. 已验收结果与边界

| 实测项 | 结果与日期 |
| --- | --- |
| AT/GMR及Wi-Fi | 通过，ESP-AT 4.1.0.0，获IPv4 |
| probe下载 | 2026-11-14，512字节、CRC及逐字节比较通过 |
| probe保存W25Q | 2026-11-14，result=1、mismatch=0，实际编程并全段读回 |
| LED固件包下载 | 2026-11-14，版本2、正文2364字节、CRC4393486E，包头及入口检查通过 |
| 真实包暂存衔接 | 2026-11-17，result=2、bytes=0x95C、mismatch=0；旧包与网络包全段一致，未新编程 |

当前最后可构建版本全量编译0错误、5条警告，Code=12862、RO=754、RW=160、ZI=13680；编译不代替硬件验收。

尚未在本实验验收：真实异版本包覆盖、真实包空白区写入分支、网络断点续传、大固件边收边写、网络更新后的断电恢复，以及完整天气时钟OTA。4096字节整包RAM接收只适用于小型LED教学包。后续需要分块与流控、正式容量规划、持久READY提交及Bootloader启动整合，不能直接把本工程用于约300KiB天气时钟固件。

## 8. 继续学习

- 下一阶段：实验五，网络App地址迁移、Bootloader恢复、状态基线迁移及升级闭环。
- 详细复现与历史：[实验四操作手册](../../outputs/实验手册/实验四_ESP32_HTTP下载_操作手册.md)。
- 学习问答：[OTA每日学习笔记](../../outputs/OTA每日学习笔记.md)。
- 整体目标：[天气时钟OTA学习与实验路线](../../outputs/天气时钟OTA学习与实验路线.md)。

本 README 说明实验四的最终阶段成果，不代表实验五已编译或验收通过。
