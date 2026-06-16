/*****************************************************************/ /**
* @file unirtos_aliyun_demo.c
* @brief UNIRTOS ALIYUN Demo Application
* @author lambert.zhao@quectel.com
* @date 2025-12-27
*
* @copyright Copyright (c) 2023 Quectel Wireless Solution, Co., Ltd.
* All Rights Reserved. Quectel Wireless Solution Proprietary and Confidential.
*
* @par EDIT HISTORY FOR MODULE
* <table>
* <tr><th>Date <th>Version <th>Author <th>Description
* <tr><td>2025-12-27 <td>1.0 <td>lambert.zhao <td> Initial version
* </table>
**********************************************************************/

#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qcm_mqtt.h"
#include "qcm_mqtt_config.h"
#include "qcm_proj_config.h"
#include "qcm_sha1.h"
#include "qosa_rtc.h"
#include "qcm_utils.h"
#include "unirtos_app_init_registry.h"

#ifdef CONFIG_QCM_VTLS_FUNC
#include "qcm_vtls_cfg.h"
#endif /* CONFIG_QCM_VTLS_FUNC */

/* This routine demonstrates the one click shortcut operation function of MQTT Aliot
 *
 * The parts that require user attention or modification have been marked with 'TODO' in the comments
 *
 */

/* TODO: Replace the aliot server information you want to connect to
 * Including server URL, login information, and subscribed topics
 *
 */
#define ALIOT_BASIC_DEMO_SERVER_ADDR     "iot-xxxxxxxx.mqtt.iothub.aliyuncs.com" /*!< Aliot server address */
#define ALIOT_BASIC_DEMO_SERVER_PORT     1883                                               /*!< Aliot server port */
#define ALIOT_BASIC_DEMO_TOPIC_SUB       "/xxxxxxxx/device_name/user/get"               /*!< MQTT test topic for subscribing messages */
#define ALIOT_BASIC_DEMO_CLIENTID        "xxxxxxxx.device_name"                                /*!< Aliot client unique identifier */
#define ALIOT_BASIC_DEMO_PRODUCT_KEY     "xxxxxxxx"                                      /*!< Aliot product key */
#define ALIOT_BASIC_DEMO_DEVICE_NAME     "device_name"                                        /*!< Aliot device name */
#define ALIOT_BASIC_DEMO_DEVICE_SECRET   ""                 /*!< Aliot device secret */


#define QOS_LOG_TAG               LOG_TAG_DEMO

#define ALIOT_BASIC_DEMO_TASK_STACK_SIZE 4096 /*!< ALIOT basic demo task stack size in bytes */

#define ALIOT_BASIC_DEMO_MSG_MAX_CNT     5

#define ALIOT_BASIC_DEMO_SIMID           0    /*!< SIM ID */
#define ALIOT_BASIC_DEMO_PDPID           1    /*!< PDP context ID for network connection */
#define ALIOT_BASIC_DEMO_KEEP_ALIVE      60   /*!< Keep alive time in seconds */
#define ALIOT_BASIC_DEMO_DELIVERY_TIME   5    /*!< Retransmission interval in seconds */
#define ALIOT_BASIC_DEMO_DELIVERY_CNT    3    /*!< Retransmission count */

#define ALIOT_BASIC_DEMO_LEN_MAX         257

#define ALIOT_BASIC_DEMO_TEST_CONTENT    "Hello Welcome to test ali mqtt!"    /*!< Test message content */
#define ALIOT_BASIC_DEMO_TOPIC_PUB       "/xxxxxxxx/device_name/user/update" /*!< MQTT test topic for publishing messages */

#define ALIOT_BASIC_DEMO_MQTTS           0                                    /*!< Test MQTT or MQTTS */

/**
 * @brief mqtt_aliot_demo_msg_t structure definition
 *
 * @param msg_id Event ID for identifying different event types
 * @param param1 Data parameter 1 for passing integer data
 * @param argv Data parameter 2 for passing pointer type data
 */
typedef struct
{
    int   msg_id; /*!< Event ID */
    int   param1; /*!< Data */
    void *argv;   /*!< Data */
} mqtt_aliot_demo_msg_t;

/**
 * @brief aliot basic demo context structure
 */
typedef struct
{
    qosa_uint8_t      client_idx; /*!< MQTT client handle */
    qosa_uint32_t     port;       /*!< MQTT server port */
    int               msg_id;     /*!< Packet identifier */
    qosa_msgq_t       msgq;       /*!< Message queue */
    qcm_ssl_config_t *ssl_config; /*!< SSL configuration */
    char             *clientID;   /*!< MQTT client identifier */
    char             *username;   /*!< MQTT client username */
    char             *password;   /*!< MQTT client password */
} mqtt_aliot_demo_ctx_t;

// Client context
static mqtt_aliot_demo_ctx_t *g_mqtt_aliot_ctx = QOSA_NULL;

/*===========================================================================
 *  Functions
 ===========================================================================*/
/**
 * @brief Free ALIOT basic demo context resources
 */
static void unir_mqtt_aliot_demo_ctx_free(void)
{
    /* Free message queue resources */
    if (g_mqtt_aliot_ctx->msgq != QOSA_NULL)
    {
        qosa_msgq_delete(g_mqtt_aliot_ctx->msgq);
        g_mqtt_aliot_ctx->msgq = QOSA_NULL;
    }

    /* Free clientID resources */
    if (g_mqtt_aliot_ctx->clientID != QOSA_NULL)
    {
        qosa_free(g_mqtt_aliot_ctx->clientID);
        g_mqtt_aliot_ctx->clientID = QOSA_NULL;
    }
    /* Free username resources */
    if (g_mqtt_aliot_ctx->username != QOSA_NULL)
    {
        qosa_free(g_mqtt_aliot_ctx->username);
        g_mqtt_aliot_ctx->username = QOSA_NULL;
    }
    /* Free password resources */
    if (g_mqtt_aliot_ctx->password != QOSA_NULL)
    {
        qosa_free(g_mqtt_aliot_ctx->password);
        g_mqtt_aliot_ctx->password = QOSA_NULL;
    }

#ifdef CONFIG_QCM_VTLS_FUNC
    /* Free SSL configuration resources */
    if (g_mqtt_aliot_ctx->ssl_config != QOSA_NULL)
    {
        qosa_free(g_mqtt_aliot_ctx->ssl_config);
        g_mqtt_aliot_ctx->ssl_config = QOSA_NULL;
    }
#endif /* CONFIG_QCM_VTLS_FUNC */

    /* Free MQTT context structure */
    qosa_free(g_mqtt_aliot_ctx);
}

/**
 * @brief Initialize ALIOT basic demo context and configuration parameters
 *
 */
static int unir_mqtt_aliot_demo_ctx_init(qcm_mqtt_config_t *mqtt_aliot_option)
{
    int ret = 0;

    if (mqtt_aliot_option == QOSA_NULL)
    {
        return -1;
    }
    // If global context is not allocated, allocate and zero memory
    if (g_mqtt_aliot_ctx == QOSA_NULL)
    {
        g_mqtt_aliot_ctx = qosa_malloc(sizeof(mqtt_aliot_demo_ctx_t));
        if (g_mqtt_aliot_ctx == QOSA_NULL)
        {
            return -1;
        }
    }
    qosa_memset(g_mqtt_aliot_ctx, 0, sizeof(mqtt_aliot_demo_ctx_t));

    // If message queue is not created, create one for MQTT message processing
    if (g_mqtt_aliot_ctx->msgq == QOSA_NULL)
    {
        ret = qosa_msgq_create(&g_mqtt_aliot_ctx->msgq, sizeof(mqtt_aliot_demo_msg_t), ALIOT_BASIC_DEMO_MSG_MAX_CNT);
        if (ret != QOSA_OK)
        {
            return -1;
        }
    }

    // If clientID is not allocated yet, allocate memory
    if (g_mqtt_aliot_ctx->clientID == QOSA_NULL)
    {
        g_mqtt_aliot_ctx->clientID = qosa_malloc(QOSA_ARRAY_BYTE_512);
        if (g_mqtt_aliot_ctx->clientID == QOSA_NULL)
        {
            return -1;
        }
        qosa_memset(g_mqtt_aliot_ctx->clientID, 0, QOSA_ARRAY_BYTE_512);
    }
    // If username is not allocated yet, allocate memory
    if (g_mqtt_aliot_ctx->username == QOSA_NULL)
    {
        g_mqtt_aliot_ctx->username = qosa_malloc(QOSA_ARRAY_BYTE_512);
        if (g_mqtt_aliot_ctx->username == QOSA_NULL)
        {
            return -1;
        }
        qosa_memset(g_mqtt_aliot_ctx->username, 0, QOSA_ARRAY_BYTE_512);
    }
    // If password is not allocated yet, allocate memory
    if (g_mqtt_aliot_ctx->password == QOSA_NULL)
    {
        g_mqtt_aliot_ctx->password = qosa_malloc(QOSA_ARRAY_BYTE_512);
        if (g_mqtt_aliot_ctx->password == QOSA_NULL)
        {
            return -1;
        }
        qosa_memset(g_mqtt_aliot_ctx->password, 0, QOSA_ARRAY_BYTE_512);
    }

    // Initialize basic fields in context
    g_mqtt_aliot_ctx->msg_id = 1;  //This parameter value can be 0 only when <QoS>=0
    g_mqtt_aliot_ctx->port = ALIOT_BASIC_DEMO_SERVER_PORT;

    // Fill MQTT connection configuration parameters
    mqtt_aliot_option->version = QCM_MQTT_VERSION_V3_1_1;
    mqtt_aliot_option->sim_id = ALIOT_BASIC_DEMO_SIMID;
    mqtt_aliot_option->pdp_cid = ALIOT_BASIC_DEMO_PDPID;
    mqtt_aliot_option->kalive_time = ALIOT_BASIC_DEMO_KEEP_ALIVE;
    mqtt_aliot_option->delivery_time = ALIOT_BASIC_DEMO_DELIVERY_TIME;
    mqtt_aliot_option->delivery_cnt = ALIOT_BASIC_DEMO_DELIVERY_CNT;
    mqtt_aliot_option->clean_session = QOSA_TRUE;
    // Other options can be configured by user, such as will message, etc.

#ifdef CONFIG_QCM_VTLS_FUNC
    // If MQTTS (SSL/TLS) is enabled, configure SSL related parameters
    if (ALIOT_BASIC_DEMO_MQTTS)
    {
        mqtt_aliot_option->ssl_enable = QOSA_TRUE;

        // If SSL configuration is not allocated yet, allocate memory
        if (g_mqtt_aliot_ctx->ssl_config == QOSA_NULL)
        {
            g_mqtt_aliot_ctx->ssl_config = qosa_malloc(sizeof(qcm_ssl_config_t));
            if (g_mqtt_aliot_ctx->ssl_config == QOSA_NULL)
            {
                return -1;
            }
        }

        // Zero SSL configuration structure and set default values
        qosa_memset(g_mqtt_aliot_ctx->ssl_config, 0x00, sizeof(qcm_ssl_config_t));
        g_mqtt_aliot_ctx->ssl_config->ssl_version = QCM_SSL_VERSION_3;   // Use TLS 1.2 version
        g_mqtt_aliot_ctx->ssl_config->auth_mode = QCM_SSL_VERIFY_NULL;   // Disable server certificate verification
        g_mqtt_aliot_ctx->ssl_config->transport = QCM_SSL_TLS_PROTOCOL;  // Use TLS protocol for transport
        g_mqtt_aliot_ctx->ssl_config->ssl_negotiate_timeout = 30;        // TLS handshake timeout is 30 seconds
        g_mqtt_aliot_ctx->ssl_config->ssl_log_debug = 4;                 // Enable TLS debug log output
        g_mqtt_aliot_ctx->ssl_config->sni_enable = QOSA_TRUE;            // Enable SNI (Server Name Indication)

        // Bind SSL configuration to MQTT configuration
        mqtt_aliot_option->ssl_config = g_mqtt_aliot_ctx->ssl_config;
    }
#endif /* CONFIG_QCM_VTLS_FUNC */

    return 0;
}

/**
 * @brief MQTT client event handling callback function
 *
 * This callback function is used to receive MQTT events
 *
 * @param event_id Event ID identifying the type of MQTT event that occurred
 * @param evt_param Event parameter pointer containing event-related data
 * @param user_param User-defined parameter pointer for passing user-specific data
 */
static void unir_mqtt_aliot_demo_event_cb(qcm_mqtt_client_event_e event_id, void *evt_param, void *user_param)
{
    mqtt_aliot_demo_msg_t msg = {0};
    int                   ret = 0;

    QOSA_UNUSED(user_param);
    // Check if event parameter is null, return directly if null
    if (evt_param == QOSA_NULL)
    {
        return;
    }

    QLOGV("event:%d,evt_param:%x", event_id, evt_param);

    // Package message structure, save event information to message
    msg.msg_id = event_id;
    msg.argv = evt_param;

    ret = qosa_msgq_release(g_mqtt_aliot_ctx->msgq, sizeof(mqtt_aliot_demo_msg_t), (qosa_uint8_t *)&msg, QOSA_NO_WAIT);

    // Check message sending result, log error if sending fails
    if (ret != QOSA_OK)
    {
        QLOGE("ret=%x", ret);
    }
}

/**
 * @brief generate aliot login info
 */
static void unir_mqtt_aliot_demo_generate_ali_login_info(void)
{
    qosa_time_t   tv_sec;
    qosa_int32_t  secure_mode = 3;
    char         *content = QOSA_NULL;
    unsigned char digset[20] = {0};

    tv_sec = qosa_get_system_time_seconds();
    tv_sec &= 0x7FFFFFFFFFFFFFFF;

    content = qosa_malloc(QOSA_ARRAY_BYTE_1024);
    if (content == QOSA_NULL)
    {
        return;
    }
    qosa_memset(content, 0, QOSA_ARRAY_BYTE_1024);
    char tv_sec_buf[65] = {0};
    qcm_utils_int64_to_text(tv_sec, tv_sec_buf, 10);
    qosa_snprintf(
        (char *)content,
        1024,
        "clientId%sdeviceName%sproductKey%stimestamp%s",
        ALIOT_BASIC_DEMO_CLIENTID,
        ALIOT_BASIC_DEMO_DEVICE_NAME,
        ALIOT_BASIC_DEMO_PRODUCT_KEY,
        tv_sec_buf
    );
    qosa_snprintf(g_mqtt_aliot_ctx->clientID, 512, "%s|securemode=%d,signmethod=hmacsha1,timestamp=%s|", ALIOT_BASIC_DEMO_CLIENTID, (int)secure_mode, tv_sec_buf);
    qosa_snprintf((char *)g_mqtt_aliot_ctx->username, 512, "%s&%s", ALIOT_BASIC_DEMO_DEVICE_NAME, ALIOT_BASIC_DEMO_PRODUCT_KEY);

    QLOGV("content=%s", content);
    qcm_hmac_sha1(
        (unsigned char *)ALIOT_BASIC_DEMO_DEVICE_SECRET,
        qosa_strlen((char *)ALIOT_BASIC_DEMO_DEVICE_SECRET),
        (unsigned char *)content,
        qosa_strlen((char *)content),
        (unsigned char *)digset
    );
    qosa_snprintf(
        (char *)g_mqtt_aliot_ctx->password,
        512,
        "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02X",
        digset[0],
        digset[1],
        digset[2],
        digset[3],
        digset[4],
        digset[5],
        digset[6],
        digset[7],
        digset[8],
        digset[9],
        digset[10],
        digset[11],
        digset[12],
        digset[13],
        digset[14],
        digset[15],
        digset[16],
        digset[17],
        digset[18],
        digset[19]
    );

    QLOGI("clientid=%s", g_mqtt_aliot_ctx->clientID);
    QLOGV("username=%s", g_mqtt_aliot_ctx->username);
    QLOGV("mqtt.pcfg=%s", g_mqtt_aliot_ctx->password);
}

/**
 * @brief Open MQTT client connection
 *
 * @return 0 on success, -1 on failure
 */
static int unir_mqtt_aliot_demo_open(void)
{
    int               ret = 0;
    qcm_mqtt_config_t mqtt_aliot_option = {0};  // MQTT configuration

    /* Initialize MQTT client default configuration */
    ret = qcm_mqtt_client_default_config(&mqtt_aliot_option);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("default config failed!");
        goto exit;
    }

    ret = unir_mqtt_aliot_demo_ctx_init(&mqtt_aliot_option);
    if (ret != 0)
    {
        QLOGE("init failed!");
        goto exit;
    }

    /* Create MQTT client instance */
    g_mqtt_aliot_ctx->client_idx = qcm_mqtt_client_create();
    QLOGI("init client_idx=%d", g_mqtt_aliot_ctx->client_idx);

    /* Initialize MQTT client and set event callback function */
    ret = qcm_mqtt_client_init(g_mqtt_aliot_ctx->client_idx, &mqtt_aliot_option, unir_mqtt_aliot_demo_event_cb, QOSA_NULL);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("init failed!");
        goto exit;
    }

    /* Establish MQTT server connection, equivalent to TCP connection */
    ret = qcm_mqtt_client_open(g_mqtt_aliot_ctx->client_idx, ALIOT_BASIC_DEMO_SERVER_ADDR, g_mqtt_aliot_ctx->port);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("open failed!");
        goto exit;
    }

    return 0;
exit:
    return -1;
}

/**
 * @brief Connect to MQTT server
 *
 * This function is used to establish a connection between the MQTT client and server,
 * using predefined client ID, username, and password for connection authentication
 * @note Some cloud platforms or servers may require special handling of client_idx,
 * modify according to actual situation
 */
static int unir_mqtt_aliot_demo_connect(void)
{
    qcm_mqtt_eercode_e ret = 0;  // Store MQTT connection operation return value

    QLOGV("enter");
    unir_mqtt_aliot_demo_generate_ali_login_info();
    /* Call MQTT client connection interface, pass connection parameters including client ID, username, password, etc., equivalent to CONNECT control packet connection */
    ret = qcm_mqtt_client_connect(
        g_mqtt_aliot_ctx->client_idx,
        g_mqtt_aliot_ctx->clientID,
        qosa_strlen(g_mqtt_aliot_ctx->clientID),
        g_mqtt_aliot_ctx->username,
        qosa_strlen(g_mqtt_aliot_ctx->username),
        g_mqtt_aliot_ctx->password,
        qosa_strlen(g_mqtt_aliot_ctx->password)
    );

    /* Check connection result, log error and return error code if connection fails */
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("connect fail=%x", ret);
        return -1;
    }
    return 0;
}

/**
 * @brief MQTT publish message demo function
 *
 * This function demonstrates the MQTT client's message publishing functionality.
 * The function constructs an MQTT message and publishes it to the specified topic.
 *
 * @note When Qos level is 0, the packet identifier field does not exist
 */
static void unir_mqtt_aliot_demo_publish(void)
{
    qcm_mqtt_quality_of_service_e qos = QCM_MQTT_AT_LEAST_ONCE_DELIVERY;  // Qos level
    qosa_bool_t                   retain = QOSA_FALSE;                    // Retain message
    qcm_mqtt_eercode_e            ret = 0;
    qcm_mqtt_pub_config_t         pub_info = {0};

    QLOGV("publish");
    // Increment packet ID and execute MQTT message publish operation
    pub_info.msg_id = g_mqtt_aliot_ctx->msg_id++;
    pub_info.qos = qos;
    pub_info.retain = retain;
    pub_info.topic.data_ptr = ALIOT_BASIC_DEMO_TOPIC_PUB;
    pub_info.topic.data_len = qosa_strlen(ALIOT_BASIC_DEMO_TOPIC_PUB);
    pub_info.payload.data_ptr = (char *)ALIOT_BASIC_DEMO_TEST_CONTENT;
    pub_info.payload.data_len = qosa_strlen(ALIOT_BASIC_DEMO_TEST_CONTENT);

    ret = qcm_mqtt_client_publish(g_mqtt_aliot_ctx->client_idx, &pub_info);

    // Check publish operation result, log error information if failed
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("publish err=%x", ret);
    }
}

/**
 * @brief MQTT client subscribe topic test
 *
 * @note Multiple topics can be added in one subscription, there's no explicit limit in the protocol, but it's not recommended to subscribe to too many topics at once
 */
static void unir_mqtt_aliot_demo_subscribe(void)
{
    qcm_mqtt_client_info_t client_info = {0};                        // Initialize MQTT client info structure
    qcm_mqtt_sub_topic_t   sub_topic[QCM_MQTT_MAX_TOPIC_NUM] = {0};  // Initialize topic array
    int                    i, topic_cnt = 0;                         // Subscribe topic counter
    qcm_mqtt_eercode_e     ret = 0;                                  // Subscribe operation return value
    qcm_mqtt_sub_config_t  sub_info = {0};

    // Check MQTT client connection state, return directly if not connected
    if (qcm_mqtt_client_get_state_info(g_mqtt_aliot_ctx->client_idx, &client_info) != QCM_MQTT_RES_OK || client_info.client_state != QCM_MQTT_STATE_MQTT_CONNECTED)
    {
        QLOGE("state err");
        return;
    }

    // Set three topics to subscribe and their QoS levels
    topic_cnt = 0;

    qcm_mqtt_malloc_data(&sub_topic[topic_cnt].topic, qosa_strlen(ALIOT_BASIC_DEMO_TOPIC_SUB), ALIOT_BASIC_DEMO_TOPIC_SUB);
    sub_topic[topic_cnt].qos = QCM_MQTT_AT_LEAST_ONCE_DELIVERY;
    topic_cnt += 1;

    // Increment packet ID and execute subscribe operation
    sub_info.msg_id = g_mqtt_aliot_ctx->msg_id++;
    sub_info.topics = sub_topic;
    sub_info.topic_cnt = topic_cnt;
    ret = qcm_mqtt_client_subscribe(g_mqtt_aliot_ctx->client_idx, &sub_info);

    QLOGD("topic_cnt=%d,err=%x", topic_cnt, ret);
    // Free topic memory
    for (i = 0; i < topic_cnt; i++)
    {
        qcm_mqtt_free_data(&sub_topic[i].topic);
    }

    // Check subscribe operation result, log error and return if failed
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("ret=%x", ret);
    }

    return;
}

/**
 * @brief MQTT client unsubscribe topic demo function
 *
 * This function will unsubscribe from ALIOT_BASIC_DEMO_TOPIC_SUB
 */
static void unir_mqtt_aliot_demo_unsubscribe(void)
{
    qcm_mqtt_eercode_e    ret = 0;                // Define MQTT operation return code
    int                   i, topic_cnt = 0;       // Subscribe topic counter
    qcm_mqtt_sub_topic_t  un_sub_topic[2] = {0};  // Initialize topic array
    qcm_mqtt_sub_config_t un_sub_info = {0};

    /* Set topics to unsubscribe from */
    topic_cnt = 0;

    qcm_mqtt_malloc_data(&un_sub_topic[topic_cnt].topic, qosa_strlen(ALIOT_BASIC_DEMO_TOPIC_SUB), ALIOT_BASIC_DEMO_TOPIC_SUB);
    un_sub_topic[topic_cnt].qos = QCM_MQTT_AT_LEAST_ONCE_DELIVERY;
    topic_cnt += 1;

    /* Execute unsubscribe operation */
    un_sub_info.msg_id = g_mqtt_aliot_ctx->msg_id++;
    un_sub_info.topics = un_sub_topic;
    un_sub_info.topic_cnt = topic_cnt;
    ret = qcm_mqtt_client_unsubscribe(g_mqtt_aliot_ctx->client_idx, &un_sub_info);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("ret=%x", ret);
    }

    // Free topic memory
    for (i = 0; i < topic_cnt; i++)
    {
        qcm_mqtt_free_data(&un_sub_topic[i].topic);
    }
    return;
}

/**
 * @brief Disconnect MQTT client connection
 *
 * @note The client will send a disconnect packet, indicating a normal disconnection, which will not trigger the will message
 */
static void unir_mqtt_aliot_demo_disconnect(void)
{
    qcm_mqtt_eercode_e ret = 0;

    /* Call MQTT client disconnect interface */
    ret = qcm_mqtt_client_disconnect(g_mqtt_aliot_ctx->client_idx, QOSA_NULL);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("ret=%x", ret);
    }
}

/**
 * @brief Close MQTT client connection
 *
 * @note close means disconnecting the TCP connection, if you don't want a normal disconnection, you can skip disconnect
 * @note Choose between disconnect and close interfaces based on business needs
 */
static void unir_mqtt_aliot_demo_close(void)
{
    qcm_mqtt_eercode_e ret = 0;

    /* Call MQTT client close interface and check the return result */
    ret = qcm_mqtt_client_close(g_mqtt_aliot_ctx->client_idx);
    if (ret != QCM_MQTT_RES_OK)
    {
        QLOGE("ret=%x", ret);
    }
}

/**
 * @brief MQTT event handling function, used to process various event notifications from MQTT client
 *
 * This function processes different MQTT operation results based on the incoming message type (msg_id),
 * including responses to connection, publish, subscribe operations
 *
 * @param msg Pointer to MQTT message structure, containing event ID and parameters
 *            Parameter content varies depending on msg_id, typically various response structures
 *
 * @return 0 on success, -1 or other error codes on failure
 */
static int unir_mqtt_aliot_demo_event_handle(mqtt_aliot_demo_msg_t *msg)
{
    qosa_uint8_t client_idx = 0;        // Client ID, used to identify current MQTT connection
    int          ret = 0;               // Function return value

    if (msg == QOSA_NULL)
    {
        return -1;
    }
    qcm_mqtt_common_resp_t *resp_ptr = (qcm_mqtt_common_resp_t *)msg->argv;
    client_idx = resp_ptr->client_id;
    QLOGV("msg->msg_id=%d, client_idx=%d", msg->msg_id, client_idx);
    // Process different MQTT events based on message ID
    if (msg->msg_id == QCM_MQTT_CLIENT_OPEN_EVENT)
    {
        QLOGV("state open = %x", resp_ptr->result);

        // If TCP connection is successful, try to perform connect connection
        if (resp_ptr->result == QCM_MQTT_RES_OK)
        {
            ret = unir_mqtt_aliot_demo_connect();
            if (ret != 0)
            {
                QLOGE("connect err");
            }
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_CONNECT_EVENT)
    {
        QLOGV("state connect = %x", resp_ptr->result);

        // After successful connection, try subscription test
        if (resp_ptr->result == QCM_MQTT_RES_OK)
        {
            QLOGV("pocotrol_code = %d", resp_ptr->pocotrol_code);
            unir_mqtt_aliot_demo_subscribe();
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_PUBLISH_EVENT)
    {
        // Publish message result returned
        QLOGV("state publish");
        qcm_mqtt_send_resp_t *send_ptr = (qcm_mqtt_send_resp_t *)(resp_ptr->data);

        QLOGD("msg_id:%d, send_result=%d", resp_ptr->msg_id, send_ptr->send_result);
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_SUBSCRIBE_EVENT)
    {
        // Subscribe message result returned
        QLOGV("state subscribe = %x", resp_ptr->result);
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_SUBACK_EVENT)
    {
        // Subscribe message result returned
        QLOGV("state subscribe = %x", resp_ptr->result);
        qcm_mqtt_suback_resp_t *suback_ptr = (qcm_mqtt_suback_resp_t *)resp_ptr->data;

        if (resp_ptr->result == QCM_MQTT_RES_OK)
        {
            if (suback_ptr != QOSA_NULL)
            {
                QLOGI("qos_cnt:%d", suback_ptr->qos_cnt);
                qosa_free(suback_ptr->qoss);  //!!! need free
            }

            // After successful subscription, try to publish message
            unir_mqtt_aliot_demo_publish();
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_UNSUBSCRIBE_EVENT)
    {
        // Unsubscribe message result returned
        QLOGV("state unsubscribe = %x", resp_ptr->result);
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_UNSUBACK_EVENT)
    {
        // Unsubscribe message result returned
        QLOGV("state unsubscribe = %x", resp_ptr->result);
        if (resp_ptr->result == QCM_MQTT_RES_OK)
        {
            // Disconnect test
            unir_mqtt_aliot_demo_disconnect();
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_STATE_EVENT)
    {
        // Disconnect event report, can confirm disconnection reason based on result code
        QLOGV("state state");
        qcm_mqtt_close_cause_t *close_cause_ptr = (qcm_mqtt_close_cause_t *)resp_ptr->data;
        if (close_cause_ptr != QOSA_NULL)
        {
            QLOGV("close cause = %d", close_cause_ptr->close_cause);
        }
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_DISCONNECT_EVENT)
    {
        // disconnect event
        QLOGE("state disconnect = %d", resp_ptr->pocotrol_code);
    }
    else if (msg->msg_id == QCM_MQTT_CLIENT_CLOSE_EVENT)
    {
        // close event
        QLOGV("state close = %x", resp_ptr->result);
    }

    if (resp_ptr->data != QOSA_NULL)
    {
        qosa_free(resp_ptr->data);
    }
    qosa_free(resp_ptr);

    return ret;
}

/**
 * @brief Read MQTT received messages
 */
static void unir_mqtt_aliot_demo_recv_new_message(mqtt_aliot_demo_msg_t *msg)
{
    qosa_uint8_t               client_idx = 0;           // Client ID
    qosa_uint32_t              payload_len = 0;          // Message length
    qcm_mqtt_recv_pub_t        recv_pub = {0};           // Topic
    qcm_mqtt_new_msg_notify_t *new_msg_ptr = QOSA_NULL;  // New message notification structure
    qosa_uint8_t               store_id = 0;             // Store ID

    // Get client ID and free notification message structure memory
    qcm_mqtt_common_resp_t *resp_ptr = (qcm_mqtt_common_resp_t *)msg->argv;
    client_idx = resp_ptr->client_id;

    new_msg_ptr = (qcm_mqtt_new_msg_notify_t *)resp_ptr->data;
    store_id = new_msg_ptr->store_id;
    QLOGD("client_idx=%d, new_message=%d", client_idx, store_id);
    // Read subscribed message content from MQTT client
    if (qcm_mqtt_client_read_subcribe_message(client_idx, store_id, &recv_pub) == QCM_MQTT_RES_OK)
    {
        QLOGD("topic = %s, payload_len = %d", recv_pub.topic.data_ptr, payload_len);
        QLOGI("recv new MQTT msg, payload = %s", recv_pub.payload.data_ptr);

        // Check for preset topic and unsubscribe if found
        if (qosa_memcmp(ALIOT_BASIC_DEMO_TOPIC_SUB, recv_pub.topic.data_ptr, qosa_strlen(ALIOT_BASIC_DEMO_TOPIC_SUB)) == 0)
        {
            unir_mqtt_aliot_demo_unsubscribe();
        }
        // Free topic and payload memory
        qcm_mqtt_free_data(&recv_pub.topic);
        qcm_mqtt_free_data(&recv_pub.payload);
    }

    if (resp_ptr->data != QOSA_NULL)
    {
        qosa_free(resp_ptr->data);
    }
    qosa_free(resp_ptr);
}

/**
 * @brief ALIOT basic demo task main function, responsible for initializing MQTT client and handling various MQTT events
 */
static void unirtos_aliyun_demo_task(void *argv)
{
    int                    ret = 0;     // Function return value
    mqtt_aliot_demo_msg_t  msg = {0};   // MQTT message structure
    qcm_mqtt_client_info_t info = {0};  // MQTT client information structure
    QOSA_UNUSED(argv);

    // Wait for system initialization to complete and facilitate log debugging
    qosa_task_sleep_sec(10);
    ret = unir_mqtt_aliot_demo_open();
    if (ret != 0)
    {
        goto exit;
    }

    // Main event processing loop
    while (1)
    {
        qosa_memset(&msg, 0, sizeof(mqtt_aliot_demo_msg_t));
        qosa_msgq_wait(g_mqtt_aliot_ctx->msgq, (qosa_uint8_t *)&msg, sizeof(mqtt_aliot_demo_msg_t), QOSA_WAIT_FOREVER);
        QLOGD("msg_id=%d", msg.msg_id);

        // Dispatch and process different events based on message ID
        switch (msg.msg_id)
        {
            case QCM_MQTT_CLIENT_OPEN_EVENT:
                ret = unir_mqtt_aliot_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_CONNECT_EVENT:
                ret = unir_mqtt_aliot_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_PUBLISH_EVENT:
                ret = unir_mqtt_aliot_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_SUBSCRIBE_EVENT:
                ret = unir_mqtt_aliot_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_SUBACK_EVENT:
                ret = unir_mqtt_aliot_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_UNSUBSCRIBE_EVENT:
                ret = unir_mqtt_aliot_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_UNSUBACK_EVENT:
                ret = unir_mqtt_aliot_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_NEW_MESSAGE_EVENT:
                unir_mqtt_aliot_demo_recv_new_message(&msg);
                break;
            case QCM_MQTT_CLIENT_STATE_EVENT:
                ret = unir_mqtt_aliot_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_DISCONNECT_EVENT:
                ret = unir_mqtt_aliot_demo_event_handle(&msg);
                break;
            case QCM_MQTT_CLIENT_CLOSE_EVENT:
                ret = unir_mqtt_aliot_demo_event_handle(&msg);
                break;
            default:
                break;
        }

        // Get and record current MQTT client state
        qosa_memset(&info, 0, sizeof(qcm_mqtt_client_info_t));
        qcm_mqtt_client_get_state_info(g_mqtt_aliot_ctx->client_idx, &info);
        QLOGD("state=%d", info.client_state);

        // The following close is just for convenience of single-threaded demo, actual closing is determined by the user
        // Decide whether to close the client based on state
        if (info.client_state == QCM_MQTT_STATE_TCP_CLOSING || info.client_state == QCM_MQTT_STATE_MQTT_DISCONNECTING)
        {
            unir_mqtt_aliot_demo_close();
        }

        // If connection is closed or in initial state, exit task
        if (info.client_state == QCM_MQTT_STATE_TCP_CLOSED || info.client_state == QCM_MQTT_STATE_INIT)
        {
            goto exit;
        }

        // If error occurs during processing, also exit task
        if (ret != 0)
        {
            goto exit;
        }
    }

exit:
    QLOGV("end");
    // Free resources
    unir_mqtt_aliot_demo_ctx_free();
}

/**
 * @note This function is responsible for creating the Easy Aliot Basic demonstration task
 * The task demonstrates the basic usage of MQTT API to connect to Aliot servers, MQTTS testing can be selected via ALIOT_BASIC_DEMO_MQTTS
 */
void unirtos_aliyun_demo_init(void)
{
    int         err = 0;
    qosa_task_t mqtt_aliot_task = QOSA_NULL;

    err = qosa_task_create(&mqtt_aliot_task, ALIOT_BASIC_DEMO_TASK_STACK_SIZE, QOSA_PRIORITY_NORMAL, "unirtos_aliyun_demo", unirtos_aliyun_demo_task, QOSA_NULL);
    if (err != QOSA_OK)
    {
        QLOGE("task create error");
        return;
    }
}
UNIRTOS_APP_EXPORT(420,"ali_demo_test",unirtos_aliyun_demo_init);
