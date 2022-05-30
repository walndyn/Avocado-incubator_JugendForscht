#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SD.h>
#include <DHT.h>
#include <DS3231.h>
#include <Wire.h>

DS3231 Clock;

DHT dht1(4, DHT22);
DHT dht2(7, DHT22);

#define encoderPinA 2
#define encoderPinB 3
#define chipSelect 53
#define durtime 7060

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

boolean A_set, B_set, Century, sw, lastsw  = false;
boolean sd = true;
boolean sdlog = true;
boolean h12;
boolean PM;

volatile unsigned int encoderPos = 1;  // a counter for the dial
unsigned int lastReportedPos = 2;   // change management
static boolean rotating = false;    // debounce management

float h, h2, t, t2 = 0;
int averageh, averaget, soil1, soil2, soil3, soil4, averagesoil, rbl, wl, heat, inth, inth2, intt, intt2, ontime, lastontime, on, offtime, lastofftime, off,  firstcyl, rblauto, wlauto, heatauto, rblcnt, wlcnt = 0;
int maxlight = 100;
int lastmaxlight = 99;
int HOn = 8;
int HOff = 20;
int temp = 25;
int lightpwm = 255;
int menu = 1;

String capString = "";
String DHTString = "";
String DHT2String = "";
String RTCString = "";
String RBLString = "";
String WLString = "";
String HeatString = "";
String RBLStringauto = "";
String WLStringauto = "";
String HeatStringauto = "";
String TimeString = "";
String Ontime = "08:00";
String Offtime = "20:00";
String Maxlight = "100%";
String Temp = "25";
String SDLog = "on";

unsigned long startMillis, currentMillis, startMillis2, currentMillis2, startMillis3, currentMillis3, startMillis4, currentMillis4, startMillis5, currentMillis5;



void symbol(int x, int y, int n) {
  u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
  u8g2.drawGlyph(x, y, n);
}

void drawStatBar() {
  u8g2.setFont(u8g2_font_t0_11_tf);
  u8g2.setCursor(0, 8);
  u8g2.print(averaget);
  u8g2.drawUTF8(12, 8, "°C");
  u8g2.setCursor(50, 8);
  u8g2.print(TimeString);
  u8g2.setCursor(108, 8);
  u8g2.print(averageh);
  u8g2.drawUTF8(120, 8, "%");
  u8g2.drawFrame(0, 10, 128, 2);
}
void Switch() {
  currentMillis2 = millis();
  if ((currentMillis2 - startMillis2 >= 250)) {
    sw ^= 1;
    startMillis2 = currentMillis2;
  }

}

// Interrupt on A changing state
void EncoderA() {
  if ( rotating ) delay (1);  // wait a little until the bouncing is done
  if ( digitalRead(encoderPinA) != A_set ) { // debounce once more
    A_set = !A_set;

    // adjust counter + if A leads B
    if ( A_set && !B_set ) {

      if (menu == 4) wl ++;
      else if (menu == 5) rbl++;
      else if (menu == 6) heat++;
      else if (menu == 10) sd ^= 1;
      else if (menu == 11) ontime++;
      else if (menu == 12) offtime++;
      else if (menu == 13) maxlight++;
      else if (menu == 9) temp++;
      else encoderPos ++;
    }
    rotating = false;  // no more debouncing until loop() hits again
    if (menu == 9 && encoderPos == 3) encoderPos = 1;
    if (wl < 0) {
      wl = 0;
    }
    if (wl > 255) {
      wl = 255;
    }
    if (rbl < 0) {
      rbl = 0;
    }
    if (rbl > 255) {
      rbl = 255;
    }
    if (heat < 0) {
      heat = 0;
    }
    if (heat > 255) {
      heat = 255;
    }
    if (encoderPos <= 0) {
      encoderPos = 3;
    }
    if (encoderPos >= 4) {
      encoderPos = 1;
    }
  }
}

// Interrupt on B changing state, same as A above
void EncoderB() {
  if ( rotating ) delay (1);
  if ( digitalRead(encoderPinB) != B_set ) {
    B_set = !B_set;
    //  adjust counter - 1 if B leads A
    if ( B_set && !A_set ) {

      if (menu == 4) wl --;
      else if (menu == 5) rbl--;
      else if (menu == 6) heat--;
      else if (menu == 10) sd ^= 1;
      else if (menu == 11) ontime--;
      else if (menu == 12) offtime--;
      else if (menu == 13) maxlight--;
      else if (menu == 9) temp--;
      else encoderPos --;
    }
    rotating = false;
  }
  if (wl < 0) {
    wl = 0;
  }
  if (wl > 255) {
    wl = 255;
  }
  if (rbl < 0) {
    rbl = 0;
  }
  if (rbl > 255) {
    rbl = 255;
  }
  if (heat < 0) {
    heat = 0;
  }
  if (heat > 255) {
    heat = 255;
  }
  if (encoderPos <= 0) {
    encoderPos = 3;
  }
  if (encoderPos >= 4) {
    encoderPos = 1;
  }
}

// Soil moist sensors
void getSoilSensors() {
  capString = "";
  for (int analogPin = 0; analogPin < 4; analogPin++) {
    int sensor = -1 * ((analogRead(analogPin) - 330) / 3.45) + 100;
    capString += String(sensor);
    capString += String("%");
    if (analogPin < 3) {
      capString += ", ";
    }
  }

  soil1 = -1 * ((analogRead(A0) - 330) / 3.45) + 100;
  soil2 = -1 * ((analogRead(A1) - 330) / 3.45) + 100;
  soil3 = -1 * ((analogRead(A2) - 330) / 3.45) + 100;
  soil4 = -1 * ((analogRead(A3) - 330) / 3.45) + 100;
  averagesoil = (soil1 + soil2 + soil3 + soil4) / 4;
}

// RTC Time
void getRTC() {
  RTCString = "";

  // first year
  if(Clock.getYear() < 10){
  RTCString += String('0');
  RTCString += String(Clock.getYear(), DEC);
  RTCString += String('.');
  } 
  else {RTCString += String(Clock.getYear(), DEC);
  RTCString += String('.');}

  // then the month
  if(Clock.getMonth(Century) < 10){
  RTCString += String('0');
  RTCString += String(Clock.getMonth(Century), DEC);
  RTCString += String('.');
  } 
  else {RTCString += String(Clock.getMonth(Century), DEC);
  RTCString += String('.');}

  // then the date
  if(Clock.getDate() < 10){
  RTCString += String('0');
  RTCString += String(Clock.getDate(), DEC);
  RTCString += String(' ');
  } 
  else {RTCString += String(Clock.getDate(), DEC);
  RTCString += String(' ');}

  // Finally the hour, minute, and second
  if(Clock.getHour(h12, PM) < 10){
  RTCString += String('0');
  RTCString += String(Clock.getHour(h12, PM), DEC);
  RTCString += String(':');
  } 
  else {RTCString += String(Clock.getHour(h12, PM), DEC);
  RTCString += String(':');}

  if(Clock.getMinute() < 10){
  RTCString += String('0');
  RTCString += String(Clock.getMinute(), DEC);
  RTCString += String(':');
  } 
  else {RTCString += String(Clock.getMinute(), DEC);
  RTCString += String(':');}

  if(Clock.getSecond() < 10){
  RTCString += String('0');
  RTCString += String(Clock.getSecond(), DEC);
  RTCString += String(':');
  } 
  else RTCString += String(Clock.getSecond(), DEC);

}

void getTime() {
  TimeString = "";

  if ((Clock.getHour(h12, PM)) < 10) {
    TimeString += String("0");
    TimeString += String(Clock.getHour(h12, PM), DEC);
  }
  else TimeString += String(Clock.getHour(h12, PM), DEC);

  TimeString += String(':');

  if ((Clock.getMinute()) < 10) {
    TimeString += String("0");
    TimeString += String(Clock.getMinute(), DEC);
  }
  else TimeString += String(Clock.getMinute(), DEC);

}

// Temprature & Humidity
void getDHT() {
  DHTString = "";
  DHT2String = "";

  // read Data
  h = dht1.readHumidity();
  t = dht1.readTemperature();
  h2 = dht2.readHumidity();
  t2 = dht2.readTemperature();
  averageh = (h + h2) / 2;
  averaget = (t + t2) / 2;

  inth = h;
  inth2 = h2;
  intt = t;
  intt2 = t2;


  // write to String
  DHTString += String(t);
  DHTString += String("°C");
  DHTString += String(", ");
  DHTString += String(h);
  DHTString += String("%");
  DHT2String += String(t2);
  DHT2String += String("°C");
  DHT2String += String(", ");
  DHT2String += String(h2);
  DHT2String += String("%");
}

// Light red-blue
void getLightrb() {
  int intrbl = (rbl / 2.55);
  RBLString = "";
  RBLString += String(intrbl);
  RBLString += String("%");
}

// Light white
void getLightw() {
  int intwl = (wl / 2.55);
  WLString = "";
  WLString += String(intwl);
  WLString += String("%");
}

// Heating
void getHeat() {
  int intheat = (heat / 2.55);
  HeatString = "";
  HeatString += String(intheat);
  HeatString += String("%");
}

// Light red-blue auto
void getLightrbauto() {
  int intrbl = (rblauto / 2.55);
  RBLStringauto = "";
  RBLStringauto += String(intrbl);
  RBLStringauto += String("%");
}

// Light white auto
void getLightwauto() {
  int intwl = (wlauto / 2.55);
  WLStringauto = "";
  WLStringauto += String(intwl);
  WLStringauto += String("%");
}

// Heating auto
void getHeatauto() {
  int intheat = (heatauto / 2.55);
  HeatStringauto = "";
  HeatStringauto += String(intheat);
  HeatStringauto += String("%");
}

void getOntime() {

  if (lastontime < ontime) {
    on += 15;
  }
  if (lastontime > ontime) {
    on -= 15;
  }
  if (on >= 60) {
    HOn++;
    on = 0;
  }
  if (on < 0) {
    HOn--;
    on = 45;
  }
  if (HOn >= 24) {
    HOn = 0;
  }
  if (HOn < 0) {
    HOn = 23;
  }
  if (lastontime != ontime) {
    lastontime = ontime;
    Ontime = "";
    if (HOn < 10) Ontime += String("0");
    Ontime += String(HOn);
    Ontime += String(":");
    if (on < 10) Ontime += String("0");
    Ontime += String(on);
  }

}

void getOfftime() {

  if (lastofftime < offtime) {
    off += 15;
  }
  if (lastofftime > offtime) {
    off -= 15;
  }
  if (off >= 60) {
    HOff++;
    off = 0;
  }
  if (off < 0) {
    HOff--;
    off = 45;
  }
  if (HOff >= 24) {
    HOff = 0;
  }
  if (HOff < 0) {
    HOff = 23;
  }
  if (lastofftime != offtime) {
    lastofftime = offtime;
    Offtime = "";
    if (HOff < 10) Offtime += String("0");
    Offtime += String(HOff);
    Offtime += String(":");
    if (off < 10) Offtime += String("0");
    Offtime += String(off);
  }

}

void getMaxlight() {

  if (maxlight < 0) {
    maxlight = 0;
  }
  if (maxlight > 100) {
    maxlight = 100;
  }

  if (lastmaxlight != maxlight) {
    lastmaxlight = maxlight;
    lightpwm = maxlight * 2.55;
    Maxlight = "";
    if (maxlight < 10) Maxlight += String("0");
    if (maxlight < 100) Maxlight += String("0");
    Maxlight += String(maxlight);
    Maxlight += String("%");
  }
}

void getTemp() {
  if (temp < 0) temp = 0;
  if (temp > 50) temp = 50;

  Temp = "";
  if (temp < 10) Temp += String("0");
  Temp += String(temp);
}

void homescreen() {

  u8g2.firstPage();
  do {

    drawStatBar();

    switch (menu) {
      case 1:
        if (encoderPos == 1) {
          u8g2.drawFrame(0, 12, 4, 4);
          if (sw != lastsw) {
            menu = 2;
            lastsw = sw;
          }
        }
        if (encoderPos == 2) {
          u8g2.drawFrame(0, 39, 4, 4);
          if (sw != lastsw) {
            menu = 3;
            encoderPos = 1;
            lastsw = sw;
          }
        }
        if (encoderPos == 3) {
          u8g2.drawFrame(65, 39, 4, 4);
          if (sw != lastsw) {
            menu = 7;
            encoderPos = 1;
            lastsw = sw;
          }
        }

        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawUTF8(37, 31, "Liveview Mode");
        u8g2.setFont(u8g2_font_5x8_tf);
        u8g2.drawUTF8(25, 60, "Manual");
        u8g2.drawUTF8(87, 60, "Settings");
        u8g2.drawFrame(0, 10, 129, 2);
        u8g2.drawHLine(0, 38, 128);
        u8g2.drawVLine(64, 38, 54);
        symbol(16, 33, 165);
        symbol(3, 60, 229);
        symbol(68, 61, 129);


        break;

      case 2:
        if (sw != lastsw) {
          menu = 1;
          encoderPos = 1;
          lastsw = sw;
        }

        if (encoderPos == 1) {

          u8g2.setFont(u8g2_font_5x7_tf);
          u8g2.drawUTF8(17, 19, "Temperatur unten:");
          u8g2.setCursor(103, 19);
          u8g2.print(intt);
          u8g2.drawUTF8(113, 19, "°C");
          u8g2.drawUTF8(17, 27, "Temperatur oben:");
          u8g2.setCursor(98, 27);
          u8g2.print(intt2);
          u8g2.drawUTF8(108, 27, "°C");
          u8g2.drawUTF8(0, 36, "Durchschnitt:");
          u8g2.setCursor(66, 36);
          u8g2.print(averaget);
          u8g2.drawUTF8(76, 36, "°C");
          u8g2.drawHLine(0, 37, 128);
          u8g2.drawUTF8(17, 45, "Luftfeuchte unten:");
          u8g2.setCursor(108, 45);
          u8g2.print(inth);
          u8g2.drawUTF8(118, 45, "%");
          u8g2.drawUTF8(17, 53, "Luftfeuchte oben:");
          u8g2.setCursor(103, 53);
          u8g2.print(inth2);
          u8g2.drawUTF8(113, 53, "%");
          u8g2.drawUTF8(0, 62, "Durchschnitt:");
          u8g2.setCursor(66, 62);
          u8g2.print(averageh);
          u8g2.drawUTF8(76, 62, "%");
          symbol(1, 29, 168);
          symbol(0, 55, 152);
        }
        else if (encoderPos == 2) {

          u8g2.setFont(u8g2_font_5x7_tf);
          u8g2.drawUTF8(22, 26, "Bodenfeuchte 1:");
          u8g2.setCursor(99, 26);
          u8g2.print(soil1);
          u8g2.drawUTF8(109, 26, "%");
          u8g2.drawUTF8(22, 34, "Bodenfeuchte 2:");
          u8g2.setCursor(99, 34);
          u8g2.print(soil2);
          u8g2.drawUTF8(109, 34, "%");
          u8g2.drawUTF8(22, 42, "Bodenfeuchte 3:");
          u8g2.setCursor(99, 42);
          u8g2.print(soil3);
          u8g2.drawUTF8(109, 42, "%");
          u8g2.drawUTF8(22, 50, "Bodenfeuchte 4:");
          u8g2.setCursor(99, 50);
          u8g2.print(soil4);
          u8g2.drawUTF8(109, 50, "%");
          u8g2.drawUTF8(0, 64, "Durchschnitt:");
          u8g2.setCursor(66, 64);
          u8g2.print(averagesoil);
          u8g2.drawUTF8(76, 64, "%");
          symbol(0, 43, 152);
        }
        else if (encoderPos == 3) {

          u8g2.setFont(u8g2_font_5x7_tf);
          u8g2.drawUTF8(20, 23, "Licht weiß:");
          u8g2.setCursor(75, 23);
          u8g2.print(WLStringauto);
          u8g2.drawUTF8(20, 31, "Licht rot-blau:");
          u8g2.setCursor(95, 31);
          u8g2.print(RBLStringauto);
          u8g2.drawHLine(0, 37, 128);
          u8g2.drawUTF8(20, 53, "Heizung:");
          u8g2.setCursor(60, 53);
          u8g2.print(HeatStringauto);

          symbol(0, 32, 259);
          symbol(0, 58, 170);

        }

        break;

      case 3:

        if (encoderPos == 1) {

          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(45, 28, "Licht weiß");
          symbol(25, 33, 259);
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.setCursor(60, 50);
          u8g2.print(WLString);

          if (sw != lastsw) {
            menu = 4;
            lastsw = sw;
          }

        }
        else if (encoderPos == 2) {

          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(35, 28, "Licht rot-blau");
          symbol(15, 33, 259);
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.setCursor(60, 50);
          u8g2.print(RBLString);

          if (sw != lastsw) {
            menu = 5;
            lastsw = sw;
          }
        }
        else if (encoderPos == 3) {

          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(49, 28, "Heizung");
          symbol(31, 33, 170);
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.setCursor(60, 50);
          u8g2.print(HeatString);

          if (sw != lastsw) {
            menu = 6;
            lastsw = sw;
          }
        }
        break;

      case 4:

        u8g2.drawFrame(0, 12, 4, 4);

        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawUTF8(45, 28, "Licht weiß");
        symbol(25, 33, 259);
        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.setCursor(60, 50);
        u8g2.print(WLString);



        if (sw != lastsw) {
          menu = 1;
          encoderPos = 2;
          lastsw = sw;
        }

        break;

      case 5:

        u8g2.drawFrame(0, 12, 4, 4);

        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawUTF8(35, 28, "Licht rot-blau");
        symbol(15, 33, 259);
        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.setCursor(60, 50);
        u8g2.print(RBLString);



        if (sw != lastsw) {
          menu = 1;
          lastsw = sw;
        }

        break;

      case 6:

        u8g2.drawFrame(0, 12, 4, 4);

        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawUTF8(49, 28, "Heizung");
        symbol(31, 33, 170);
        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.setCursor(60, 50);
        u8g2.print(HeatString);



        if (sw != lastsw) {
          menu = 1;
          encoderPos = 2;
          lastsw = sw;
        }

        break;

      case 7:

        if (encoderPos == 1) {

          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(38, 28, "Programmwahl:");
          symbol(18, 33, 128);
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(50, 50, "Licht");

          if (sw != lastsw) {
            menu = 8;
            encoderPos = 1;
            lastsw = sw;
          }

        }

        else if (encoderPos == 2) {

          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(38, 28, "Programmwahl:");
          symbol(18, 33, 128);
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(44, 50, "Heizung");

          if (sw != lastsw) {
            menu = 9;
            encoderPos = 1;
            lastsw = sw;
          }

        }

        else if (encoderPos == 3) {

          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(53, 28, "SD-Log");
          u8g2.setFont(u8g2_font_open_iconic_mime_2x_t);
          u8g2.drawGlyph(33, 33, 72);
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.setCursor(59, 50);
          u8g2.print(SDLog);

          if (sw != lastsw) {
            menu = 10;
            encoderPos = 1;
            lastsw = sw;
          }

        }

        break;

      case 8:

        if (encoderPos == 1) {
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(56, 28, "Licht");
          symbol(36, 33, 259);
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(3, 50, "Anschaltzeit:");
          u8g2.setCursor(82, 50);
          u8g2.print(Ontime);

          if (sw != lastsw) {
            menu = 11;
            encoderPos = 1;
            lastsw = sw;
          }

        }
        else if (encoderPos == 2) {
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(56, 28, "Licht");
          symbol(36, 33, 259);
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(3, 50, "Ausschaltzeit:");
          u8g2.setCursor(88, 50);
          u8g2.print(Offtime);

          if (sw != lastsw) {
            menu = 12;
            encoderPos = 1;
            lastsw = sw;
          }

        }
        else if (encoderPos == 3) {
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(56, 28, "Licht");
          symbol(36, 33, 259);
          u8g2.setFont(u8g2_font_profont11_tf);
          u8g2.drawUTF8(3, 50, "Maximalwert:");
          u8g2.setCursor(76, 50);
          u8g2.print(Maxlight);

          if (sw != lastsw) {
            menu = 13;
            encoderPos = 1;
            lastsw = sw;
          }

        }
        break;

      case 9:

          getTemp();

        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawUTF8(33, 28, "Temperatur:");
        u8g2.setCursor(55, 50);
        u8g2.print(Temp);
        u8g2.drawUTF8(68, 50, "°C");

        if (sw != lastsw) {
          menu = 1;
          encoderPos = 3;
          lastsw = sw;
        }
        
        break;

      case 10:

        u8g2.drawFrame(0, 12, 4, 4);

        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawUTF8(53, 28, "SD-Log");
        u8g2.setFont(u8g2_font_open_iconic_mime_2x_t);
        u8g2.drawGlyph(33, 33, 72);

        if (sd == true) {
          sdlog = true;
          SDLog = "";
          SDLog += String("on");
        }
        if (sd == false) {
          sdlog = false;
          SDLog = "";
          SDLog += String("off");
        }
        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.setCursor(59, 50);
        u8g2.print(SDLog);

        if (sw != lastsw) {
          menu = 1;
          encoderPos = 3;
          lastsw = sw;
        }
        break;

      case 11:
        getOntime();

        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawUTF8(30, 28, "Anschaltzeit");
        u8g2.setCursor(50, 50);
        u8g2.print(Ontime);

        if (sw != lastsw) {
          menu = 1;
          encoderPos = 3;
          lastsw = sw;
        }
        break;

      case 12:
        getOfftime();

        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawUTF8(27, 28, "Ausschaltzeit");
        u8g2.setCursor(50, 50);
        u8g2.print(Offtime);

        if (sw != lastsw) {
          menu = 1;
          encoderPos = 3;
          lastsw = sw;
        }
        break;

      case 13:
        getMaxlight();

        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawUTF8(30, 28, "Maximalwert");
        u8g2.setCursor(53, 50);
        u8g2.print(Maxlight);

        if (sw != lastsw) {
          menu = 1;
          encoderPos = 3;
          lastsw = sw;
        }
        break;
    }

    if (menu >= 16 || menu <= 0) {
      menu = 1;
    }
  } while ( u8g2.nextPage() );

}

void setup() {
  

  u8g2.begin();

  pinMode(encoderPinA, INPUT);
  pinMode(encoderPinB, INPUT);

  attachInterrupt(digitalPinToInterrupt(2), EncoderA, CHANGE); // encoder pin on interrupt 0 (pin 2)
  attachInterrupt(digitalPinToInterrupt(3), EncoderB, CHANGE); // encoder pin on interrupt 1 (pin 3)
  attachInterrupt(digitalPinToInterrupt(18), Switch, RISING); // encoder switch on interrupt 2 (pin 18)

  SD.begin(chipSelect);
  
  Serial.begin(9600);  // start Serial communication
  Serial.println(F("booting..."));
  Serial.println(F(""));

  Serial.print(F("Initializing SD card..."));

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println(F("Card failed, or not present"));
  }
  else Serial.println(F("card initialized."));
  
  Wire.begin();

  dht1.begin();
  dht2.begin();

  startMillis = millis();
  startMillis2 = millis();
  startMillis3 = millis();
  startMillis4 = millis();
  startMillis5 = millis();
  Serial.println(F("booting up done")); 
  Serial.println(F(""));
}

void loop() {

  currentMillis = millis();
  currentMillis3 = millis();
  currentMillis4 = millis();
  currentMillis5 = millis();
  
  if (isnan(h) || isnan(t) || isnan(h2) || isnan(t2)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
    }

  if (currentMillis3 - startMillis3 >= 2000) {
    getTime();
    getDHT();
    getSoilSensors();


    startMillis3 = currentMillis3;
  }

  getLightrb();
  getLightw();
  getHeat();
  getLightrbauto();
  getLightwauto();
  getHeatauto();


  if (currentMillis - startMillis >= 10000 && sdlog == true) {

    getRTC();

    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open("datalog.txt", FILE_WRITE);

    // if the file is available, write to it:
    if (dataFile) {
      dataFile.print(RTCString);
      dataFile.print(" ");
      dataFile.print("Bodenfeuchtigkeitssensor: ");
      dataFile.print(capString);
      dataFile.print(" ");
      dataFile.print("Temp/Humid1: ");
      dataFile.print(DHTString);
      dataFile.print(" ");
      dataFile.print("Temp/Humid2: ");
      dataFile.print(DHT2String);
      dataFile.print(" ");
      dataFile.print("Licht rot-blau: ");
      dataFile.print(RBLString);
      dataFile.print(" ");
      dataFile.print("Licht warmweiß: ");
      dataFile.print(WLString);
      dataFile.print(" ");
      dataFile.print("Heizung: ");
      dataFile.println(HeatString);
      dataFile.close();
      
      Serial.println(F("Writing to SD:"));
      Serial.print(F("Date/Time: "));
      Serial.println(RTCString);
      Serial.print(F("Soil moisture: "));
      Serial.println(capString);
      Serial.print(F("Sensor oben: "));
      Serial.println(DHTString);
      Serial.print(F("Sensor unten: "));
      Serial.println(DHT2String);
      Serial.print(F("Licht rot-blau: "));
      Serial.println(RBLStringauto);
      Serial.print(F("Licht weiß: "));
      Serial.println(WLStringauto);
      Serial.print(F("Heizung: "));
      Serial.println(HeatStringauto);
      Serial.println(F(""));
      Serial.println(F(""));
    }
    else {
      Serial.println(F("error opening datalog.txt"));
      Serial.println(F(""));
    }
   
    startMillis = currentMillis;
  }

  homescreen();

  if (menu == 3 || menu == 4 || menu == 5 || menu == 6) {
    analogWrite(8, wl);
    analogWrite(9, rbl);
    analogWrite(10, heat);
  } else {
    analogWrite(8, wlauto);
    analogWrite(9, rblauto);
    analogWrite(10, heatauto);


    if (averaget < temp) {
      analogWrite(10, 255);
      heatauto = 255;
    }
    else if (averaget > temp) {
      analogWrite(10, 0);
      heatauto = 0;
    }

    if ((Clock.getHour(h12, PM)) >= HOn && (Clock.getMinute()) >= on && (Clock.getHour(h12, PM)) <= HOff && firstcyl == 0) {
      firstcyl = 1;
      rblauto ++;
    }
    else if ((Clock.getHour(h12, PM)) >= HOn && firstcyl == 1) {
      if ((currentMillis4 - startMillis4 >= durtime) && rblauto <= lightpwm && rblcnt == 0) {   //7060
        if (rblauto >= lightpwm) {
          rblcnt = 1;
          wlcnt = 1;
        }
        else if (rblauto != lightpwm) rblauto ++;
        startMillis4 = currentMillis4;
      }
      else if ((currentMillis4 - startMillis4 >= durtime) && rblauto >= 0 && rblcnt == 1) {
        if (rblauto <= 0) {
          rblcnt = 1;
          firstcyl = 2;
        }
        else if (rblauto != 0) rblauto --;
        startMillis4 = currentMillis4;
      }
      if ((currentMillis5 - startMillis5 >= durtime) && wlauto < lightpwm && wlcnt == 1) {
        wlauto ++;
        startMillis5 = currentMillis5;
      }
    }

    if ((Clock.getHour(h12, PM)) >= HOff && (Clock.getMinute()) >= off && firstcyl == 2) {
      firstcyl = 3;
      wlauto --;
    }
    else if ((Clock.getHour(h12, PM)) >= HOff && firstcyl == 3) {
      if ((currentMillis4 - startMillis4 >= durtime) && rblcnt == 1) {   //7060
        if (rblauto >= lightpwm) {
          rblcnt = 0;
          wlcnt = 0;
        }
        else if (rblauto != lightpwm) rblauto ++;
        if (wlauto != 0) wlauto --;
        startMillis4 = currentMillis4;
      }
      else if ((currentMillis4 - startMillis4 >= durtime) && rblcnt == 0) {
        if (rblauto != 0) rblauto --;
        startMillis4 = currentMillis4;
      }
    }
  }

  if ((Clock.getHour(h12, PM)) == 0 && (Clock.getMinute()) == 0) firstcyl = 0;
  
  rotating = true;  // reset the debouncer

  if (lastReportedPos != encoderPos)
  {
    lastReportedPos = encoderPos;
  }
}