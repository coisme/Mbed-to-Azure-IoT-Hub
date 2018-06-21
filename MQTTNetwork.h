#ifndef _MQTTNETWORK_H_
#define _MQTTNETWORK_H_

#include "NetworkInterface.h"

#include "TLSSocket.h"

class MQTTNetwork {
public:
    MQTTNetwork(NetworkInterface* aNetwork) : network(aNetwork) {
        socket = new TLSSocket();
    }

    ~MQTTNetwork() {
        delete socket;
    }

    int read(unsigned char* buffer, int len, int timeout) {
        socket->set_timeout(timeout);
        return socket->recv(buffer, len);
    }

    int write(unsigned char* buffer, int len, int timeout) {
        return socket->send(buffer, len);
    }

    int connect(const char* hostname, int port, const char *ssl_ca_pem = NULL,
            const char *ssl_cli_pem = NULL, const char *ssl_pk_pem = NULL) {
        nsapi_error_t ret = 0;
        if((ret = socket->open(network)) != 0) {
            return ret;
        }
        socket->set_root_ca_cert(ssl_ca_pem);
        socket->set_client_cert_key(ssl_cli_pem, ssl_pk_pem);
        return socket->connect(hostname, port);
    }

    int disconnect() {
        return socket->close();
    }

private:
    NetworkInterface* network;
    TLSSocket* socket;
};

#endif // _MQTTNETWORK_H_
