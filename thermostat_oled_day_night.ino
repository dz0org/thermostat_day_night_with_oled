// Date and time functions using a DS3231 RTC connected via I2C and Wire lib
// check other TimeRTC.pde
#include <Wire.h>
#include "RTClib.h"
#include <Button.h>

// sensor temperature
#include <OneWire.h>
#include <DallasTemperature.h>

// Oled
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

RTC_DS1307 rtc;

#define OLED_RESET 4
#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2

Adafruit_SSD1306 display(OLED_RESET);

#define LOGO16_GLCD_HEIGHT 16 
#define LOGO16_GLCD_WIDTH  16 
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

// PIN
#define PIN_THERM 10
#define PIN_RELAY 9
#define PIN_POT 2    // Analog A2, value between 0 - 1023
#define PIN_BT_A 5
#define PIN_BT_B 3

OneWire oneWire(PIN_THERM);
DallasTemperature sensors(&oneWire);

// VARIABLES //
// temp
float tempC = 0, temp_max = 0, temp_max_day = 32, temp_max_night = 18;

// time

DateTime now;
DateTime dt_start_day (now.year(), now.month(), now.day(), 8, 30, now.second());  // 8h30
DateTime dt_end_day (now.year(), now.month(), now.day(), 23, 34, now.second());  // 21h30
  
// buttons
#define LONG_PRESS 1000
enum {WAIT, CHANGE_TIME, T_START, T_END, STOP};  
Button bt_a(PIN_BT_A, true, false, 20);
Button bt_b(PIN_BT_B, true, false, 20);
uint8_t STATE;

boolean day_mode = 'null';    // day_mode = 0 


void setup() { 
  
  Serial.begin(9600);

  sensors.begin();   // sonde thermia on
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  
  //Adafruit splashscreen
  display.display();
  delay(1000);
  display.clearDisplay();
  
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

}


void loop() {
  
  DateTime now = rtc.now();

  
  // detect if night or day
  if ( now.hour() > dt_end_day.hour() && now.hour() < dt_start_day.hour() )     // 22 > 21 && 7 < 8 -> night
    day_mode = false;
    
  else {
    
    if ( now.hour() < dt_end_day.hour() && now.hour() > dt_start_day.hour() )   // 20 < 21 && 7 > 8 -> day
     day_mode = true;
     
    else {
     
      if ( now.hour() == dt_end_day.hour() ) {         // 21h = 21h
        if ( now.minute() >= dt_end_day.minute()  )    // 45m >= 30m
          day_mode = false;
        else
          day_mode = true;
      }
      
      else if ( now.hour() == dt_start_day.hour() ) {  // 8h = 8h
        if ( now.minute() >= dt_end_day.minute()  )    // 45m >= 30m
          day_mode = true;
        else
          day_mode = false;
      }  
    }
  } 

  
  button_action(day_mode);
  set_oled(day_mode);

  // int relay_on_off = check_tempC();
  // digitalWrite(PIN_RELAY,relay_on_off);

}   // end loop


// - BUTTONS_FUCT -
void button_action(boolean day_mode) {
  
  bt_a.read(); bt_b.read();

  switch (STATE) {
      
      case WAIT:
          Serial.println("WAIT");    
          if ( bt_a.pressedFor(1000) && bt_b.isReleased() )
            set_tempC_with_pot();
            
          if ( bt_b.isPressed() && bt_a.isReleased() )
            Serial.println("select mode D/N");
          else if ( bt_a.isPressed() && bt_b.isPressed() )
            STATE = CHANGE_TIME;
          break;
          
      case CHANGE_TIME:
          Serial.println("change_time");
          if ( bt_a.wasReleased() )
            STATE = T_END;
          else if ( bt_b.wasReleased() )
            STATE = T_START;
          break;
          
      case T_START:
          Serial.println( "T_START" );
          if ( day_mode )
            bt_add_time( &dt_start_day );
          else 
            bt_add_time( &dt_end_day );
          break;
            
      case T_END:
          Serial.println( "T_END" );
          if ( day_mode )
            bt_add_time( &dt_end_day );
          else 
            bt_add_time( &dt_start_day );
          break;
    }
} // end fct

void bt_add_time(DateTime *dt) {
  
  // if inative during 15sec go to wait status 
  if ( bt_a.releasedFor(15000) && bt_b.releasedFor(15000) ) {
      STATE = WAIT;
      // Serial.println( "inatif go wait");
    }
    
  // pressed the two buttons go to wait status
  if ( bt_a.isPressed() && bt_b.isPressed() )
    STATE = WAIT;
  else {
        
      if ( bt_a.pressedFor(LONG_PRESS) || bt_a.isPressed() ) {
        Serial.println( "+1h" );
        *dt = *dt + TimeSpan(0, 1, 0, 0);
      }
        
      if ( bt_b.pressedFor(LONG_PRESS) || bt_b.isPressed() ) {
        Serial.println( "+5m" );
        *dt = *dt + TimeSpan(0, 0, 5, 0);
      }
  }
  
}

// - TIME_FUCT -
//DateTime add_time(DateTime *dt, TimeSpan time) {
//  *dt = *dt + time;
//  return *dt;
//} 

void get_time(const DateTime dt, boolean full) {
  
  Serial.println();
  Serial.print(dt.hour(), DEC);
  Serial.print('h');
  Serial.print(dt.minute(), DEC);
  
  if ( full ) {
    Serial.print(' ');
    Serial.print(dt.day(), DEC);
    Serial.print('/');
    Serial.print(dt.month(), DEC);
    Serial.print('/');
    Serial.print(dt.year(), DEC);
  }
  Serial.println();

} //end fct

// - TEMP_FUCT -
void set_tempC_with_pot(void) {
  
  float pot_val = 16.0 + (analogRead(PIN_POT) / 51.2) ;
  
  Serial.println("Potentiometer");
  
  if (day_mode == 0) {
    temp_max_day = pot_val;
    //Serial.print("set D max temp");
  }
  else if ( day_mode == 1) {
    temp_max_night = pot_val;
    //Serial.print("set N max temp");
  }
  else
    Serial.println("Error durring potentiometer value");
    
  // Serial.println("pot_val : ");
  // Serial.print(pot_val); 
}  // end fct



float get_current_tempC(void) {
  
  sensors.requestTemperatures();
  tempC = sensors.getTempCByIndex(0);
  
  return tempC;
}   // end fct


int check_tempC(void) {
   
  tempC = get_current_tempC();
  Serial.println(tempC);
  
  if (tempC < temp_max) {
    Serial.print(" Relay: [ON]");
    return 0;
  }
  
  else {
    Serial.print(" Relay: [OFF]");
    return 1;
  }
  
}  // end fct



// - OLED_FUCT -
void set_oled(boolean day_mode) {

  DateTime now = rtc.now(), t_start = 'null', t_end = 'null';
  
  display.setCursor(0,0);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  
  // select time slot in therms of day or night mode
  if ( day_mode ) {
    display.print('D');
    t_start = dt_start_day;
    t_end = dt_end_day;
  }
  else {
    display.print('N');
    t_start = dt_end_day;
    t_end = dt_start_day;
  }

  // time
  display.setCursor(20,0);
  display_time( now );
  
  // ON
  display.setCursor(90,0);
  display.print("ON");
 
  // curren temp
  display.setCursor(0,25);
  display.print(get_current_tempC(), 1);
  
  // temp_max
  display.setCursor(65,25);
  display.print("MAX C");
    
  // time slot
  display.setCursor(0,50);
  display_time( t_start );
  
  display.setCursor(65,50);
  display_time( t_end );
    
  display.display();
  display.clearDisplay();

}  // end fct

void display_time(const DateTime dt) {
  
  display.print(dt.hour(), DEC);
  display.print('h');
  display.print(dt.minute(), DEC);

} //end old fct

