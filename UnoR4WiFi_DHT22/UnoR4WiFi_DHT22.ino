/** Main Arduino Sketch.
 *
 * This file is a part of: Arduino Uno R4 WiFi with DHT22 Sensor, https://github.com/haukex/unor4wifi_dht22
 *
 * This work © 2023 by Hauke Dämpfling (haukex@zero-g.net) is licensed under Attribution-ShareAlike 4.0
 * International. To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/4.0/
 */

/* ********** ********** ********** Globals ********** ********** ********** */

const String VERSION = String("$Id$").substring(5,13);

//TODO Later: I should check that the places in this code that use millis() won't blow up on its overflow.
//TODO: The WiFi connection seems to randomly crash after a few days, requiring a full power cycle.

#define delayMilliseconds(x)   delay(x)

void halt(const char* message) {
  Serial.print(F("HALT "));
  Serial.println(message);
  // keep reporting the message in case we connect via USB to see what's wrong
  unsigned long next_msg_ms = millis();
  while (true) {
    while ( millis() < next_msg_ms );
    Serial.print(F("HALTED because "));
    Serial.println(message);
    next_msg_ms += 10000;
  }
}

#define SERIAL_TIMEOUT_MS (30000)

/* ********** ********** ********** LED Matrix ********** ********** ********** */
// https://github.com/arduino-libraries/ArduinoGraphics
// can ignore message "WARNING: library ArduinoGraphics claims to run on samd architecture(s)
// and may be incompatible with your current board which runs on renesas_uno architecture(s)."
#include "ArduinoGraphics.h"
// https://github.com/arduino/ArduinoCore-renesas/tree/main/libraries/Arduino_LED_Matrix
#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;

void matrix_text(const char *text, size_t scroll) {
  bool do_scroll = scroll>0;
  if (!do_scroll) scroll=1;
  while (scroll--) {
    matrix.beginDraw();
    matrix.stroke(0xFFFFFFFF);
    if (do_scroll) matrix.textScrollSpeed(40);
    matrix.textFont(Font_5x7);
    matrix.beginText(1, 1, 0xFFFFFF);
    matrix.print(text);
    if (do_scroll) matrix.endText(SCROLL_LEFT);
    else matrix.endText();
    matrix.endDraw();
  }
}

/* ********** ********** ********** DHT22 ********** ********** ********** */
// https://github.com/adafruit/DHT-sensor-library
// requires https://github.com/adafruit/Adafruit_Sensor
#include "DHT.h"

// note this interval isn't necessarily how often the sensor is read out, see `sens_maybe_update()` for details
#define SENS_UPD_INTERVAL_MS (1000)

DHT dht(A0, DHT22);

unsigned long sens_update_at_millis;
uint8_t sens_state;
unsigned long sens_when_updated_millis;
float sens_temperature_c;
float sens_humidity_per;
float sens_heat_index_c;

void init_sens() {
  dht.begin();
  sens_state = 0;
  sens_update_at_millis = millis() - 1;  // force update
  sens_maybe_update();
}

void sens_maybe_update() {
  static char sens_text_temp[3];
  static char sens_text_humid[3];
  if ( millis() < sens_update_at_millis ) return;

  if ( sens_state==0 ) {
    strncpy(sens_text_temp,  "? ", 3);
    strncpy(sens_text_humid, " ?", 3);
    // Reading temperature or humidity takes about 250ms
    // Sensor readings may also be up to 2s old (slow sensor)
    sens_temperature_c = dht.readTemperature();  // Celsius (default)
    sens_humidity_per = dht.readHumidity();
    if ( isnan(sens_humidity_per) || isnan(sens_temperature_c) )
      Serial.println(F("ERROR: Failed to read from DHT22 sensor!"));
    else {
      sens_when_updated_millis = millis();
      // Compute heat index in Celsius (isFahreheit = false)
      sens_heat_index_c = dht.computeHeatIndex(sens_temperature_c, sens_humidity_per, false);
      Serial.print(F("DHT22 Temperature: "));
      Serial.print(sens_temperature_c);
      Serial.print(F("°C,  Humidity: "));
      Serial.print(sens_humidity_per);
      Serial.print(F("%,  Heat index: "));
      Serial.print(sens_heat_index_c);
      Serial.println(F("°C"));
      snprintf(sens_text_temp,  3, "%2d", (int)sens_temperature_c);
      snprintf(sens_text_humid, 3, "%2d", (int)sens_humidity_per);
    }
    matrix_text(sens_text_temp, 0);
    sens_state = 1;
  }
  else if ( sens_state==1 ) {
    matrix_text(sens_text_humid, 0);
    sens_state = 0;
  }
  else sens_state = 0; // shouldn't happen

  sens_update_at_millis = millis() + SENS_UPD_INTERVAL_MS;
}

/* ********** ********** ********** EEPROM ********** ********** ********** */
#include <EEPROM.h>
#include "DataFlashBlockDevice.h"

// Sadly, the Arduino EEPROM library doesn't expose this functionality, so I've had to roll my own.
// https://github.com/arduino/ArduinoCore-renesas/blob/9a838ba4/libraries/EEPROM/src/EEPROM.h#L49
// https://github.com/arduino/ArduinoCore-renesas/blob/9a838ba4/libraries/BlockDevices/virtualEEPROM.cpp#L90
// https://github.com/arduino/ArduinoCore-renesas/blob/9a838ba4/libraries/BlockDevices/DataFlashBlockDevice.cpp#L308
// https://github.com/arduino/ArduinoCore-renesas/blob/9a838ba4/variants/UNOWIFIR4/pins_arduino.h#L167
void erase_virtual_eeprom() {
  DataFlashBlockDevice& dfbd = DataFlashBlockDevice::getInstance();
  const bd_size_t total_size = dfbd.size();  // should be 8192
  const bd_size_t erase_size = dfbd.get_erase_size();  // should be 1024
  for (bd_addr_t addr = 0; addr < total_size; addr += erase_size) {
    Serial.print(F("vEE: Erasing "));
    Serial.print(erase_size);
    Serial.print(F(" bytes at addr 0x"));
    Serial.print(addr, HEX);
    int rv = dfbd.erase(addr, erase_size);
    if (rv) {
      Serial.print(F(" - ERROR "));
      Serial.println(rv);
      break;
    } else Serial.println(F(" - OK"));
  }
}

#define WIFI_EEFLAG_APMODE (0x01)
String wifi_ssid = "";
String wifi_pass = "";
bool wifi_ap_mode = false;

void read_wifi_auth() {
  wifi_ssid = "";
  wifi_pass = "";
  wifi_ap_mode = false;
  if (EEPROM.read(0)==255) return;  // "Locations that have never been written to have the value of 255."
  size_t eeptr = 0;
  while (true) {
    uint8_t c = EEPROM.read(eeptr++);
    if (c) wifi_ssid += (char)c;
    else break;
  }
  while (true) {
    uint8_t c = EEPROM.read(eeptr++);
    if (c) wifi_pass += (char)c;
    else break;
  }
  uint8_t flags = EEPROM.read(eeptr++);
  wifi_ap_mode = flags & WIFI_EEFLAG_APMODE;
  Serial.print(F("Read "));
  Serial.print(eeptr, DEC);
  Serial.println(F(" bytes from EEPROM."));
}

void write_wifi_auth() {
  if ( ( wifi_ssid.length()+1 + wifi_pass.length()+1 +1 ) > EEPROM.length() )
    halt("Internal error: SSID/Pass too long"); // the code in prompt_save_wifi_auth should prevent this!

  // for security, erase any previous info that may have been stored.
  erase_virtual_eeprom();
  size_t eeptr = 0;
  for(size_t i = 0; i < wifi_ssid.length(); i++)
    EEPROM.write(eeptr++, wifi_ssid.charAt(i));
  EEPROM.write(eeptr++, 0);
  for(size_t i = 0; i < wifi_pass.length(); i++)
    EEPROM.write(eeptr++, wifi_pass.charAt(i));
  EEPROM.write(eeptr++, 0);
  EEPROM.write(eeptr++, wifi_ap_mode ? WIFI_EEFLAG_APMODE : 0);

  Serial.print(F("Success, saved "));
  Serial.print(eeptr, DEC);
  Serial.println(F(" bytes to EEPROM."));
}

/* ********** ********** ********** WiFi ********** ********** ********** */
#include <WiFiS3.h>  // currently no official documentation for this, instead
// https://www.arduino.cc/reference/en/libraries/wifinina/ should be similar; source:
// https://github.com/arduino/ArduinoCore-renesas/tree/main/libraries/WiFiS3/src

WiFiServer netserv(80);
#define WIFI_HOSTNAME ("dht22sens")
const unsigned long WIFI_CONN_TIMEOUT_MS = 30000;
const unsigned long WIFI_READ_TIMEOUT_MS = 5000;
const unsigned long WIFI_CHECK_INTERV_MS = 30000;

unsigned long wifi_check_at_millis;

bool prompt_save_wifi_auth() {  // returns true if the user entered new info
  String resp;

  if (wifi_ssid.length()) {
    Serial.print(F("I currently have WiFi authentication for SSID \""));
    Serial.print(wifi_ssid);
    Serial.println(F("\" saved."));
    if (wifi_ap_mode) {
      Serial.print(F("AP mode - password is \""));
      Serial.print(wifi_pass);
      Serial.println("\"");
    }
    Serial.setTimeout(5000);
    Serial.println(F("Would you like to enter a new SSID and password? [yN, 5s]"));
    resp = Serial.readStringUntil('\n');
    Serial.setTimeout(SERIAL_TIMEOUT_MS);
    if ( !( resp.equals("y") || resp.equals("y\r") ) ) return false;
  }
  else Serial.println(F("I currently have no WiFi authentication info saved."));

  Serial.setTimeout(10000);
  Serial.println(F("Scan for networks? [yN, 10s]"));
  resp = Serial.readStringUntil('\n');
  Serial.setTimeout(SERIAL_TIMEOUT_MS);
  if ( resp.equals("y") || resp.equals("y\r") ) {
    Serial.println(F("Scanning, please wait..."));
    int numSsid = WiFi.scanNetworks();
    if (numSsid<0)
      Serial.println(F("Couldn't get a WiFi connection"));
    else {
      Serial.print(F("Found "));
      Serial.print(numSsid, DEC);
      Serial.println(F(" networks"));
      for (int i = 0; i < numSsid; i++) {
        Serial.print(i+1);
        Serial.print(F(") "));
        Serial.print(WiFi.SSID(i));
        Serial.print(F(" ("));
        Serial.print(WiFi.RSSI(i));
        Serial.println(F(" dBm)"));
      }
    }
  }

  bool new_ap_mode;
  String new_ssid, new_pass;
  Serial.setTimeout(60000);
  while (true) {

    Serial.println(F("Would you like to enable AP mode? [yN, 60s]:"));
    resp = Serial.readStringUntil('\n');
    new_ap_mode = resp.equals("y") || resp.equals("y\r");

    Serial.println(F("Please enter the new WiFi SSID [60s]:"));
    new_ssid = Serial.readStringUntil('\n');
    if ( !new_ssid.length() ) continue;
    if (new_ssid.endsWith("\r")) new_ssid.remove(new_ssid.length()-1);

    Serial.println(F("Please enter the WiFi Password [60s]:"));
    new_pass = Serial.readStringUntil('\n');
    if ( !new_pass.length() ) continue;
    if (new_pass.endsWith("\r")) new_pass.remove(new_pass.length()-1);

    if ( ( new_ssid.length()+1 + new_pass.length()+1 +1 ) > EEPROM.length() ) {
      Serial.print(F("Sorry, length of SSID+Password combined must be less than "));
      Serial.print(EEPROM.length()-3, DEC);
      Serial.println(F(" bytes, I won't be able to connect to this network."));
      continue;
    }

    if (new_ap_mode) Serial.print(F("AP Mode - "));
    Serial.print(F("SSID \""));
    Serial.print(new_ssid);
    Serial.print(F("\" Password \""));
    Serial.print(new_pass);
    Serial.println(F("\""));
    Serial.println(F("Are you sure this is correct? [yN, 60s]"));
    resp = Serial.readStringUntil('\n');
    if (resp.equals("y") || resp.equals("y\r")) break;

  }
  Serial.setTimeout(SERIAL_TIMEOUT_MS);

  wifi_ap_mode = new_ap_mode;
  wifi_ssid = new_ssid;
  wifi_pass = new_pass;
  write_wifi_auth();
  return true;
}

void print_wifi_mac(Print& out) {
  byte mac[6];
  WiFi.macAddress(mac);
  out.print(F("WiFi MAC: "));
  for (int i = 5; i >= 0; i--) {
    out.print(mac[i], HEX);
    if (i > 0) out.print(F(":"));
  }
  out.println();
}

void print_wifi_status(Print& out, bool try_restart) {
  if ( WiFi.firmwareVersion() < WIFI_FIRMWARE_LATEST_VERSION ) {
    out.println(F("WARNING: WiFi Firmware Upgrade required!"));
    // This is done via the sketch that can be found in the library examples in the Arduino IDE.
  }
  const int wifi_status = WiFi.status();
  const bool wifi_good = wifi_ap_mode
    ? ( wifi_status==WL_AP_LISTENING || wifi_status==WL_AP_CONNECTED )
    : ( wifi_status==WL_CONNECTED );
  if ( wifi_good ) {
    out.print( wifi_ap_mode ? F("Serving a WiFi Access Point \"") : F("WiFi connected to ") );
    out.print(WiFi.SSID());
    if (wifi_ap_mode) out.print("\"");
    if (wifi_ap_mode) {
      if (wifi_status==WL_AP_CONNECTED)
        // Note the module still appears to report "connected" even after all clients have disconnected from the AP...
        out.print(F(", Connected"));
      out.print(F(", Password=\""));
      out.print(wifi_pass);
      out.print("\"");
    }
    else {
      out.print(F(", RSSI="));
      out.print(WiFi.RSSI());
      out.print(F(" dBm"));
    }
    out.print(F(", Address http://"));
    out.println(WiFi.localIP());
  }
  else {
    out.println( wifi_ap_mode ? F("NOT listening as WiFi AP") : F("NOT connected to WiFi") );
    if (try_restart) {
      out.println(F("Reinitializing WiFi..."));
      init_wifi();
    }
  }
}

void init_wifi() {
  Serial.println("Starting WiFi...");
  if ( WiFi.status() == WL_NO_MODULE )
    halt("ERROR: Communication with WiFi module failed!");
  WiFi.end();
  print_wifi_mac(Serial);
  read_wifi_auth();
  while (!wifi_ssid.length()) prompt_save_wifi_auth();
  WiFi.setHostname(WIFI_HOSTNAME);
  int status = WL_IDLE_STATUS;
  if (wifi_ap_mode) {
    Serial.print(F("Creating Access Point named: "));
    Serial.println(wifi_ssid);
    WiFi.config(IPAddress(192,168,40,1));
    int attempts = 3;
    while (true) {
      // Happily, the WiFi module also serves DHCP.
      status = WiFi.beginAP(wifi_ssid.c_str(), wifi_pass.c_str());
      if ( status != WL_AP_LISTENING ) {
        if (--attempts<1)
          halt("Creating Access Point failed");
        else {
          Serial.println(F("Creating Access Point failed, retrying in 5s"));
          delayMilliseconds(5000);
        }
      }
      else break;
    }
  }
  else {
    unsigned long timeout_at_millis = millis() + WIFI_CONN_TIMEOUT_MS;
    while (status != WL_CONNECTED) {
      Serial.print(F("Attempting to connect to network "));
      Serial.println(wifi_ssid);
      status = WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
      for (int i=0;i<10;i++) {
        status = WiFi.status();
        if ( status==WL_CONNECTED ) break;
        else if ( millis() >= timeout_at_millis ) {
          Serial.print(F("Timed out connecting to network "));
          Serial.println(wifi_ssid);
          prompt_save_wifi_auth();
          timeout_at_millis = millis() + WIFI_CONN_TIMEOUT_MS;
        }
      }
    }
  }
  netserv.begin();
  print_wifi_status(Serial, false);
  String scrollip = "  " + WiFi.localIP().toString() + "   ";
  matrix_text(scrollip.c_str(), 2);  // this is mostly just so I can hit the reset button and see the IP...
  wifi_check_at_millis = millis() + WIFI_CHECK_INTERV_MS;
}

void wifi_maybe_check() {
  if ( millis() < wifi_check_at_millis ) return;
  print_wifi_status(Serial, true);
  wifi_check_at_millis = millis() + WIFI_CHECK_INTERV_MS;
}

/* ********** ********** ********** Main Code ********** ********** ********** */

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  //while (!Serial);  // no, don't wait for USB-Serial connection to boot
  delayMilliseconds(3000);  // instead, just wait a few seconds
  Serial.setTimeout(SERIAL_TIMEOUT_MS);
  Serial.println(F("==========> Booting..."));
  Serial.print(F("Firmware version: "));
  Serial.println(VERSION);
  matrix.begin();
  init_sens();
  init_wifi();
  Serial.println(F("========== ==========> Ready! <========== =========="));
  digitalWrite(LED_BUILTIN, LOW);
}

#define HTTP_MAX_LINE_LENGTH (256)
#define HTTP_BUFFER_SIZE (1024)
char http_buffer[HTTP_BUFFER_SIZE];  // for receiving post bodies and generating response bodies
#include "write_html.h"
#include "favicon.h"

void loop() {
  sens_maybe_update();
  wifi_maybe_check();
  //TODO Later: Multiple clients still seems to be a problem.
  WiFiClient client = netserv.available();  // non-blocking
  if (client) {
    if ( client.connected() && client.available() ) {
      digitalWrite(LED_BUILTIN, HIGH);
      // our code here blocks, so let's hope the client sends everything quickly, because I don't feel like writing code to buffer data per client
      client.setTimeout(WIFI_READ_TIMEOUT_MS);
      IPAddress ip = client.remoteIP();
      bool got_req_line = false;
      bool method_post = false;
      long content_length = -1;
      String error = F("400 Bad Request");
      memset(http_buffer, 0, HTTP_BUFFER_SIZE);
      String uri;
      while (true) {
        if (!client.connected()) break;

        // This is a replacement for readStringUntil, so that we can impose a max line length.
        String line;
        bool read_ok = false;
        unsigned long timeout_at_millis = millis() + WIFI_READ_TIMEOUT_MS;
        while ( millis() < timeout_at_millis ) {
          int c = client.read();
          if ( c<0 ) break;
          else if (c=='\r');  // let's just ignore this
          else if (c=='\n') { read_ok=true; break; }
          else if (c<0x20) break;  // control characters
          else if ( line.length() >= HTTP_MAX_LINE_LENGTH ) break;
          else line += (char)c;
        }
        if (!read_ok) break;

        // Handle request line
        if ( !got_req_line ) {
          if ( !( line.endsWith(" HTTP/1.0") || line.endsWith(" HTTP/1.1") ) )
            // we don't support HTTP/0.9 Simple-Request (see https://www.w3.org/Protocols/HTTP/1.0/spec.html#Request)
            break;
          if ( line.startsWith("GET ") )
            uri = line.substring(4, line.length()-9);
          else if ( line.startsWith("POST ") ) {
            uri = line.substring(5, line.length()-9);
            method_post = true;
          }
          else { error=F("405 Method Not Allowed"); break; }
          got_req_line = true;
        }
        // Handle headers
        else if (line.length()) {
          line.toLowerCase();
          if (line.startsWith("content-length: "))
            content_length = line.substring(16).toInt();
          else if (line.startsWith("content-type: ")) {
            // for now we only accept binary data in our POST requests
            if ( method_post && !line.equals("content-type: application/octet-stream") )
              break;
          }
          // else some other header, ignore
        }
        // blank line = end of headers
        else {
          Serial.print(ip);
          Serial.print(F(" >RX> "));
          Serial.print( method_post ? F("POST ") : F("GET ") );
          Serial.print(uri);
          if (content_length>=0) {
            Serial.print(F(" CL="));
            Serial.println(content_length, DEC);
          } else Serial.println();
          if (method_post) {
            if ( content_length<0 || content_length>HTTP_BUFFER_SIZE ) break;
            if ( client.readBytes(http_buffer, content_length) != content_length ) break;
          }
          else if ( content_length>0 ) break;
          // done receiving request!
          if ( uri.equals("/") ) {
            if (!method_post) {
              client.println(F("HTTP/1.1 200 OK"));
              client.println(F("Connection: keep-alive"));
              client.println(F("Content-Type: text/html; charset=utf-8"));
              client.print(F("Content-Length: "));
              client.println(WRITE_HTML_SIZE, DEC);
              client.println();
              write_html(client);
              Serial.print(ip);
              Serial.println(F(" <TX< 200 OK write_html"));
              error="";
            }
            else error=F("405 Method Not Allowed");
          }
          else if ( uri.equals("/favicon.ico") ) {
            if (!method_post) {
              client.println(F("HTTP/1.1 200 OK"));
              client.println(F("Connection: keep-alive"));
              client.println(F("Content-Type: image/x-icon"));
              client.print(F("Content-Length: "));
              client.println(FAVICON_SIZE, DEC);
              client.println();
              write_favicon(client);
              Serial.print(ip);
              Serial.println(F(" <TX< 200 OK favicon"));
              error="";
            }
            else error=F("405 Method Not Allowed");
          }
          else if ( uri.equals("/sensor.json") ) {
            if (!method_post) {
              int bytes =
                snprintf(http_buffer, HTTP_BUFFER_SIZE, "{\"temp_c\":%f,\"humid_p\":%f,\"heatidx_c\":%f,\"age_ms\":%d}",
                sens_temperature_c, sens_humidity_per, sens_heat_index_c, millis()-sens_when_updated_millis);
              if ( bytes>0 && bytes<HTTP_BUFFER_SIZE ) {
                client.println(F("HTTP/1.1 200 OK"));
                client.println(F("Connection: keep-alive"));
                client.println(F("Access-Control-Allow-Origin: *"));
                client.println(F("Access-Control-Allow-Headers: *"));
                client.println(F("Access-Control-Allow-Methods: GET"));
                client.println(F("Cache-Control: no-store, no-cache, max-age=0, must-revalidate, proxy-revalidate"));
                client.println(F("Content-Type: application/json; charset=ascii"));
                client.print(F("Content-Length: "));
                client.println(bytes, DEC);
                client.println();
                client.print(http_buffer);
                Serial.print(ip);
                Serial.print(F(" <TX< 200 OK "));
                Serial.println(http_buffer);
                error="";
              }
              else error=F("500 Internal Server Error");  // could happen if http_buffer is too small
            }
            else error=F("405 Method Not Allowed");
          }
          else if ( uri.equals("/testpost") ) {  // This is just a demo / test of POST requests!
            if (method_post) {
              // Note since this is just a test, even though the client is sending us binary data,
              // we're just going to assume it's ASCII and echo it back as such.
              client.println(F("HTTP/1.1 200 OK"));
              client.println(F("Connection: keep-alive"));
              client.println(F("Content-Type: text/plain; charset=ascii"));
              client.print(F("Content-Length: "));
              client.println( content_length + 13 );
              client.println();
              client.print(F("You sent me: "));
              client.print(http_buffer);
              Serial.print(ip);
              Serial.print(F(" <TX< 200 OK testpost body=<<"));
              Serial.print(http_buffer);
              Serial.println(F(">>"));
              error="";
            }
            else error=F("405 Method Not Allowed");
          }
          else error=F("404 Not Found");
          client.flush();
          break;
        }
      } // end of readline while
      if ( error.length() && client.connected() ) {
        // successful responses clear out the error variable above, so if we get here,
        // we broke out of the loop due to an error (but we don't need to send if not connected)
        client.print(F("HTTP/1.1 "));
        client.println(error);
        client.println(F("Connection: close"));
        client.println(F("Content-Length: 0"));
        client.println();
        Serial.print(ip);
        Serial.print(F(" <TX< "));
        Serial.println(error);
        client.stop();  // not closing connection for non-error responses due to "Connection: keep-alive" above
      }
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  if ( Serial.available()>6 ) {
    Serial.setTimeout(100);  // assume the entire command was already recieved, and don't block WiFi comms
    String line = Serial.readStringUntil('\n');
    if (line.endsWith("\r")) line.remove(line.length()-1);
    Serial.setTimeout(SERIAL_TIMEOUT_MS);
    if ( line.equals("STATUS") ) {
      print_wifi_status(Serial, false);
    }
    else if ( line.equals("RECONF") ) {
      if ( prompt_save_wifi_auth() ) {
        Serial.println(F("Reinitializing WiFi..."));
        init_wifi();
      }
    }
    else {
      Serial.println(F("Unrecognized command. Available commands:"));
      Serial.println(F("STATUS - show current status"));
      Serial.println(F("RECONF - reconfigure WiFi"));
    }
  }
}
