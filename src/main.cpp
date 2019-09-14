#include <Arduino.h>

#include "SoftwareSerial.h"

// needed because ESP8266 has only one hardware serial port
#define USE_SOFTWARE_SERIAL

#define GPS_RX_PIN D7
#define RESET_PIN D6
#define RTK_BASE_MODULE_RX_PIN D5

#include "sensesp_app.h"
#include "wiring_helpers.h"

unsigned long last_base_rx = 0;

void reset_gps(int reset_pin) {
  // keep the reset pin pulled low for 100 ms
  app.onDelay(100, [reset_pin](){
    pinMode(reset_pin, OUTPUT);
    digitalWrite(reset_pin, HIGH);
    pinMode(reset_pin, INPUT);
  });
}

// Watchdog for ensuring that the RTK base receiver
// is outputting some data on the serial pin
void setup_base_watchdog(int rx_pin, int32_t bitrate, int reset_pin) {
  last_base_rx = millis();
  SoftwareSerial* base_rx_stream = 
    new SoftwareSerial(rx_pin, SW_SERIAL_UNUSED_PIN);
  app.onAvailable(*base_rx_stream, [base_rx_stream](){
    last_base_rx = millis();
  });
  base_rx_stream->begin(bitrate);
  app.onRepeat(1000, [reset_pin](){
    unsigned long cur_time = millis();
    if (cur_time - last_base_rx > 5000) {
      // nothing from base receiver in five seconds
      debugW("No input from base receiver, resetting GPS modules");
      reset_gps(reset_pin);
      last_base_rx = millis();
    }
  });
}

ReactESP app([] () {
  // Some initialization boilerplate when in debug mode...
  #ifdef USE_SOFTWARE_SERIAL
  Serial.begin(115200);
  Serial.available();

  // A small arbitrary delay is required to let the
  // serial port catch up
  delay(100);
  Debug.setSerialEnabled(true);
  #endif

  sensesp_app = new SensESPApp();

  Stream* rx_stream;

  #ifdef USE_SOFTWARE_SERIAL
  SoftwareSerial* swSerial = 
    new SoftwareSerial(GPS_RX_PIN, SW_SERIAL_UNUSED_PIN);
  swSerial->begin(38400, SWSERIAL_8N1);
  rx_stream = swSerial;
  #else
  Serial.begin(38400);
  // This moves RX to pin 13 (D7)
  Serial.swap();
  rx_stream = &Serial;
  #endif

  reset_gps(RESET_PIN);
  setup_base_watchdog(RTK_BASE_MODULE_RX_PIN, 38400, RESET_PIN);
  setup_gps(rx_stream);


  sensesp_app->enable();
});
