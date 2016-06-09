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
{ 
  B00000000, B11000000,
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
#define PIN_RELAY 8
#define PIN_POT 2    // Analog A2, value between 0 - 1023
#define PIN_BT_A 5
#define PIN_BT_B 3

OneWire oneWire(PIN_THERM);
DallasTemperature sensors(&oneWire);
DeviceAddress tempDeviceAddress;

// VARIABLES //

int output_relay;   // 0: power on | 1: shutdown
int temp_res = 10, blink_i = 0;

// sonde therm
unsigned long lastTempRequest = millis(), delayInMillis = 2000 / (1 << (12 - temp_res));

// temp
float temp = 0, temp_max = 0, temp_day = 25, temp_night = 18;

// time
DateTime now;
DateTime dt_start_day (now.year(), now.month(), now.day(), 8, 30, now.second());  // 8h30
DateTime dt_end_day (now.year(), now.month(), now.day(), 21, 30, now.second());  // 21h30
DateTime t_start = 'null', t_end = 'null';


// buttons
#define LONG_PRESS 1000
enum {
  WAIT, CHANGE_TIME, T1, T2, SET_TEMPERATURE};
Button bt_a(PIN_BT_A, true, false, 20);
Button bt_b(PIN_BT_B, true, false, 20);
uint8_t STATE;

boolean is_day = 'null', switch_temp_display = false; 
boolean debug = true;

void setup() { 

  Serial.begin(9600);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  
  //Adafruit splashscreen
  display.display();
  delay(1000);
  display.clearDisplay();
  
  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, temp_res);

  sensors.setWaitForConversion(true);
  temp = sensors.getTempCByIndex(0);
  sensors.requestTemperatures();
  
  pinMode(PIN_RELAY, OUTPUT);
  sensors.setAlarmHandler(&switch_relay);
}


void loop() {

  now = rtc.now();
  is_day = check_time();
  Serial.println( delayInMillis );
  get_temp();
  buttons();

  switch_temp_display ? get_display_temperature() : get_display_default();
  
  sensors.setLowAlarmTemp(tempDeviceAddress, temp_max);
  sensors.processAlarms();

}   // end loop


// detect if night or day
boolean check_time() {

  if ( now.hour() > dt_end_day.hour() || now.hour() < dt_start_day.hour() )     // 22 > 21 && 7 < 8 -> night
    is_day = false;

  else {

    if ( now.hour() < dt_end_day.hour() && now.hour() > dt_start_day.hour() )   // 20 < 21 && 7 > 8 -> day
      is_day = true;

    else {

      if ( now.hour() == dt_end_day.hour() ) {         // 21h = 21h
        if ( now.minute() >= dt_end_day.minute()  )    // 45m >= 30m
          is_day = false;
        else
          is_day = true;
      }

      else if ( now.hour() == dt_start_day.hour() ) {  // 8h = 8h
        if ( now.minute() >= dt_end_day.minute()  )    // 45m >= 30m
          is_day = true;
        else
          is_day = false;
      }  
    }
  }

  return is_day;
}


// - BUTTONS_FUCT -
void buttons() {

  bt_a.read();   bt_b.read();
  switch_temp_display = false;

switch (STATE) {

  case WAIT:
    // Serial.println("WAIT");
    blink_i = 0;    
    if ( bt_a.wasPressed() && bt_b.isReleased() )
      STATE = SET_TEMPERATURE;

    else if ( bt_a.isReleased() && bt_b.wasReleased() )
      STATE = CHANGE_TIME;
    break;

  case CHANGE_TIME:
    // Serial.println("change_time");
    blink_i += 1;

    if ( bt_a.isPressed() && bt_b.isReleased() )
      ( is_day ) ? STATE = T2 : STATE = T1;

    else if ( bt_b.isPressed() && bt_a.isReleased() )
      ( is_day ) ? STATE = T1 : STATE = T2;

    if_inactive_go_to_wait_state();
    break;

  case T1:
    // Serial.println( "T1" );
    blink_i += 1;
    bt_add_time( &dt_start_day );
    break;

  case T2:
    // Serial.println( "T2" );
    blink_i += 1;
    bt_add_time( &dt_end_day );
    break;

  case SET_TEMPERATURE:
    // Serial.println( "SET TEMPERATURE" );
    switch_temp_display = true;

    // set temp max day
    if ( bt_a.pressedFor(LONG_PRESS) && bt_b.isReleased() )
      temp_day = set_temp();

    // set temp max night
    if ( bt_b.isPressed() && bt_a.isReleased() )
      temp_night = set_temp(); 

    if_inactive_go_to_wait_state();
    break;
  }
} // end fct

void bt_add_time(DateTime *dt) {

  if_inactive_go_to_wait_state();

  if ( bt_a.wasReleased() && bt_b.wasReleased() )
    STATE = WAIT;
  else {

    if ( bt_a.pressedFor(LONG_PRESS) ) { // || bt_a.isPressed() ) {
      *dt = *dt + TimeSpan(0, 0, 30, 0);
      delay(LONG_PRESS/2);
    }

    if ( bt_b.pressedFor(LONG_PRESS) ) { // || bt_b.isPressed() ) {
      *dt = *dt - TimeSpan(0, 0, 30, 0);
      delay(LONG_PRESS/2);
    }
  }

}

// if inative during x sec go to wait status
void if_inactive_go_to_wait_state() {

  if ( bt_a.releasedFor(LONG_PRESS*5) && bt_b.releasedFor(LONG_PRESS*5) || bt_a.isPressed() && bt_b.isPressed() )
    STATE = WAIT;
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
void get_temp() {

  if (millis() - lastTempRequest >= delayInMillis) { // waited long enough??
    temp = sensors.getTempCByIndex(0);

    sensors.requestTemperatures();
    lastTempRequest = millis(); 
  } 
}

float set_temp(void) {
  
  float pot_val = 16.0 + (analogRead(PIN_POT) / 51.2) ; 
  return pot_val;
}

void switch_relay(const uint8_t* deviceAddress) {
  
  char temp_c = (char) temp;
  
  ( temp_c < sensors.getLowAlarmTemp(deviceAddress) ) ? output_relay = 0 : output_relay = 1; 
  digitalWrite( PIN_RELAY, output_relay );
}

// - OLED_FUCT -
void get_display_default() {

  display.setCursor(0,0);
  display.setTextSize(2);
  display.setTextColor(WHITE);

  // select time range in therms of day or night mode
  if ( is_day ) {
    display.print('D');
    temp_max = temp_day;
    t_start = dt_start_day;
    t_end = dt_end_day;
  }
  else {
    display.print('N');
    temp_max = temp_night;
    t_start = dt_end_day;
    t_end = dt_start_day;
  }

  // time
  display.setCursor(20,0);
  display_time( now );

  // ON
  display.setCursor(90,0);
  ( output_relay == 0 ) ? display.print("ON") : display.print("OFF");

  // current temp
  display.setCursor(0,25);
  display.print( temp );

  // temp_max
  display.setCursor(65,25);
  display.print(temp_max);

  // time range
  blink();

  display.display();
  display.clearDisplay();

}  // end fct


void blink() {

 if ( blink_i % 2 == 1) {
    delay(250);
    blink_i =+ 1;
  }
  
  else {    
    switch (STATE) {

    case CHANGE_TIME: 
      display_time_range();
      break;
    case T2:
      ( ! is_day ) ? display_t_start() : display_t_end();      
      break;
      
    case T1:
      ( ! is_day ) ? display_t_end() : display_t_start();      
      break;
      
    default: 
      display_time_range();
      display_t_start();
      display_t_end();
      break;
    }
  }
}

void display_time_range() {
  display_t_start();
  display_t_end();
}

void display_t_start() {
  display.setCursor(0,50);
  display_time( t_start );
}

void display_t_end() {
  display.setCursor(65,50);
  display_time( t_end );
}

void get_display_temperature () {

  display.setCursor(0,0);
  display.setTextSize(2);
  display.setTextColor(WHITE);

  display.println("SET TEMP");

  display.setCursor(0,25);
  display.println("D");

  display.setCursor(25,25);
  display.println(temp_day);

  display.setCursor(0,50);
  display.println("N");

  display.setCursor(25,50);
  display.println(temp_night);

  display.display();
  display.clearDisplay();
}


void display_time(const DateTime dt) {

  display_digit( dt.hour() );
  display.print('h');
  display_digit( dt.minute() );

} //end oled fct

void display_digit(int digits) {
  if(digits < 10)
    display.print('0');
  display.print(digits);
}

