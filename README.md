# Aliyun MQTT Demo

## 概述

Aliyun MQTT Demo是一个基于UniRTOS的示例项目，演示了如何使用UniRTOS平台通过MQTT协议连接阿里云IoT平台。该项目支持设备认证、消息订阅与发布、TLS安全连接等功能，帮助开发者快速上手UniRTOS与阿里云IoT的对接开发。

适用平台：所有支持UniRTOS的平台

## 快速上手

### 1. 开发环境搭建

参考[UniRTOS快速入门](https://docs.quectel.com/zh/UniRTOS/UniRTOS%E6%96%87%E6%A1%A3/%E5%BF%AB%E9%80%9F%E4%B8%8A%E6%89%8B/%E5%BF%AB%E9%80%9F%E4%B8%8A%E6%89%8B.html)文档，了解如何搭建开发环境，了解开发过程。

### 2. 获取项目

进入unirtos/qos_applications目录下执行以下命令下载项目：

```
git clone https://github.com/UniRTOS/unirtos_aliyun_demos.git
```

### 3. 项目结构

```
unirtos_aliyun_demos/
├── CMakeLists.txt           # CMake构建配置
├── demo.manifest.json       # 应用清单文件
├── README.md               # 本文件
├── unirtos_aliyun_demo.c   # 源代码
└── depend.config           # 依赖配置
```

### 4. 构建项目

unirtos目录下运行以下命令进行构建：

```
unirtos make EG800ZCN_LA EG800ZCNLAR01A01M04_BETA_OCPU_20260616
```

### 5. 配置说明

在源代码中修改以下宏定义以匹配你的阿里云IoT设备信息：

| 宏定义 | 说明 |
|--------|------|
| `ALIOT_BASIC_DEMO_SERVER_ADDR` | 阿里云MQTT服务器地址 |
| `ALIOT_BASIC_DEMO_SERVER_PORT` | 服务器端口号 |
| `ALIOT_BASIC_DEMO_TOPIC_SUB` | 订阅的MQTT主题 |
| `ALIOT_BASIC_DEMO_CLIENTID` | MQTT客户端标识 |
| `ALIOT_BASIC_DEMO_PRODUCT_KEY` | 阿里云产品Key |
| `ALIOT_BASIC_DEMO_DEVICE_NAME` | 设备名称 |
| `ALIOT_BASIC_DEMO_DEVICE_SECRET` | 设备密钥 |

### 6. 日志展示

成功运行后，您会在串口看到类似以下日志输出：

```
[I/DEMO] unir_mqtt_aliot_demo_event_cb 281 event:11,evt_param:c111b9c
[I/DEMO] unir_mqtt aliot demo recv new message 749 client idx=1, new message=0
```

## 代码概览

### 参数配置

在源代码中通过宏定义配置云平台MQTT连接参数：

```c
#define ALIOT_BASIC_DEMO_SERVER_ADDR     "iot-xxxxxxxx.mqtt.iothub.aliyuncs.com"
#define ALIOT_BASIC_DEMO_SERVER_PORT     1883
#define ALIOT_BASIC_DEMO_TOPIC_SUB       "/xxxxxxxx/device_name/user/get"
#define ALIOT_BASIC_DEMO_CLIENTID        "xxxxxxxx.device_name"
#define ALIOT_BASIC_DEMO_PRODUCT_KEY     "xxxxxxxx"
#define ALIOT_BASIC_DEMO_DEVICE_NAME     "device_name"
#define ALIOT_BASIC_DEMO_DEVICE_SECRET   ""
#define ALIOT_BASIC_DEMO_TEST_CONTENT    "Hello Welcome to test ali mqtt!"
#define ALIOT_BASIC_DEMO_TOPIC_PUB       "/xxxxxxxx/device_name/user/update"
#define ALIOT_BASIC_DEMO_MQTTS           0
```

系统参数包括任务栈大小、消息队列深度、SIM卡ID、PDP上下文ID、MQTT保活时间、重传间隔与次数等。

### 结构体定义

- **mqtt_aliot_demo_msg_t**：消息队列中的消息结构，包含事件ID和数据指针。
- **mqtt_aliot_demo_ctx_t**：全局保存MQTT连接上下文，供初始化和反初始化等操作使用。

### 主要 API 接口

#### unirtos_aliyun_demo_init

任务初始化函数。通过`UNIRTOS_APP_EXPORT`宏注册应用入口，创建名为"ali_demo_test"的MQTT demo任务，设置堆栈大小（4096字节）和优先级。

#### unir_mqtt_aliot_demo_ctx_init

初始化MQTT配置上下文，为结构体字段分配内存并填充MQTT连接选项。

- 为全局上下文分配内存并清零
- 创建消息队列用于异步接收MQTT事件，队列深度为5条
- 分别为clientID、username、password各分配512字节缓冲区
- 填写MQTT协议版本（v3.1.1）、SIM卡ID、PDP上下文ID、保活时间（60s）、重传间隔（5s）、重传次数（3次）及会话清除标志
- 若启用MQTTS，则分配并配置SSL结构体，设置TLS版本、认证模式、握手超时（30s）及SNI开关

#### unir_mqtt_aliot_demo_generate_ali_login_info

按照阿里云物联网平台的签名规范，使用HMAC-SHA1算法生成MQTT登录所需的clientID、username和password。

- 获取当前系统时间戳作为签名防重放参数
- 按规范构造待签名内容：`clientId{...}deviceName{...}productKey{...}timestamp{...}`
- 以`DEVICE_SECRET`为密钥进行HMAC-SHA1运算，得到20字节摘要
- 将摘要格式化为40位十六进制字符串，作为MQTT连接的password

#### unir_mqtt_aliot_demo_open

完成MQTT客户端的完整初始化并建立TCP连接。

- 获取MQTT客户端默认配置
- 初始化上下文并覆写业务参数
- 创建MQTT客户端实例并绑定事件回调`unir_mqtt_aliot_demo_event_cb`
- 向阿里云服务器发起TCP连接（异步，结果经回调返回）

#### unir_mqtt_aliot_demo_connect

在TCP连接建立成功后，发送MQTT CONNECT报文完成应用层鉴权握手。

- 生成阿里云签名规范的登录凭据
- 调用`qcm_mqtt_client_connect()`传入clientID、username、password发起MQTT CONNECT

#### unir_mqtt_aliot_demo_subscribe

向阿里云物联网平台订阅指定主题（QoS 1），以便接收云端下发的消息。

#### unir_mqtt_aliot_demo_publish

向阿里云物联网平台上报测试消息，QoS级别为1，不保留消息。

#### unir_mqtt_aliot_demo_event_handle

处理来自MQTT客户端的各类异步事件，是连接流程的核心调度函数。按事件类型驱动状态转移：

| 事件 | 处理动作 |
|------|----------|
| `OPEN_EVENT` | TCP连接成功→调用connect发起鉴权握手 |
| `CONNECT_EVENT` | 鉴权成功→调用subscribe订阅主题 |
| `SUBACK_EVENT` | 订阅成功→调用publish上报测试消息 |
| `NEW_MESSAGE_EVENT` | 收到新消息→调用recv_new_message处理 |
| `UNSUBACK_EVENT` | 取消订阅成功→调用disconnect断开MQTT |
| `STATE_EVENT` | 连接状态变更，可获取断连原因 |
| `DISCONNECT_EVENT` / `CLOSE_EVENT` | 记录协议层返回码，释放资源 |

#### unir_mqtt_aliot_demo_recv_new_message

从MQTT接收缓冲中读取云端下发的订阅消息，根据主题匹配执行相应业务逻辑（如取消订阅）。

### 完整连接流程

基于UniRTOS的MQTT客户端采用全异步消息驱动模式，任务在消息队列上永久阻塞等待，MQTT驱动层通过事件回调将异步通知投递到队列，任务取出消息后分发至对应处理函数，驱动状态机按序推进：

```
系统启动
    │
    ▼
unirtos_aliyun_demo_init()
    │  创建演示任务（栈4096字节，优先级NORMAL）
    ▼
unir_easy_aliot_basic_demo_task()
    │  延迟10s，等待网络就绪
    ▼
unir_mqtt_aliot_demo_open()
    ├─ qcm_mqtt_client_default_config()   获取默认MQTT配置
    ├─ unir_mqtt_aliot_demo_ctx_init()    初始化上下文及MQTT选项
    ├─ qcm_mqtt_client_create()           创建客户端实例
    ├─ qcm_mqtt_client_init()             绑定配置与事件回调
    └─ qcm_mqtt_client_open()            建立TCP连接（异步，结果经回调返回）
    │
    ▼ TCP连接结果经回调异步返回
QCM_MQTT_CLIENT_OPEN_EVENT（result=OK）
    └─ unir_mqtt_aliot_demo_connect()
           ├─ generate_ali_login_info()  HMAC-SHA1签名生成凭据
           └─ qcm_mqtt_client_connect() 发送MQTT CONNECT报文
    │
    ▼
QCM_MQTT_CLIENT_CONNECT_EVENT（result=OK）
    └─ unir_mqtt_aliot_demo_subscribe()  订阅目标主题（QoS 1）
    │
    ▼
QCM_MQTT_CLIENT_SUBACK_EVENT（result=OK）
    └─ unir_mqtt_aliot_demo_publish()    上报测试消息（QoS 1）
    │
    ▼
QCM_MQTT_CLIENT_NEW_MESSAGE_EVENT
    └─ unir_mqtt_aliot_demo_recv_new_message()
           └─ unir_mqtt_aliot_demo_unsubscribe()  匹配主题后取消订阅
    │
    ▼
QCM_MQTT_CLIENT_UNSUBACK_EVENT（result=OK）
    └─ unir_mqtt_aliot_demo_disconnect()  发送MQTT DISCONNECT报文
    │
    ▼
QCM_MQTT_CLIENT_STATE_EVENT / QCM_MQTT_CLIENT_DISCONNECT_EVENT
    └─ unir_mqtt_aliot_demo_close()       关闭TCP连接
    │
    ▼
状态变为TCP_CLOSED / INIT
    └─ unir_mqtt_aliot_demo_ctx_free()    释放所有资源，任务退出
```

### 依赖组件

- `qcm_mqtt`: MQTT协议栈组件
- `vtls`: TLS安全传输组件
- `utils`: 通用工具组件

## 论坛社区

[点此进入](https://forumschinese.quectel.com/c/66-category/66)

## 贡献指南

欢迎提交 Issue 和 Pull Request！
