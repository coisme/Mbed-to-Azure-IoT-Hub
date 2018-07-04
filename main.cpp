/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Ian Craggs - make sure QoS2 processing works, and add device headers
 *******************************************************************************/

 /**
  This is a sample program to illustrate the use of the MQTT Client library
  on the mbed platform.  The Client class requires two classes which mediate
  access to system interfaces for networking and timing.  As long as these two
  classes provide the required public programming interfaces, it does not matter
  what facilities they use underneath. In this program, they use the mbed
  system libraries.

 */

#define MQTTCLIENT_QOS1 0
#define MQTTCLIENT_QOS2 0

#include "easy-connect.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "MQTT_server_setting.h"
#include "mbed-trace/mbed_trace.h"
#include "mbed_events.h"
#include "NTPClient.h"
#include "jwtgen.h"

#define MQTT_MAX_CONNECTIONS     5
#define MQTT_MAX_PACKET_SIZE  1024

#define TIME_JWT_EXP      (60*60*24)  // 24 hours (MAX)

// LED on/off - This could be different among boards
#define LED_ON  0    
#define LED_OFF 1

#include <string>

/* Flag to be set when a message needs to be published, i.e. BUTTON is pushed. */
static volatile bool isPublish = false;
/* Flag to be set when received a message from the server. */
static volatile bool isMessageArrived = false;
/* Buffer size for a receiving message. */
const int MESSAGE_BUFFER_SIZE = 1024;
/* Buffer for a receiving message. */
char messageBuffer[MESSAGE_BUFFER_SIZE];

// Function prototypes
int getPassword(char *buf, size_t buf_size);
void handleMqttMessage(MQTT::MessageData& md);
void handleButtonRise();


int main(int argc, char* argv[])
{
    mbed_trace_init();
    
    const float version = 0.1;

    NetworkInterface* network = NULL;

    DigitalOut led_red(LED_RED, LED_OFF);
    DigitalOut led_green(LED_GREEN, LED_OFF);
    DigitalOut led_blue(LED_BLUE, LED_OFF);

    printf("Mbed to Google IoT Cloud: version is %.2f\r\n", version);
    printf("\r\n");

    // Turns on green LED to indicate processing initialization process
    led_green = LED_ON;

    printf("Opening network interface...\r\n");
    {
        network = easy_connect(true);    // If true, prints out connection details.
        if (!network) {
            printf("Unable to open network interface.\r\n");
            return -1;
        }
    }
    printf("Network interface opened successfully.\r\n");
    printf("\r\n");


    /* NTP - Get the current time. Required to generate JWT. */
    NTPClient ntp(network);
    ntp.set_server("time.google.com", 123);
    time_t now = ntp.get_timestamp();
    set_time(now);

    /* JWT - Set JWT as password.  */
    const int buf_size = 1024;
    char* password = new char[buf_size];
    if(getPassword(password, buf_size) != 0) {
        printf("ERROR: Failed to generate JWT.\r\n");
        return -1;
    }
    printf("JWT:\r\n%s\r\n\r\n", password);

    /* Establish a network connection. */
    MQTTNetwork* mqttNetwork = NULL;
    printf("Connecting to host %s:%d ...\r\n", MQTT_SERVER_HOST_NAME, MQTT_SERVER_PORT);
    {
        mqttNetwork = new MQTTNetwork(network);
        int rc = mqttNetwork->connect(MQTT_SERVER_HOST_NAME, MQTT_SERVER_PORT, SSL_CA_PEM,
                SSL_CLIENT_CERT_PEM, SSL_CLIENT_PRIVATE_KEY_PEM);
        if (rc != MQTT::SUCCESS){
            printf("ERROR: rc from TCP connect is %d\r\n", rc);
            return -1;
        }
    }
    printf("Connection established.\r\n");
    printf("\r\n");

    // Generates MQTT cliend ID from user's parameter in MQTT_server_setting.h
    std::string mqtt_client_id = 
            std::string{"projects/"} + GOOGLE_PROJECT_ID + "/locations/"
            + GOOGLE_REGION + "/registries/" + GOOGLE_REGISTRY + "/devices/" + GOOGLE_DEVICE_ID;

    /* Establish a MQTT connection. */
    MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS>* mqttClient = NULL;
    printf("MQTT client is connecting to the service ...\r\n");
    {
        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
        data.MQTTVersion = 4; // 3 = 3.1 4 = 3.1.1
        data.clientID.cstring = (char *)mqtt_client_id.c_str();
        data.username.cstring = (char *)"ignored";
        data.password.cstring = (char *)password;

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
    std::string mqtt_topic_pub = 
            std::string{"/devices/"} + GOOGLE_DEVICE_ID + "/events";
    std::string mqtt_topic_sub =
            std::string{"/devices/"} + GOOGLE_DEVICE_ID + "/config";

    /* Subscribe a topic. */
    bool isSubscribed = false;
    printf("Client is trying to subscribe a topic \"%s\".\r\n", mqtt_topic_sub.c_str());
    {
        int rc = mqttClient->subscribe(mqtt_topic_sub.c_str(), MQTT::QOS0, handleMqttMessage);
        if (rc != MQTT::SUCCESS) {
            printf("ERROR: rc from MQTT subscribe is %d\r\n", rc);
            return -1;
        }
        isSubscribed = true;
    }
    printf("Client has subscribed a topic \"%s\".\r\n", mqtt_topic_sub.c_str());
    printf("\r\n");

    // Enable button 1 for publishing a message.
    InterruptIn btn1 = InterruptIn(BUTTON1);
    btn1.rise(handleButtonRise);
    
    printf("To send a packet, push the button 1 on your board.\r\n");

    // Compose a message for the first time.
    std::string initStr = std::string{"{\"cloudRegion\":\""} + GOOGLE_REGION + "\",\"deviceId\":\"" 
                + GOOGLE_DEVICE_ID + ",\"registryId\":\"" + GOOGLE_REGISTRY + "\",\"hops\":1}"; 
    char* buf = (char*)initStr.c_str();

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
            // Save the message to resend when the button is pushed.
            buf = messageBuffer;
        }
        /* Button is pushed - publish a message. */
        if(isPublish) {
            isPublish = false;
            static unsigned int id = 0;
            static unsigned int count = 0;

            count++;

            // When sending a message, blue LED lights.
            led_blue = LED_ON;

            MQTT::Message message;
            message.retained = false;
            message.dup = false;

            message.payload = (void*)buf;

            message.qos = MQTT::QOS0;
            message.id = id++;
            message.payloadlen = strlen(buf);
            // Publish a message.
            printf("\r\nPublishing message to the topic %s:\r\n%s\r\n", mqtt_topic_pub.c_str(), buf);
            int rc = mqttClient->publish(mqtt_topic_pub.c_str(), message);
            if(rc != MQTT::SUCCESS) {
                printf("ERROR: rc from MQTT publish is %d\r\n", rc);
            }
            printf("Message published.\r\n");

            led_blue = LED_OFF;
        }
    }

    printf("The client has disconnected.\r\n");

    if(mqttClient) {
        if(isSubscribed) {
            mqttClient->unsubscribe(mqtt_topic_sub.c_str());
            mqttClient->setMessageHandler(mqtt_topic_sub.c_str(), 0);
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
 * Creates a password string (JWT) to connect Google Cloud IoT Core. 
 * @param buf Pointer to the buffer stores password string followed by terminate character, '\0'. 
 * @param buf_size Size of the buffer in bytes.
 * @return 0 when success, otherwise != 0
 */
int getPassword(char *buf, size_t buf_size) {
    int ret = -1;
    size_t len = 0;
    time_t now = time(NULL);

//    jwtgen.setPrivateKey(SSL_CLIENT_PRIVATE_KEY_PEM);
    if(JwtGenerator::getJwt(buf, buf_size, &len, SSL_CLIENT_PRIVATE_KEY_PEM,
            GOOGLE_PROJECT_ID, now, now + TIME_JWT_EXP) == JwtGenerator::SUCCESS) {
        buf[len] = '\0';
        ret = 0;
    }
    return ret;
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
