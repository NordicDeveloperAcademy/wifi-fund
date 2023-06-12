# MQTT over Wi-Fi on the nRF7002 DK

This is a sample nRF Connect SDK application for the nRF7002 DK that demonstrates the use of the provisioning service and running MQTT over Wi-Fi.

The sample uses the prvisinoing service to allow you to securely connect the nRF7002 DK to your Wi-Fi network using the [nRF Wi-Fi Provisioner](https://www.nordicsemi.com/Products/Development-tools/nRF-Wi-Fi-Provisioner) mobile app.
Once a connection to the internet is established, the nRF7002 DK will act as an MQTT client, connects to an MQTT broker, and establish bidirectional communication over Wi-Fi with another MQTT client connected to the same broker. Through this connection, you can control the LEDs and monitor the buttons on the nRF7002 DK. 

How to use:

1.  Flash the demo application to your nRF7002 DK.
    
    1.1 Option A: Build from source code
    
    1.2 Option B: Flash the binary files in /hex

2.  Provision the nRF7002 DK to your access point/router:

    2.1 Install the nRF Wi-Fi Provisioner mobile app on your smartphone or tablet.

    2.2 Select your Wi-Fi network from the list of available Wi-Fi networks and enter your Wi-Fi network password (access point/router password)..

3.  Connect to the nRF7002 DK over MQTT using another MQTT client.

4.  Control the LEDs and monitor the buttons of the nRF7002 DK from another MQTT client.

More details can be found on this blog post: [DevZone Blog](https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/implementing-mqtt-over-wi-fi-on-the-nrf7002-development-kit)
