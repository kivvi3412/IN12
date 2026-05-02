//
// project_config.h — IN12 Nixie Clock 全局配置
//
// 所有跨模块共用的系统级常量集中在此处定义。
// 各 component 的 CMakeLists.txt 中 REQUIRES project_config 即可使用。
//
// ⚠ 此文件不得 #include 任何 ESP-IDF 或第三方头文件，
//   以保证它可以被任意层级的 component 无副作用地引入。
//

#ifndef IN12_PROJECT_CONFIG_H
#define IN12_PROJECT_CONFIG_H

// 硬件引脚配置 C3
#define ST_CP_GPIO GPIO_NUM_10
#define SH_CP_GPIO GPIO_NUM_6
#define DS_GPIO    GPIO_NUM_7
#define NEON_GPIO  GPIO_NUM_0
#define HV_CTL_GPIO GPIO_NUM_1

// WIFI_AP 配置
#define WIFI_AP_SSID     "IN12"
#define WIFI_AP_PASS     "20030212"
#define WIFI_AP_CHANNEL  1
#define WIFI_AP_MAX_CONN 4
#define WIFI_SCAN_MAX    10


// mDNS 主机名：浏览器可通过 http://in12.local 访问
#define MDNS_HOSTNAME         "in12"
#define MDNS_INSTANCE_NAME    "IN12 Nixie Web Config"


//  LittleFS 文件系统
// VFS 挂载点（与 partitions.csv 中 littlefs 分区对应）
#define LITTLEFS_MOUNT_POINT     "/littlefs_root"
// partitions.csv 中的分区 label
#define LITTLEFS_PARTITION_LABEL "littlefs"


// WiFi 配置的 NVS namespace 和 key
#define NVS_NS_WIFI      "wifi_config"
#define NVS_KEY_WIFI     "wifi_config"
// 辉光管/氖泡设置的 NVS namespace 和 key
#define NVS_NS_LIGHT     "light_cfg"
#define NVS_KEY_LIGHT    "settings"


#endif // IN12_PROJECT_CONFIG_H
