// ----------------------------------------------------------------------------
// Copyright 2016-2019 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------

#define MQTTCLIENT_QOS1 0
#define MQTTCLIENT_QOS2 0

#include "mbed.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "MQTT_server_setting.h"
#include "mbed-trace/mbed_trace.h"
#include "mbed_events.h"
#include "NTPClient.h"
#include "mbedtls/error.h"

#define MQTT_MAX_CONNECTIONS     5
#define MQTT_MAX_PACKET_SIZE  1024

#define TIME_JWT_EXP      (60*60*24)  // 24 hours (MAX)

// LED on/off - This could be different among boards
#define LED_ON  0
#define LED_OFF 1

/* Flag to be set when a message needs to be published, i.e. BUTTON is pushed. */
static volatile bool isPublish = false;
/* Flag to be set when received a message from the server. */
static volatile bool isMessageArrived = false;
/* Buffer size for a receiving message. */
const int MESSAGE_BUFFER_SIZE = 1024;
/* Buffer for a receiving message. */
char messageBuffer[MESSAGE_BUFFER_SIZE];

// Function prototypes
void handleMqttMessage(MQTT::MessageData& md);
void handleButtonRise();

int main(int argc, char* argv[])
{
    mbed_trace_init();

    const float version = 0.1;

    NetworkInterface* network = NULL;

    DigitalOut led_red(LED1, LED_OFF);
    DigitalOut led_green(LED2, LED_OFF);
    DigitalOut led_blue(LED3, LED_OFF);

    printf("Mbed to Azure IoT Hub: version is %.2f\r\n", version);
    printf("\r\n");

    // Turns on green LED to indicate processing initialization process
    led_green = LED_ON;

    printf("Opening network interface...\r\n");

    network = NetworkInterface::get_default_instance();
    if (!network) {
        printf("Unable to open network interface.\r\n");
        return -1;
    }

    nsapi_error_t net_status = NSAPI_ERROR_NO_CONNECTION;
    while ((net_status = network->connect()) != NSAPI_ERROR_OK) {
        printf("Unable to connect to network (%d). Retrying...\r\n", net_status);
    }

    printf("Connected to the network successfully. IP address: %s\r\n", network->get_ip_address());
    printf("\r\n");

    // sync the real time clock (RTC)
    {
        NTPClient ntp(network);
        ntp.set_server("time.google.com", 123);
        time_t now = ntp.get_timestamp();
        set_time(now);
        printf("Time is now %s", ctime(&now));
    }

    /* Establish a network connection. */
    MQTTNetwork* mqttNetwork = NULL;
    printf("Connecting to host %s:%d ...\r\n", MQTT_SERVER_HOST_NAME, MQTT_SERVER_PORT);
    {
        mqttNetwork = new MQTTNetwork(network);
#if IOTHUB_AUTH_METHOD == IOTHUB_AUTH_SYMMETRIC_KEY
        int rc = mqttNetwork->connect(MQTT_SERVER_HOST_NAME, MQTT_SERVER_PORT, SSL_CA_PEM);
#elif IOTHUB_AUTH_METHOD == IOTHUB_AUTH_CLIENT_SIDE_CERT
         int rc = mqttNetwork->connect(MQTT_SERVER_HOST_NAME, MQTT_SERVER_PORT, SSL_CA_PEM,
                    SSL_CLIENT_CERT_PEM, SSL_CLIENT_PRIVATE_KEY_PEM);
#endif
        if (rc != MQTT::SUCCESS) {
            const int MAX_TLS_ERROR_CODE = -0x1000;
            // Network error
            // TODO: implement converting an error code into message.
            printf("ERROR from MQTTNetwork connect is %d\r\n", rc);
#if defined(MBEDTLS_ERROR_C) || defined(MBEDTLS_ERROR_STRERROR_DUMMY)
            // TLS error - mbedTLS error codes starts from -0x1000 to -0x8000.
            if(rc <= MAX_TLS_ERROR_CODE) {
                const int buf_size = 256;
                char *buf = new char[buf_size];
                mbedtls_strerror(rc, buf, buf_size);
                printf("TLS ERROR (%d) : %s\r\n", rc, buf);
            }
#endif
            return -1;
        }
    }
    printf("Connection established.\r\n");
    printf("\r\n");

    // Generate username from host name and client id.
    const char *username = MQTT_SERVER_HOST_NAME "/" DEVICE_ID "/api-version=2016-11-14";

    /* Establish a MQTT connection. */
    MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS>* mqttClient = NULL;
    printf("MQTT client is connecting to the service ...\r\n");
    {
        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
        data.MQTTVersion = 4; // 3 = 3.1 4 = 3.1.1
        data.clientID.cstring = (char*)DEVICE_ID;
        data.username.cstring = (char*)username;
        data.password.cstring = (char*)MQTT_SERVER_PASSWORD;

        mqttClient = new MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS>(*mqttNetwork);
        int rc = mqttClient->connect(data);
        if (rc != MQTT::SUCCESS) {
            printf("ERROR: rc from MQTT connect is %d\r\n", rc);
            return -1;
        }
    }
    printf("Client connected.\r\n");
    printf("\r\n");

    // Network initialization done. Turn off the green LED
    led_green = LED_OFF;


    // Generates topic names from user's setting in MQTT_server_setting.h
    //devices/{device_id}/messages/events/
    const char *mqtt_topic_pub = "devices/" DEVICE_ID "/messages/events/";
    const char *mqtt_topic_sub = "devices/" DEVICE_ID "/messages/devicebound/#";

    /* Subscribe a topic. */
    bool isSubscribed = false;
    printf("Client is trying to subscribe a topic \"%s\".\r\n", mqtt_topic_sub);
    {
        int rc = mqttClient->subscribe(mqtt_topic_sub, MQTT::QOS0, handleMqttMessage);
        if (rc != MQTT::SUCCESS) {
            printf("ERROR: rc from MQTT subscribe is %d\r\n", rc);
            return -1;
        }
        isSubscribed = true;
    }
    printf("Client has subscribed a topic \"%s\".\r\n", mqtt_topic_sub);
    printf("\r\n");

    // Enable button 1 for publishing a message.
    InterruptIn btn1(BUTTON1);
    btn1.rise(handleButtonRise);

    printf("To send a packet, push the button 1 on your board.\r\n");

    /* Main loop */
    while(1) {
        /* Client is disconnected. */
        if(!mqttClient->isConnected()){
            break;
        }
        /* Waits a message and handles keepalive. */
        if(mqttClient->yield(100) != MQTT::SUCCESS) {
            break;
        }
        /* Received a message. */
        if(isMessageArrived) {
            isMessageArrived = false;
            printf("\r\nMessage arrived:\r\n%s\r\n", messageBuffer);
        }
        /* Button is pushed - publish a message. */
        if(isPublish) {
            isPublish = false;
            static unsigned int id = 0;
            static unsigned int count = 0;

            // When sending a message, blue LED lights.
            led_blue = LED_ON;

            MQTT::Message message;
            message.retained = false;
            message.dup = false;

            const size_t len = 128;
            char buf[len];
            snprintf(buf, len, "Message #%d from %s.", count, DEVICE_ID);
            message.payload = (void*)buf;

            message.qos = MQTT::QOS0;
            message.id = id++;
            message.payloadlen = strlen(buf);
            // Publish a message.
            printf("\r\nPublishing message to the topic %s:\r\n%s\r\n", mqtt_topic_pub, buf);
            int rc = mqttClient->publish(mqtt_topic_pub, message);
            if(rc != MQTT::SUCCESS) {
                printf("ERROR: rc from MQTT publish is %d\r\n", rc);
            }
            printf("Message published.\r\n");

            count++;

            led_blue = LED_OFF;
        }
    }

    printf("The client has disconnected.\r\n");

    if(mqttClient) {
        if(isSubscribed) {
            mqttClient->unsubscribe(mqtt_topic_sub);
            mqttClient->setMessageHandler(mqtt_topic_sub, 0);
        }
        if(mqttClient->isConnected())
            mqttClient->disconnect();
        delete mqttClient;
    }
    if(mqttNetwork) {
        mqttNetwork->disconnect();
        delete mqttNetwork;
    }
    if(network) {
        network->disconnect();
        // network is not created by new.
    }

    // Turn on the red LED when the program is done.
    led_red = LED_ON;
}

/*
 * Callback function called when a message arrived from server.
 */
void handleMqttMessage(MQTT::MessageData& md)
{
    // Copy payload to the buffer.
    MQTT::Message &message = md.message;
    memcpy(messageBuffer, message.payload, message.payloadlen);
    messageBuffer[message.payloadlen] = '\0';

    isMessageArrived = true;
}

/*
 * Callback function called when button is pushed.
 */
void handleButtonRise() {
    isPublish = true;
}
