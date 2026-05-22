# IN-12 辉光管时钟

基于 ESP32-C3 驱动的 IN-12 辉光管时钟，四位数显（HH:MM），配有 iOS 风格 Web 控制面板（配网、时间校准、管子设置全在浏览器里完成）。

## 项目结构

```
IN12/
├── main/               # 入口 app_main.cpp，按顺序拉取各模块
├── components/         # 自定义组件
│   ├── in12_driver/    # 辉光管底层驱动（74HC595 移位寄存器 + BCD 译码器）
│   ├── controller/     # 中央控制器 — 显示优先级、防阴极中毒、设置持久化
│   ├── timezone_module/# 时间管理器 — NTP 同步 + 秒/分精度回调分发
│   ├── wifi_module/    # WiFi 模块 — AP 热点配网 + STA 连接 + 扫描列表
│   ├── web_server/     # Web 服务 — REST API（9 个路由）+ LittleFS 静态文件
│   ├── nvs_module/     # NVS 存储 — 持久化 WiFi 凭据与灯管参数
│   ├── littlefs_module/# LittleFS 文件系统 — 挂载 web 前端文件
│   └── project_config/ # 项目全局常量（引脚、WiFi 名称、分区标签等）
├── littlefs_root/      # Web 前端源文件
│   ├── index.html      # 三页签界面（通用 / 辉光管 / 关于）
│   ├── app.js          # 前端逻辑 — API 调用、实时时间、WiFi 配网
│   ├── style.css       # iOS 暗色风格样式表
│   └── media/          # 媒体资源 (图片、视频等)
├── resources/          # 硬件与设计资源
│   ├── PCB&电路图/      # 立创 EDA (EasyEDA Pro) 工程文件
│   └── 面板亚克力图纸/   # 外壳亚克力面板图纸
├── managed_components/ # ESP-IDF 组件管理器下载的依赖
├── partitions.csv      # 分区表配置
├── sdkconfig           # 项目配置文件
└── CMakeLists.txt      # 项目构建脚本
```

## 各部分说明

### 固件 (`/`)

#### `main/app_main.cpp` — 启动入口
按严格顺序初始化整个系统：基础存储 → 网络层 → 中央控制器 → Web 服务。这条链路确保每一项启动时依赖的前置模块已经就绪。

#### `in12_driver` — 辉光管驱动
最底层的硬件操作。通过 74HC595 移位寄存器将 16 位数据串行推入，再经 4 片 BCD 译码器（如 74141 / K155ID1 ）驱动四颗 IN-12 辉光管。支持设置四位数字、熄屏、控制氖泡、控制高压电源。

#### `controller` — 中央控制器
系统的决策中枢，负责"屏幕上到底显示什么"。按优先级依次判断：
1. 防阴极中毒动画（最高优先级，0→9 循环）
2. 总开关（关闭则熄屏）
3. 展示模式（手动指定固定数字）
4. 定时熄灯时段（夜间自动关闭）
5. 正常时钟显示（默认 HH:MM）

同时接管氖泡闪烁逻辑、高压电源管理、设置参数的 NVS 读写。设置变更通过 `applySettings()` 原子应用并立即刷新硬件。

#### `timezone_module` — 时间管理器
- **NTP 校时**：上电自动从 `ntp.aliyun.com` 和 `pool.ntp.org` 获取精确时间
- **时间分发**：内部 100ms 周期轮询 RTC，分离出半秒粒度和分钟粒度回调，分别驱动氖泡闪烁和辉光管数字变化，避免累积误差
- 支持 NTP/手动模式切换、强制同步、手动写时间（写硬件 RTC）

#### `wifi_module` — WiFi 模块
- **AP 热点**：设备启动时创建一个专属配网热点，手机连上即可打开控制面板
- **STA 连接**：从 NVS 读取已保存 WiFi 自动连接；断线自动重连
- **扫描与配网**：前端触发扫描 → 展示可用热点列表 → 输入密码连接 → 连接成功自动保存到 NVS
- 失败原因翻译（密码错误、找不到热点、信号不稳定），直接反馈到前端

#### `web_server` — Web 服务
基于 ESP-IDF HTTP Server，提供 9 个 REST API 路由 + 通配符静态文件兜底。API 涵盖：
- `GET/POST /api/light_data` — 辉光管/氖泡/熄灯/防中毒/展示模式设置
- `GET/POST /api/time_data` — 时区/NTP/手动时间
- `GET /api/wifi/list` — WiFi 列表缓存
- `POST /api/wifi/scan` — 触发扫描并返回结果
- `POST /api/wifi/connect` — 配网
- `GET /api/wifi/status` — 当前连接状态
- `POST /api/reset` — 恢复出厂设置

静态文件服务支持 Range 请求（用于移动端视频播放）。

#### `nvs_module` — 持久化存储
封装 ESP-IDF NVS Flash，提供泛型 `save<T>()` / `load<T>()` 接口，存储 WiFi 凭据和灯管设置参数。

#### `littlefs_module` — 文件系统
挂载 SPI Flash 上的 LittleFS 分区，存放 Web 前端文件（HTML/CSS/JS/媒体资源），供 HTTP 服务器读取。

### 硬件 (`resources/PCB&电路图/`)

立创 EDA Pro 工程，包含 MCU（ESP32-C3）、高压升压模块（170V 驱动辉光管）、74HC595 + BCD 译码器级联、氖泡驱动电路、Type-C 供电等。

### 外壳 (`resources/面板亚克力图纸/`)

亚克力面板的设计图纸，包含 DXF（2D 切割/激光雕刻用）和 Fusion 360 三维模型。

## 使用方法

1. 上电后，设备自动创建名为 `IN12` 的 WiFi 热点
2. 手机连接该热点，浏览器访问 `192.168.10.1`
3. 在"通用"页面连接家中 WiFi，设备自动切换到 STA 模式
4. 首次 NTP 校时后即可正常显示时间
5. 所有设置实时生效、自动保存，断电不丢失

## 设计

Designed by Kivvi
