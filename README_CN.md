# OPC UA 服务器 for Kincony A16V3 - 从物联网控制器到工业网关

本项目将经济实惠的 **Kincony A16V3 物联网控制器** 转变为功能完整的 **工业级 OPC UA 网关**，适用于工业 4.0 生态系统。

## 📋 项目概述

**目标：** 提供一个完整的、生产就绪的解决方案，用于将低成本硬件控制器集成到工业自动化系统中，使用标准化的 OPC UA 协议。

**核心理念：** "从物联网控制器到工业网关"

*   **硬件平台：** Kincony A16V3 (基于 ESP32 的工业控制器)
*   **协议：** OPC UA (开放平台通信统一架构)
*   **目的：** 创建一个 OPC UA 服务器，将控制器的输入/输出和状态暴露为标准化的 OPC UA 信息模型。

## 🎯 当前状态与目标

*   **核心功能：** 已实现并可运行
*   **当前任务：** 在不改变程序逻辑的前提下，完成**源代码文档的英文化与增强**（技术注释）
*   **最终目标：** 准备项目用于干净的发布，提供专业的、全英文的代码文档

## 📁 项目结构与处理计划

项目由几个关键模块组成，按优先级顺序处理：

1.  **PCF8574 驱动** (优先级 1)
    *   `components/esp32-pcf8574/pcf8574.c`
    *   `components/esp32-pcf8574/include/pcf8574.h`
    *   *状态：* 已处理。所有注释已翻译成英文，并添加了 Doxygen 风格的文档。

2.  **OPC UA 数据模型** (优先级 2)
    *   `components/model/model.c`
    *   `components/model/include/model.h`

3.  **I/O 缓存系统** (优先级 3)
    *   `components/io_cache/io_cache.c`
    *   `components/io_cache/include/io_cache.h`
    *   `components/io_cache/io_polling.c`

4.  **网络模块** (优先级 4)
    *   `components/ethernet/ethernet_connect.c`
    *   `components/ethernet/include/ethernet_connect.h`

## ✨ 代码文档原则

在增强文档时，遵循严格的规则：
*   ✅ **不修改逻辑：** 算法、变量和结构名称保持不变
*   ✅ **翻译注释：** 所有非英语注释 (`//` 和 `/* ... */`) 均翻译为英文
*   ✅ **添加 Doxygen 文档：** 为每个函数、结构和关键代码块添加详细的 `/** ... */` 注释，解释其目的、参数和返回值
*   ✅ **风格一致：** 遵循在 `main/opcua_esp32.c` 中建立的文档风格
*   ✅ **许可证归属：** 每个文件头包含版权和许可信息

## 📄 许可证

项目包含受不同许可证约束的代码，如文件头所述：
*   **MPL-2.0 许可 (修改版)：** `components/model/`
*   **Apache 2.0 许可 (来自 ESP-IDF)：** `components/ethernet/`
*   **MIT 许可 (原创)：** `components/esp32-pcf8574/`
*   **项目原创文件：** `components/io_cache/`, `main/`

有关详细信息，请参阅仓库根目录中的 `LICENSE` 文件和各个源文件头部。

## 🔧 编译、构建与刷写

### 1. ESP-IDF 环境设置

在构建之前，请激活 ESP-IDF 环境：

```bash
# 导航到 ESP-IDF 安装目录
cd ~/esp/esp-idf
# 激活 ESP-IDF 环境
. ./export.sh

# 返回项目目录
cd ~/exchange/K868/git/opcua-kincony-a16v3
```

### 2. 项目编译与构建

#### 基本构建：
```bash
# 清理之前的构建 (可选)
idf.py fullclean

# 配置项目 (如果未配置)
idf.py menuconfig

# 构建项目
idf.py build
```

### 3. 刷写到 Kincony A16V3 设备

#### 确定 COM 端口：
```bash
# Linux/Mac
ls /dev/ttyUSB* || ls /dev/ttyACM*

# Windows
# 在设备管理器中识别 COM 端口 (例如, COM3)
```

#### 刷写方法：

**选项 1：自动刷写 (推荐)**
```bash
# 设置设备端口
export ESPPORT=/dev/ttyUSB0  # Linux/Mac
# 或 set ESPPORT=COM3       # Windows

# 使用单命令刷写
idf.py flash
```

**选项 2：通过 esptool.py 手动刷写**
```bash
# 分别写入所有组件
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/opcua_esp32.bin
```

### 4. 设备监控

用于调试和监控设备运行：

```bash
# 监控串口
idf.py monitor

# 自动重连监控
idf.py monitor -p /dev/ttyUSB0 -b 115200

# 要退出监控器，请按 Ctrl+]
```

## 🧪 测试工具编译 (test_counter8)

`test_counter8` 实用程序用于 OPC UA 服务器性能测试。

### 构建测试工具：

```bash
# 导航到测试工具目录
cd ~/exchange/K868/git/TEST_OPC_X86/test_counter

# 使用 GCC 编译 (Linux/Mac)
gcc -o test_counter8 test_counter8.c \
  -I/usr/local/include/open62541 \
  -L/usr/local/lib \
  -lopen62541 \
  -lpthread \
  -lm

# 使用特定标志的替代编译
gcc -Wall -Wextra -O2 -std=c99 -o test_counter8 test_counter8.c \
  -lopen62541 -lpthread -lm
```

### 运行性能测试：

```bash
# 基本测试 (1000 毫秒间隔)
./test_counter8 -t 1000 opc.tcp://10.0.0.128:4840

# 不同间隔的测试
./test_counter8 -t 500 opc.tcp://10.0.0.128:4840  # 500 毫秒
./test_counter8 -t 100 opc.tcp://10.0.0.128:4840  # 100 毫秒

# 显示帮助
./test_counter8 --help
```

## 📊 性能测试结果分析

### 测试参数：
- **间隔：** 1000 毫秒
- **服务器：** `opc.tcp://10.0.0.128:4840`
- **标签：** 9 个标签 (5 个系统标签 + 4 个 ADC 通道)
- **持续时间：** 2789.890 毫秒 (≈2.8 秒)
- **周期数：** 53

### 关键性能指标：

| 指标 | 值 | 评估 |
|--------|-------|------------|
| 平均标签读取时间 | 4.739 毫秒 | ✅ 优秀 |
| 最小读取时间 | 3.768 毫秒 | ✅ 优秀 |
| 最大读取时间 | 13.314 毫秒 | ✅ 在可接受范围内 |
| 总可靠性 | 0 错误 (100%) | ✅ 完美 |
| 平均周期时间 | 52.612 毫秒 | ✅ 比计划 (1000 毫秒) 更快 |

### 理论吞吐量：
- **所有 9 个标签的最大轮询频率：** 19.0 Hz
- **单个标签最大读取频率：** 211.0 Hz
- **ADC 通道最大读取频率：** 211.5 Hz

### 性能评估：

```
┌─────────────────────────────────────────────┐
│         性能测试结果            │
├─────────────────────────────────────────────┤
│  性能:       ⭐⭐⭐⭐⭐ (5/5)      │
│  可靠性:     ⭐⭐⭐⭐⭐ (5/5)      │
│  稳定性:     ⭐⭐⭐⭐⭐ (5/5)      │
│  符合要求:   ⭐⭐⭐⭐⭐ (5/5)      │
└─────────────────────────────────────────────┘
```

## 📋 日志示例

### 服务器日志示例 (ESP32)：
```
I (102) cpu_start: Starting scheduler on PRO CPU.
I (123) gpio: GPIO[21]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (133) pcf8574: I2C initialized on port 0, SDA=21, SCL=22, speed=100000
I (153) eth_connect: Ethernet Link Up
I (193) eth_connect: Got IP Address: 10.0.0.128
I (203) opcua_esp32: Network initialized, IP: 10.0.0.128
I (333) open62541lib: Server running on opc.tcp://10.0.0.128:4840/
```

### 测试工具日志示例：
```
=============================================
   OPC UA 高速性能测试
   包含 ADC 通道的完整系统测试
   按任意键停止
=============================================

正在连接到 opc.tcp://10.0.0.128:4840...
[2025-12-12 03:06:39.171] info/channel TCP 5 | SC 2 | SecureChannel opened
已连接！

开始测试...
在 discrete_outputs 上生成方波
将字计数器写入 loopback_input
读取所有 9 个标签 (5 个系统标签 + 4 个 ADC 通道)

周期 | 字计数 | 状态 | 时间 (毫秒)
-----------------------------------
    0 |       1 |   低 |    50.226
   10 |      11 |   低 |    50.105
   20 |      21 |   低 |    52.366

=== 性能总结 ===
总测试时间：        2789.890 毫秒
总周期数：          53
总标签读取次数：     477
总错误数：          0
平均每个标签读取时间： 4.739 毫秒
```

## 🤝 贡献指南

欢迎贡献。请确保所有新注释和文档均使用英文撰写，并且编码风格与现有代码库匹配。

## 📞 联系信息

**项目维护者：** Alex D
**全球联系方式：** aksurd@gmail.com
**中国地区：** wxid_ic7ytyv3mlh522
**GitHub：** [Aksurd](https://github.com/Aksurd)
**代码仓库：** `https://github.com/Aksurd/opcua-kincony-a16v3`

## 📚 附加资源

- [ESP-IDF 文档](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Open62541 文档](https://open62541.org/doc/current/)
- [OPC UA 规范](https://opcfoundation.org/developer-tools/specifications-unified-architecture)
- [Kincony A16V3 文档](https://www.kincony.com/esp32-16-channel-relay-controller.html)

---

*本项目展示了在成本效益高的硬件上实现工业级 OPC UA 服务器，弥合了物联网设备与工业 4.0 系统之间的差距。* 
