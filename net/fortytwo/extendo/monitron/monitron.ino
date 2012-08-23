#include <Wire.h>

////////////////////////////////////////

#define SOUND_PIN         A0
#define VIBR_PIN          A1
#define LIGHT_PIN         A2
#define DUST_PIN          A3
// analog pin 4 is reserved for I2C
// analog pin 5 is reserved for I2C
// A6
// A7

// digital pin 0 is reserved for RX
// digital pin 1 is reserved for TX
#define SPEAKER_PIN       2
#define DUST_LED_PIN      3
// 4 -- light sensor LED
// 5
// 6
#define DHT22_PIN         7
#define MOTION_PIN        8
#define RGB_LED_GREEN_PIN 9
#define RGB_LED_BLUE_PIN  10
#define RGB_LED_RED_PIN   11
// 12
#define LED_PIN           13

////////////////////////////////////////

#define OM_SENSOR_7BB206L0_VIBRN      "/om/sensor/7bb206l0/vibr"
#define OM_SENSOR_ADJDS311CR999_BLUE  "/om/sensor/adjds311cr999/blue"
#define OM_SENSOR_ADJDS311CR999_GREEN "/om/sensor/adjds311cr999/green"
#define OM_SENSOR_ADJDS311CR999_RED   "/om/sensor/adjds311cr999/red"
#define OM_SENSOR_BMP085_PRESSURE     "/om/sensor/bmp085/press"
#define OM_SENSOR_BMP085_TEMP         "/om/sensor/bmp085/temp"
#define OM_SENSOR_GP2Y1010AU0F_DUST   "/om/sensor/gp2y1010au0f/dust"
#define OM_SENSOR_MD9745APZF_SOUND    "/om/sensor/md9745apzf/sound"
#define OM_SENSOR_PHOTO_LIGHT         "/om/sensor/photo/light"
#define OM_SENSOR_RHT03_ERROR         "/om/sensor/rht03/error"
#define OM_SENSOR_RHT03_HUMID         "/om/sensor/rht03/humid"
#define OM_SENSOR_RHT03_TEMP          "/om/sensor/rht03/temp"
#define OM_SENSOR_SE10_MOTION         "/om/sensor/se10/motion"
#define OM_SYSTEM_ERROR               "/om/system/error"
#define OM_SYSTEM_TIME                "/om/system/time"

// Each sampling cycle must take at least this long.
// If a cycle is finished sooner, we will wait before starting the next cycle.
#define CYCLE_MILLIS_MIN  3000

// A sampling cycle may take at most this long.
// If the sample runs over, an error message will be generated
#define CYCLE_MILLIS_MAX  5000

////////////////////////////////////////

#include <AnalogSampler.h>

AnalogSampler sampler_7bb206l0_vibr(VIBR_PIN);
AnalogSampler sampler_adjds311cr999_blue(0);
AnalogSampler sampler_adjds311cr999_green(0);
AnalogSampler sampler_adjds311cr999_red(0);
AnalogSampler sampler_bmp085_press(0);
AnalogSampler sampler_bmp085_temp(0);
AnalogSampler sampler_gp2y1010au0f_dust(0);
AnalogSampler sampler_md9745apzf_sound(SOUND_PIN);
AnalogSampler sampler_photo_light(LIGHT_PIN);
AnalogSampler sampler_rht03_humid(0);
AnalogSampler sampler_rht03_temp(0);

////////////////////////////////////////

#include <DHT22.h>

DHT22 dht22(DHT22_PIN);

////////////////////////////////////////

#include <BMP085.h>

BMP085 bmp085;

////////////////////////////////////////

#include "om_droidspeak.h"
#include "om_dust.h"
#include "om_motion.h"
#include "om_timer.h"
#include "om_rgb_led.h"

////////////////////////////////////////

void setup() {
    pinMode(SPEAKER_PIN, OUTPUT);    
    pinMode(DUST_LED_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(MOTION_PIN, INPUT);
    
    rgb_led_setup();
    
    // TODO: why is this white immediately overridden by red?
    pushColor(WHITE);

    speakPowerUpPhrase();
    
    randomSeed(analogRead(LIGHT_PIN));
 
    Serial.begin(9600);
    bmp085.setup();
        
    //lastCycle_highBits = 0;
    //lastCycle_lowBits = millis();
    
    speakSetupCompletedPhrase(); 
    
    popColor();
    pushColor(GREEN);
}

void loop()
{        
    startCycle();
    digitalWrite(LED_PIN, HIGH);
    
    resetMotionDetector();
    sampleMotionDetector();
    
    //samplePhotoresistor();
    sampleAnalog();
    sampleDHT22();
    sampleBMP085();
    sampleDustSensor();
    
    sampleMotionDetector();
    reportMotionDetectorResults();
    
    //rateTest();
    
    digitalWrite(LED_PIN, LOW);
    endCycle();
}

void beginSample()
{
    pushColor(BLUE);
}

void endSample()
{
    popColor();
}

void beginOSCWrite()
{
    pushColor(WHITE);
    tick();
}

void endOSCWrite()
{
    popColor();
}

void finishAnalogObservation(AnalogSampler s, char* prefix)
{
    beginOSCWrite();
    Serial.print(prefix);
    Serial.print(" ");
    Serial.print(s.getStartTime());
    Serial.print(" ");
    Serial.print(s.getEndTime());
    Serial.print(" ");
    Serial.print(s.getNumberOfMeasurements());
    Serial.print(" ");
    Serial.print(s.getMinValue(), 3);
    Serial.print(" ");
    Serial.print(s.getMaxValue(), 3); 
    Serial.print(" ");
    Serial.print(s.getMean(), 3);
    Serial.print(" ");
    Serial.println(s.getVariance(), 6);
    endOSCWrite();  
    
    s.reset();
}

const unsigned long analogIterations = 10000;

void sampleAnalog()
{    
    beginSample();
    unsigned long now = millis();
    for (unsigned long i = 0; i < analogIterations; i++)
    {
        sampler_7bb206l0_vibr.measure(now);
        sampler_md9745apzf_sound.measure(now);
        sampler_photo_light.measure(now);
    }
    endSample();
    
    finishAnalogObservation(sampler_7bb206l0_vibr, OM_SENSOR_7BB206L0_VIBRN);
    finishAnalogObservation(sampler_md9745apzf_sound, OM_SENSOR_MD9745APZF_SOUND);
    finishAnalogObservation(sampler_photo_light, OM_SENSOR_PHOTO_LIGHT);
}

// needs 2s between readings
// however, the actual readData operation takes only 0.03ms on Arduino Nano
void sampleDHT22()
{
    DHT22_ERROR_t errorCode;

    beginSample();
    unsigned long now = millis();
    errorCode = dht22.readData();
    if (DHT_ERROR_NONE == errorCode) {
        sampler_rht03_humid.addMeasurement(dht22.getHumidity() / 100.0, now);
        sampler_rht03_temp.addMeasurement(dht22.getTemperatureC(), now);
    }  
    endSample();
  
  beginOSCWrite();
  
  switch(errorCode)
  {
    case DHT_ERROR_NONE:
        finishAnalogObservation(sampler_rht03_humid, OM_SENSOR_RHT03_HUMID);
        finishAnalogObservation(sampler_rht03_temp, OM_SENSOR_RHT03_TEMP);
        break;
    case DHT_ERROR_CHECKSUM:
      Serial.print(OM_SENSOR_RHT03_ERROR);
      Serial.println(" checksum-error");
      break;
    case DHT_BUS_HUNG:
      Serial.print(OM_SENSOR_RHT03_ERROR);
      Serial.println(" bus-hung");
      break;
    case DHT_ERROR_NOT_PRESENT:
      Serial.print(OM_SENSOR_RHT03_ERROR);
      Serial.println(" not-present");
      break;
    case DHT_ERROR_ACK_TOO_LONG:
      Serial.print(OM_SENSOR_RHT03_ERROR);
      Serial.println(" ack-timeout");
      break;
    case DHT_ERROR_SYNC_TIMEOUT:
      Serial.print(OM_SENSOR_RHT03_ERROR);
      Serial.println(" sync-timeout");
      break;
    case DHT_ERROR_DATA_TIMEOUT:
      Serial.print(OM_SENSOR_RHT03_ERROR);
      Serial.println(" data-timeout");
      break;
    case DHT_ERROR_TOOQUICK:
      Serial.print(OM_SENSOR_RHT03_ERROR);
      Serial.println(" polled-too-quick");
      break;
  } 
 
    endOSCWrite(); 
}

void sampleBMP085()
{
    // this has been found to take around 12ms (on Arduino Nano)
    beginSample();
    unsigned long now = millis();
    bmp085.sample();
    sampler_bmp085_press.addMeasurement(bmp085.getLastPressure(), now);
    sampler_bmp085_temp.addMeasurement(bmp085.getLastTemperature() / 10.0, now);
    endSample();
    
    beginOSCWrite();
    finishAnalogObservation(sampler_bmp085_press, OM_SENSOR_BMP085_PRESSURE);
    finishAnalogObservation(sampler_bmp085_temp, OM_SENSOR_BMP085_TEMP);
    endOSCWrite();
}

void rateTest()
{
  long before = millis();
  long unsigned int n = 100000;
  for (long unsigned int i = 0; i < n; i++)
  {
      dht22.readData();
  }
  long duration = millis() - before;
  
  //beginOSCWrite();
  Serial.print(n);
  Serial.print(" iterations in ");
  Serial.print(duration);
  Serial.println(" ms");
  //endOSCWrite();
}
//*/
