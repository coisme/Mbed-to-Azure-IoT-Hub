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

#define LED_ON 0
#define LED_OFF 1

static volatile bool isPublish = false;

/*
 * Callback function called when a message arrived from server.
 */
void messageArrived(MQTT::MessageData& md)
{
    /* TODO: Move printf to outside interrupt context. */
    MQTT::Message &message = md.message;
    printf("\r\n");
    printf("! Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n",
            message.qos, message.retained, message.dup, message.id);
    printf("! Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    printf("\r\n");
}

void btn1_rise_handler() {
    isPublish = true;
}


int main(int argc, char* argv[])
{
    mbed_trace_init();
    
    const float version = 0.8;
    bool isSubscribed = false;

    NetworkInterface* network = NULL;
    MQTTNetwork* mqttNetwork = NULL;
    MQTT::Client<MQTTNetwork, Countdown>* mqttClient = NULL;

    DigitalOut led_red(LED_RED, LED_OFF);
    DigitalOut led_green(LED_GREEN, LED_ON);
    DigitalOut led_blue(LED_BLUE, LED_OFF);

    printf("HelloMQTT: version is %.2f\r\n", version);
    printf("\r\n");

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

    printf("MQTT client is trying to connect the server ...\r\n");
    {
        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
        data.MQTTVersion = 3;
        data.clientID.cstring = (char *)MQTT_CLIENT_ID;
        data.username.cstring = (char *)MQTT_USERNAME;
        data.password.cstring = (char *)MQTT_PASSWORD;

        mqttClient = new MQTT::Client<MQTTNetwork, Countdown>(*mqttNetwork);
        int rc = mqttClient->connect(data);
        if (rc != MQTT::SUCCESS) {
            printf("ERROR: rc from MQTT connect is %d\r\n", rc);
            return -1;
        }
    }
    printf("Client connected.\r\n");
    printf("\r\n");

    // Turn off the green LED
    led_green = LED_OFF;

    printf("Client is trying to subscribe a topic \"%s\".\r\n", MQTT_TOPIC_SUB);
    {
        int rc = mqttClient->subscribe(MQTT_TOPIC_SUB, MQTT::QOS0, messageArrived);
        if (rc != MQTT::SUCCESS) {
            printf("ERROR: rc from MQTT subscribe is %d\r\n", rc);
            return -1;
        }
        isSubscribed = true;
    }
    printf("Client has subscribed a topic \"%s\".\r\n", MQTT_TOPIC_SUB);
    printf("\r\n");

    // Enable button 1
    InterruptIn btn1 = InterruptIn(BUTTON1);
    btn1.rise(btn1_rise_handler);
    
    printf("To send a packet, push the button 1 on your board.\r\n");


    while(1) {
        if(!mqttClient->isConnected()){
            break;
        }
        if(mqttClient->yield(100) != MQTT::SUCCESS) {
            break;
        }
        if(isPublish) {
            isPublish = false;
            static unsigned int id = 0;
            static unsigned int count = 0;

            count++;

            // When sending a message, LED lights blue.
            led_blue = LED_ON;

            MQTT::Message message;
            message.retained = false;
            message.dup = false;

            const size_t buf_size = 100;
            char *buf = new char[buf_size];
            message.payload = (void*)buf;

            message.qos = MQTT::QOS0;
            message.id = id++;
            sprintf(buf, "%d", count);
            message.payloadlen = strlen(buf)+1;
            // Publish a message.
            printf("Publishing message.\r\n");
            int rc = mqttClient->publish(MQTT_TOPIC_SUB, message);
            if(rc != MQTT::SUCCESS) {
                printf("ERROR: rc from MQTT publish is %d\r\n", rc);
            }
            printf("Message published.\r\n");
            delete[] buf;    

            led_blue = LED_OFF;
        }
    }

    printf("The client has disconnected.\r\n");

    if(mqttClient) {
        if(isSubscribed) {
            mqttClient->unsubscribe(MQTT_TOPIC_SUB);
            mqttClient->setMessageHandler(MQTT_TOPIC_SUB, 0);
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

//    exit(0);
}
