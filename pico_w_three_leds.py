import time
import network
import machine
from umqtt.simple import MQTTClient


WIFI_SSID = "YOUR_WIFI_SSID"
WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"
MQTT_HOST = "172.16.3.215"
MQTT_PORT = 1883
USERNAME = "user1001"
MQTT_PASSWORD = "YOUR_MQTT_PASSWORD"

LED_PINS = {
    "1": 15,
    "2": 14,
    "3": 13,
}

BASE_TOPIC = "classroom/{}/".format(USERNAME)
CONTROL_TOPICS = {
    led: (BASE_TOPIC + "control/led/" + led).encode()
    for led in LED_PINS
}
STATUS_TOPICS = {
    led: (BASE_TOPIC + "status/led/" + led).encode()
    for led in LED_PINS
}

led_outputs = {
    led: machine.Pin(pin, machine.Pin.OUT)
    for led, pin in LED_PINS.items()
}
wlan = network.WLAN(network.STA_IF)
mqtt = None


def publish_status(led, is_on):
    mqtt.publish(
        STATUS_TOPICS[led],
        b"ON" if is_on else b"OFF",
        retain=True,
        qos=1,
    )


def handle_message(topic, payload):
    for led, control_topic in CONTROL_TOPICS.items():
        if topic != control_topic:
            continue

        if payload == b"ON":
            led_outputs[led].value(1)
            publish_status(led, True)
        elif payload == b"OFF":
            led_outputs[led].value(0)
            publish_status(led, False)
        return


def connect_wifi():
    wlan.active(True)
    if wlan.isconnected():
        return

    print("Connecting to Wi-Fi...")
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)
    while not wlan.isconnected():
        time.sleep(1)

    print("Wi-Fi connected:", wlan.ifconfig()[0])


def connect_mqtt():
    global mqtt

    client_id = ("pico-" + USERNAME).encode()
    client = MQTTClient(
        client_id,
        MQTT_HOST,
        port=MQTT_PORT,
        user=USERNAME,
        password=MQTT_PASSWORD,
        keepalive=30,
    )
    client.set_callback(handle_message)
    client.connect()

    for topic in CONTROL_TOPICS.values():
        client.subscribe(topic, qos=1)

    mqtt = client
    for led, output in led_outputs.items():
        publish_status(led, bool(output.value()))

    print("MQTT connected; subscribed to all LED controls")


for output in led_outputs.values():
    output.value(0)

while True:
    try:
        connect_wifi()
        if mqtt is None:
            connect_mqtt()

        mqtt.check_msg()
        time.sleep_ms(50)
    except Exception as error:
        print("Connection lost:", error)
        if mqtt is not None:
            try:
                mqtt.disconnect()
            except Exception as disconnect_error:
                print("MQTT disconnect failed:", disconnect_error)
        mqtt = None
        time.sleep(2)
