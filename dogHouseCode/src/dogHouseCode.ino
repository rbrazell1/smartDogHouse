/*
 * Project dogHouseCode
 * Description:
 * Author:
 * Date:
 */
#include <Adafruit_MQTT.h>
#include "Adafruit_MQTT/Adafruit_MQTT.h"
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "neopixel.h"
#include "IOTTimer.h"
#include "credentials.h"
#include "Adafruit_MCP9808.h"

// Pins
const int NP_HEALTH_INDECATOR_PIN = A0;
const int AC_RELAY_PIN = D8;
const int HEAT_TRACE_RELAY_PIN = D7;
const int RADIANT_HEATER_RELAY_PIN = D6;
const int FAN_HEATER_RELAY_PIN = D5;

const int NP_HEALTH_INDECATOR_COUNT = 4;
const int NP_HEALTH_INDECATOR_BRIGHTNESS = 255;

int int_insideTempF = -21;

bool ACRunning;
bool heatTraceOn;
bool radiantHeaterOn;
bool fanHeaterRunning;

float insideTempF;
float outsideTempF;

float insideTempC;
float outsideTempC;

String lastupdateTime;

// Timers
IOTTimer connectTimer;
IOTTimer publishTimer;
IOTTimer brightnessTimer;
IOTTimer shortCycleTimer;

// MQTT vars
unsigned long last;
unsigned long lastTime;

// MQTT Constructors
TCPClient TheClient;

Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Publish
Adafruit_MQTT_Publish mqttPubinsideTemp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME
                                                                "/feeds/InsideTempF");
Adafruit_MQTT_Publish mqttPuboutsideTemp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME
                                                                 "/feeds/outsideTempF");
Adafruit_MQTT_Publish mqttPubACRunning = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME
                                                               "/feeds/acrunning");
Adafruit_MQTT_Publish mqttPubHeatTrace = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME
                                                               "/feeds/heatTrace");
Adafruit_MQTT_Publish mqttPubradiantHeater = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME
                                                                   "/feeds/radiantHeater");
Adafruit_MQTT_Publish mqttPubFanHeater = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME
                                                               "/feeds/fanHeater");
Adafruit_MQTT_Publish mqttPubLastUpdateTime = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME
                                                                    "/feeds/LastUpdateTime");

// NeoPixel
Adafruit_NeoPixel NPIndecator(NP_HEALTH_INDECATOR_COUNT, NP_HEALTH_INDECATOR_PIN, WS2812);

// MCP 9808 temp sensors
Adafruit_MCP9808 insideTempSensor;
Adafruit_MCP9808 outsideTempSensor;

void setup()
{
    Serial.begin(115200);
    waitFor(Serial.isConnected, 5000);
    NPIndecator.setBrightness(NP_HEALTH_INDECATOR_BRIGHTNESS);
    NPIndecator.begin();
    NPIndecator.show();
    NPIndecator.clear();

    insideTempSensor.begin(0x19);
    outsideTempSensor.begin();
    // 0 is wake 1 is shutdown
    insideTempSensor.shutdown_wake(0);
    outsideTempSensor.shutdown_wake(0);

    // AC NP
    NPIndecator.setPixelColor(0, 0xCC5500);
    // Heat Trace NP
    NPIndecator.setPixelColor(1, 0x0000FF);
    // Fan Heater NP
    NPIndecator.setPixelColor(2, 0xFF0000);
    // Radiant Heater NP
    NPIndecator.setPixelColor(3, 0xFF0000);
    NPIndecator.show();
    Serial.println("Pixel Color check complete");

    shortCycleTimer.startTimer(1500);

    pinMode(AC_RELAY_PIN, OUTPUT);
    pinMode(HEAT_TRACE_RELAY_PIN, OUTPUT);
    pinMode(FAN_HEATER_RELAY_PIN, OUTPUT);
    pinMode(RADIANT_HEATER_RELAY_PIN, OUTPUT);

    MQTT_connect();
}

void loop()
{
    insideTempF = insideTempSensor.readTempF();
    outsideTempF = outsideTempSensor.readTempF();
    Serial.printf("Inside Temp F %f\n", insideTempF);
    Serial.printf("Outside Temp F %f\n", outsideTempF);
    heaterControl(insideTempF);
    heatTraceControl(outsideTempF);
    if (shortCycleTimer.isTimerReady())
    {
        ACControl(insideTempF);
    }
    publishReadings();
    insideTempSensor.shutdown_wake(1);
    outsideTempSensor.shutdown_wake(1);
    delay(1000);
    insideTempSensor.shutdown_wake(0);
    outsideTempSensor.shutdown_wake(0);
    delay(1000);
}

void heaterControl(float currentHeat)
{
    if (currentHeat < 65.0)
    {
        // Heater on
        radiantHeaterOn = true;
        fanHeaterRunning = true;
        digitalWrite(FAN_HEATER_RELAY_PIN, 1);
        digitalWrite(RADIANT_HEATER_RELAY_PIN, 1);
        NPIndecator.setPixelColor(2, 0x0AE400);
        NPIndecator.setPixelColor(3, 0x0AE400);
        NPIndecator.show();
    }
    else if (currentHeat > 69.0)
    {
        // Heater off
        radiantHeaterOn = false;
        fanHeaterRunning = false;
        digitalWrite(FAN_HEATER_RELAY_PIN, 0);
        digitalWrite(RADIANT_HEATER_RELAY_PIN, 0);
        NPIndecator.setPixelColor(2, 0xFF0000);
        NPIndecator.setPixelColor(3, 0xFF0000);
        NPIndecator.show();
    }
    Serial.printf("Radiant heater %i\nFan Heater %i\n", radiantHeaterOn, fanHeaterRunning);
}

void heatTraceControl(float currentHeat)
{
    if (currentHeat < 32.0)
    {
        // Heat trace on
        heatTraceOn = true;
        digitalWrite(HEAT_TRACE_RELAY_PIN, 1);
        NPIndecator.setPixelColor(1, 0x0AE400);
        NPIndecator.show();
    }
    else if (currentHeat > 33.0)
    {
        // Heat trace off
        heatTraceOn = false;
        digitalWrite(HEAT_TRACE_RELAY_PIN, 0);
        NPIndecator.setPixelColor(1, 0xFF0000);
        NPIndecator.show();
    }
    Serial.printf("Heat trace %i\n", heatTraceOn);
}

void ACControl(float currentHeat)
{
    if (currentHeat > 78.0)
    {
        // 600000 is 10 min
        shortCycleTimer.startTimer(600000);
        // AC on
        ACRunning = true;
        digitalWrite(AC_RELAY_PIN, 1);
        NPIndecator.setPixelColor(0, 0x0AE400);
        NPIndecator.show();
    }
    else if (currentHeat < 73.0)
    {
        // AC off
        ACRunning = false;
        digitalWrite(AC_RELAY_PIN, 0);
        NPIndecator.setPixelColor(0, 0xFF0000);
        NPIndecator.show();
    }
    Serial.printf("AC %i\n\n", ACRunning);
}

// Function to send the readings from the water scale to the adafruit dashboard
void publishReadings()
{
    MQTT_connect();
    if ((millis() - last) > 120000)
    {
        Serial.printf("Pinging MQTT \n");
        if (!mqtt.ping())
        {
            Serial.printf("Disconnecting \n");
            mqtt.disconnect();
        }
        last = millis();
    }
    if ((millis() - lastTime > 30000))
    {
        if (mqtt.Update())
        {
            mqttPubinsideTemp.publish(insideTempF);
            mqttPuboutsideTemp.publish(outsideTempF);
            mqttPubradiantHeater.publish(radiantHeaterOn);
            mqttPubFanHeater.publish(fanHeaterRunning);
            mqttPubHeatTrace.publish(heatTraceOn);
            mqttPubACRunning.publish(ACRunning);
            // mqttPubLastUpdateTime.publish(lastupdateTime);
            Serial.printf("Sent %f to the feed\n", insideTempF);
        }
        lastTime = millis();
    }
}

// Function to ensure connection to the adafruit dashboard
void MQTT_connect()
{
    int8_t ret;
    // Stop if already connected.
    if (mqtt.connected())
    {
        return;
    }
    Serial.print("Connecting to MQTT... ");
    while ((ret = mqtt.connect()) != 0)
    { // connect will return 0 for connected
        Serial.printf("%s\n", (char *)mqtt.connectErrorString(ret));
        Serial.printf("Retrying MQTT connection in 5 seconds..\n");
        mqtt.disconnect();
        connectTimer.startTimer(5000);
        while (!connectTimer.isTimerReady())
            ;
    }
    Serial.printf("MQTT Connected!\n");
}