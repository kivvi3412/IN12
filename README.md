## IN12
### 包管理器
1. 包管理器官网 & 本项目使用到的包
    > [乐鑫的包管理器官网](https://components.espressif.com/)
    - [mdns](https://components.espressif.com/components/espressif/mdns) 
       - `idf.py add-dependency "espressif/mdns^1.2.5"`
    - [littlefs](https://components.espressif.com/components/joltwallet/littlefs)
       - `idf.py add-dependency "joltwallet/littlefs^1.14.2"`
    - 安装完后执行 `idf.py reconfigure` 自动下载依赖包
