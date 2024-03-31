# IN12 ESP32 时钟

IN12 ESP32 时钟是一个基于 ESP32 开发板的时钟项目，具有以下特点：

- 使用 IN-12 显示时间
- 支持通过网页配置 WiFi 连接和时区设置
- 具有自动时钟模式和自定义数字显示模式
- 可设置防阴极中毒时间，定期刷新显示管
- 使用 LittleFS 文件系统存储网页文件
- 使用 NVS（非易失性存储）保存 WiFi 配置信息

## 软件要求

- ESP-IDF v5.2.1 及以上版本
- 支持 C++11 的编译器

## 功能说明

1. WiFi 连接：通过网页配置 WiFi SSID 和密码，连接到指定的无线网络。
2. 时区设置：可通过网页选择时区，自动同步网络时间。
3. 自动时钟模式：根据设置的时区自动显示当前时间。
4. 自定义数字显示模式：可通过网页输入四位数字，在时钟上显示自定义数字。
5. 防阴极中毒：可设置定期刷新显示管的时间，防止阴极中毒。

## 文件结构

- `components/` - 项目组件目录
   - `display_module/` - 显示模块，控制 IN-12 显示
   - `in12_driver/` - IN-12 驱动程序
   - `littlefs_module/` - LittleFS 文件系统模块
   - `nvs_module/` - NVS 存储模块
   - `timezone_module/` - 时区设置模块
   - `web_server/` - 网页服务器模块
   - `wifi_module/` - WiFi 连接模块
- `littlefs_root/` - LittleFS 文件系统根目录，存储网页文件
- `main/` - 主程序目录
   - `app_main.cpp` - 应用程序入口文件
- `CMakeLists.txt` - CMake 构建脚本
- `README.md` - 项目说明文件

## 如何编译和烧录

1. 确保已安装 ESP-IDF v5.2.1 及以上版本。
2. 克隆本项目到本地：`git clone https://github.com/kivvi3412/in12.git`
3. 进入项目目录：`cd in12`
4. 编译项目：`idf.py build`
5. 将编译生成的二进制文件烧录到 ESP32 开发板：`idf.py -p PORT flash`，其中 `PORT` 为开发板的串口号。
6. 打开串口监视器查看输出：`idf.py -p PORT monitor`

## 如何使用

1. 将 ESP32 开发板连接到电源。
2. 使用手机或电脑连接到 ESP32 开发板创建的 WiFi 热点（SSID 为 "IN12"，密码为 "12345678"）。
3. 打开浏览器，访问 `http://in12.local` 或 `http://192.168.10.1`，进入时钟配置页面。
4. 在网页中配置 WiFi SSID 和密码，选择时区，设置显示模式和防阴极中毒时间。
5. 保存配置后，ESP32 开发板将连接到指定的 WiFi 网络，并根据设置的模式显示时间或自定义数字。

## 本项目其他说明
1. 包管理器官网 & 本项目使用到的包
   > [乐鑫的包管理器官网](https://components.espressif.com/)
   - [mdns](https://components.espressif.com/components/espressif/mdns)
      - `idf.py add-dependency "espressif/mdns^1.2.5"`
   - [littlefs](https://components.espressif.com/components/joltwallet/littlefs)
      - `idf.py add-dependency "joltwallet/littlefs^1.14.2"`
   - 安装完后执行 `idf.py reconfigure` 自动下载依赖包
2. 生成最小配置文件
   - `idf.py menuconfig`
   - 按`D`, 输入` ../sdkconfig.defaults` 生成最小配置文件