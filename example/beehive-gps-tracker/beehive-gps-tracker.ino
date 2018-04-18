#include <Sim800l.h>
#include <SoftwareSerial.h>
#include <TimerOne.h>
#include <TinyGPS.h>
#include <LowPower.h>

#define GPS_RETRY 180
#define NUMBER    "+33612345678"
#define DEBUG     false

volatile bool   shift, periodic = false;

Sim800l         gsm;
TinyGPS         gps;
String          pos;
String          last_pos;
SoftwareSerial  nss(4, 5);

void setup() {
#if DEBUG
  Serial.begin(9600);
#endif
  
  pinMode(2, INPUT);  // shift interruption

  pinMode(7, OUTPUT); // VCC of shift
  digitalWrite(7, HIGH);

  pinMode(8, OUTPUT); // GND of shift
  digitalWrite(8, LOW);

  Timer1.initialize(5000000); // interruption every 5 seconds
  Timer1.attachInterrupt(periodicHandler);
  
  attachInterrupt(0, shiftHandler, CHANGE); // interrupt on pin 2

  delay(1000);
}

void shiftHandler() {
  if (shift == false) {
#if DEBUG
    Serial.println("shift handler");
#endif
    shift = true;
  }
}

void periodicHandler() {
  static volatile unsigned int count = 0;
  if (periodic == false) {
#if DEBUG
    Serial.println("periodic handler");
#endif
    count++;
    if (count == 60) { // Every 5 minutes
      periodic = true;
      count = 0;
    }
  }
}

bool getPos() {      
    float lat, lon = 0;
    unsigned long fixAge = 0;
    bool gpsAvailable = false;

    nss.begin(9600);
    for (int i = 0; i < GPS_RETRY; i++) {
#if DEBUG
      Serial.print("check gps availability "); Serial.print(i); Serial.print("/"); Serial.println(GPS_RETRY);
#endif
      while (nss.available()) {
        char c = nss.read();
#if DEBUG
        Serial.print(c);
#endif
        if (gps.encode(c)) {
          gpsAvailable = true;
          
          gps.f_get_position(&lat, &lon, &fixAge);
          break;
        }
      }

      #if DEBUG
      Serial.println("");
      #endif
      
      if (gpsAvailable) {
#if DEBUG
        Serial.println("gps is available");
#endif
        break;
      }
#if DEBUG
      else {
        Serial.println("gps is not available");
      }
#endif  
      delay(1000);    
    }

    if (gpsAvailable) {
      pos = "http://maps.google.com/maps?q=" + String(lat, 5) + "," + String(lon, 5);
      last_pos = pos;
      return true;
    }
    pos = last_pos;
    return false;
}

void sendMsg(String msg) {
#if DEBUG
  Serial.println(msg);
#endif
  
  gsm.begin(10, 11, 9);
  delay(1000);
  gsm.sendSms(NUMBER, msg.c_str());

#if DEBUG
  Serial.println("sms sent");
#endif
}

void loop() { 
#if DEBUG
  Serial.println("loop");
#endif
  
  if (shift) {
#if DEBUG
    Serial.println("shift action");
#endif
    
    if (getPos())
      sendMsg("LA RUCHE A BOUGE : " + pos);
    else
      sendMsg("LA RUCHE A BOUGE, ANCIENNE POSITION : " + pos);
    shift = false;
  }

  if (periodic) {
#if DEBUG
    Serial.println("periodic action");
#endif

    String msg = gsm.readSms(1);
    
    if (msg.indexOf("OK") != -1) {
      String sender = gsm.getNumberSms(1);
      
#if DEBUG
      Serial.println("a sms is received from:" + sender);
#endif
      if (sender.indexOf(NUMBER) != -1 && msg.indexOf("Ruche") != -1) {
        if (getPos())
          sendMsg(pos);
        else
          sendMsg("ANCIENNE POSITION : " + pos);
#if DEBUG
        Serial.println("invalid sms. skip action");
#endif
      }
      gsm.delAllSms();
    }
    
    periodic = false;
  }

  delay(5000);
}
