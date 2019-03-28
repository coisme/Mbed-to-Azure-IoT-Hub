# Mbed-to-Azure-IoT-Hub

Mbed OS example program connecting to Azure IoT Hub with MQTT over TLS. You can authenticate through X.509 certificates or through a symmetric key.

## Setting your device identity

### Using an X.509 certificate

See [Azure IoT Hub from Mbed OS device](https://os.mbed.com/users/coisme/notebook/azure-iot-hub-from-mbed-os-device/) for more details on how to create a new certificate.

### Using symmetric keys

1. In the Azure IoT Hub, click **IoT Devices > Add**.
1. Enter a Device ID.
1. Under 'Authentication type', select **Symmetric key**.
1. In this application, open [MQTT_server_setting.h](MQTT_server_setting.h), and:
    * Set `IOTHUB_AUTH_METHOD` to `IOTHUB_AUTH_SYMMETRIC_KEY`.
    * Set `DEVICE_ID` to your Device ID.
    * Set `MQTT_SERVER_HOST_NAME` to your IoT Hub hostname (you can find this under the 'Overview' tab in the IoT Hub).
    * Set `MQTT_SERVER_PASSWORD` to an SAS token.
        * You can generate one easily from the '[Azure IoT Hub Toolkit](https://github.com/Microsoft/vscode-azure-iot-toolkit/wiki/Generate-SAS-Token-for-IoT-Hub)' extension in Visual Studio Code.

If authentication failed, you'll see `ERROR: rc from MQTT connect is 5` in the console.

## Memory usage

Azure IoT Hub uses RSA to authenticate, which requires a significant amount of memory. This library has the following heap requirements:

**Symmetric keys**

```
While connecting: 36594 bytes
After connecting: 30098 bytes
```

**X.509 certificates**

```
While connecting: 54673 bytes
After connecting: 42865 bytes
```

