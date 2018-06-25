#ifndef __MQTT_SERVER_SETTING_H__
#define __MQTT_SERVER_SETTING_H__

const char MQTT_SERVER_HOST_NAME[] = "<< REPLACE_HERE >>";
const char MQTT_CLIENT_ID[] = "<< REPLACE_HERE >>";
const char MQTT_USERNAME[] = "<< REPLACE_HERE >>";
const char MQTT_PASSWORD[] = "<< REPLACE_HERE >>";
const char MQTT_TOPIC_PUB[] = "<< REPLACE_HERE >>";
const char MQTT_TOPIC_SUB[] = "<< REPLACE_HERE >>";


const int MQTT_SERVER_PORT = 8883;

/*
 * Root CA certificate here in PEM format.
 * "-----BEGIN CERTIFICATE-----\n"
 * ...
 * "-----END CERTIFICATE-----\n";
 */
const char* SSL_CA_PEM = NULL;

/*
 * (optional) Client certificate here in PEM format.
 * Set NULL if you don't use.
 * "-----BEGIN CERTIFICATE-----\n"
 * ...
 * "-----END CERTIFICATE-----\n";
 */
const char* SSL_CLIENT_CERT_PEM = NULL;


/*
 * (optional) Client private key here in PEM format.
 * Set NULL if you don't use.
 * "-----BEGIN RSA PRIVATE KEY-----\n"
 * ...
 * "-----END RSA PRIVATE KEY-----\n";
 */
const char* SSL_CLIENT_PRIVATE_KEY_PEM = NULL;

#endif /* __MQTT_SERVER_SETTING_H__ */
