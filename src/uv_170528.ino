/*********************************************************************** 
  For all libraries from Adafruit:
  Adafruit invests time and resources providing this 
  open source code, please support Adafruit and open-
  source hardware by purchasing products from Adafruit!
  Written by Limor Fried/Ladyada for Adafruit Industries 
  BSD license, all text above must be included in any redistribution
 ***********************************************************************/


//---------- LIBRARY INCLUDES
#include "SPI.h"                        // Arduino 1.8.2 libray
#include "Adafruit_SPIFlash.h"          // Adafruit v1.0.0 https://github.com/adafruit/Adafruit_SPIFlash
#include "Adafruit_SPIFlash_FatFs.h"    // Adafruit v1.0.0 https://github.com/adafruit/Adafruit_SPIFlash
#include "SimpleTimer.h"                // Arduino launch timed actions http://playground.arduino.cc/Code/SimpleTimer
#include "RTCZero.h"                    // Arduino real tinme clock for Atmel M0 devices v1.5.2 http://www.arduino.cc/en/Reference/RTCZero
#include "Adafruit_SharpMem.h"          // Adafruit SHARP Display v1.0.4 https://github.com/adafruit/Adafruit_SHARP_Memory_Display 
#include "Adafruit_GFX.h"               // Adafruit GFX Library v1.2.2 https://github.com/adafruit/Adafruit-GFX-Library
#include "Adafruit_Sensor.h"            // Adafruit Unified Sensor v1.0.2 https://github.com/adafruit/Adafruit_Sensor
#include "Adafruit_BME280.h"            // Adafruit BME280 Library v1.0.5 https://github.com/adafruit/Adafruit_BME280_Library
#include "Adafruit_VEML6070.h"          // Adafruit VEML6070 Library 5-26-16 https://github.com/adafruit/Adafruit_VEML6070


//---------- GLOBAL DECLARATIONS
#define FLASH_TYPE      SPIFLASHTYPE_W25Q16BV           // Flash chip type
#define FLASH_SS        SS1                             // Flash chip CS pin (Feather MO Express)
#define FLASH_SPI_PORT  SPI1                            // Use SPI bus 2 (Feather MO Express)
Adafruit_SPIFlash flash(FLASH_SS, &FLASH_SPI_PORT);     // initiate SPI flash system (hardware SPI)
Adafruit_M0_Express_CircuitPython pythonfs(flash);      // initiate Circuit Python file system
#define FILE_NAME       "uvlog.csv"                     // http://elm-chan.org/fsw/ff/en/open.html

#define SPEAKER     A1                                  // Buzzer pin 
#define VBAT_PIN    A7                                  // Feather series battery monitoring pin
#define BUTTON      5                                   // Button pin

SimpleTimer timer;                                      // initiate interrupt based timers to call functions
RTCZero rtc;                                            // initiate M0 SAMD timebase
char DATE[7]        = "171222";                         // Date stamp 
char TIME[7]        = "230059";                         // Time stamp

#define SHARP_CS    12                                  // Display SPI chip select pin
Adafruit_SharpMem display(SCK, MOSI, SHARP_CS);         // initiate Sharp memory display 96 x 96  16 chr x 12 lines
#define BLACK       0                                   // Sharp Memory Display foreground color
#define WHITE       1                                   // Sharp Memory Display background color
#define CHAR_W      6                                   // Display size 1 character width
#define CHAR_H      8                                   // Display size 1 character height
#define MARGIN      3                                   // Margin for graphics
#define ROTATION    2                                   // Display orientation
int LINE[12];                                           // Array for display line numbers each 1 character high

#define BME280_ADDRESS   0x76                           // BME280 I2C Address
#define SEALEVELPRESSURE_HPA     (1017.80)              // Sea level ref reading (standard = 1013.25)
double BARO_MB      = 1086.00;                          // Barometer (milli bars)               0-1086
int BARO_PA         = 10860;                            // Barometer (pascals)                  0-10860
double BARO_HG      = 29.29 ;                           // Barometer (inches mercury)           0-10860
double ALT_M        = 1000.00;                          // Altitude (meters)                    0-11,000
int TEMP_C          = 50;                               // Temperature degrees (Celsius)        0-50
int TEMP_F          = 122;                              // Temperature degrees (Fahrenheit)     0-120
int HUMD            = 100;                              // Relative humididty (%)               0-100 
int DEWPOINT        = 60;                               // Dew Point (Fahrenheit)               0-100
int HEATIX          = 120;                              // Heat Index (Fahrenheit)              0-150
Adafruit_BME280 bme;                                    // initiate BME280 barometer Temperature Humdity chip

Adafruit_VEML6070 uv = Adafruit_VEML6070();             // initiate VEML6070 UV chip
double UVINDEX      = 15.00;                            // UV Index                             0.00-26.00

int MINS2MED        = 555;                              // Mins to Minimal Erythemal Dose (MED) 0-inf
int FITZ            = 13;                               // Fitzpatrick Score                    0-32
int SPF             = 30;                               // SPF Applied                          0-100
bool H2O            = 0;                                // On Water (bool)                      yes = 1 no = 0
bool SNOW           = 0;                                // Snow Present (bool)                  yes = 1 no = 0
int RECORDS         = 0;                                // Number of records written to flash
double VOLTS      = 5.1;


//---------- SPECIAL FUNCTIONS
double med () {                                                                     // Mins to Minimal Erythemal Dose (MED)
    
    double uvi_f =                                      // Real time UV Index factoring
                     (UVINDEX * (ALT_M * 1.2)) +        // Real time altitude amplifier (1.2 x meters above sea level)  
                     (UVINDEX * (H2O * 1.5)) +          // On water UVindex amplifier (1.5x)
                     (UVINDEX * (SNOW * 1.85));         // On Snow UVindex amplifier (1.85x)
                    
    double s2med_b = (-3.209E-5 * pow(FITZ, 5)) +       // Fitz score @ 1 UV index seconds to MED (5th order polynomial plot)  
                     (2.959E-3 * pow(FITZ, 4)) -
                     (0.103 * pow(FITZ, 3)) +
                     (1.664* pow(FITZ, 2)) +
                     (3.82 * FITZ) + 
                     34.755;
    double s2med = ((s2med_b / uvi_f) * SPF);           // Combine factors (secs to MED at UVindex 1 + UV index amplifiing factors)
    int m2med = s2med / 60;                             // Convert seconds to minutes (integer)
    return m2med;
}
double c2f(double celsius) {                                                        // Convert temp Celcius to Fahrenheit
    return ((celsius * 9) / 5) + 32; 
}
double pa2hg(double pascals) {                                                      // Convert pressure Pascals to in. mercury
    return (pascals * 0.000295333727); 
}
double dewPointF(double cel, int hum) {                                             // Calc Dew Point (F) from temp (C) & rel humd
    double Td = (237.7 * ((17.271 * cel) / (237.7 + cel) + log(hum * 0.01))) / 
                (17.271 - ((17.271 * cel) / (237.7 + cel) + log(hum * 0.01)));
    return c2f(Td);
}
double heatIndex(double tempF, double humidity) {                                   // Calc Heat Index (F) from temp (F) & rel humd
    if (tempF < 80 || humidity < 40) { double pass = tempF; return pass;}
    double c1=-42.38, c2=2.049, c3=10.14, c4=-0.2248, c5=-6.838e-3;
    double c6=-5.482e-2, c7=1.228e-3, c8=8.528e-4, c9=-1.99e-6  ;
    double t = tempF;
    double r = humidity;
    double a = ((c5 * t) + c2) * t + c1;
    double b = ((c7 * t) + c4) * t + c3;
    double c = ((c9 * t) + c8) * t + c6;
    double rv = (c * r + b) * r + a;
    return rv;
}
char *dtostrf (double val, signed char width, unsigned char prec, char *sout) {     // convert floats to fixed string
// val      Your float variable;
// width    Length of the string that will be created INCLUDING decimal point;
// prec     Number of digits after the deimal point to print;
// sout     Destination of output buffer
    char fmt[20];
    sprintf(fmt, "%%%d.%df", width, prec);
    sprintf(sout, fmt, val);
    return sout;
}


//---------- SETUP
void setup() {
    Serial.begin(115200);                                               // Start serial port for debugging
    while (!Serial);

    Serial.println("START");

	pinMode(VBAT_PIN, INPUT);                                           // Set Battery pin
    pinMode(BUTTON, INPUT_PULLUP);                                      // Set Button pin


    //--------- REAL TIME CLOCK 
	byte seconds, minutes, hours, days, months, years;                  // Set local time variables
    char s_month[5];                                                    //
    int tmonth, tday, tyear, thour, tminute, tsecond;                   //
    static const char month_names[] =                                   //
                            "JanFebMarAprMayJunJulAugSepOctNovDec";     //
    rtc.begin();                                                        // Initiaite real time clock
    sscanf(__TIME__, "%d:%d:%d", &thour, &tminute, &tsecond);           // Pull time out of epic
    sscanf(__DATE__, "%s %d %d", s_month, &tday, &tyear);               // Pull date out of epic
    hours = thour; minutes = tminute; seconds = tsecond;                // Parse hours, minutes, seconds
    tmonth = (strstr(month_names, s_month) - month_names) / 3;          // Parse month
    years = tyear - 2000; months = tmonth + 1; days = tday;             // Parse year, month, day
    rtc.setTime(hours, minutes, seconds);                               // Set real time clock to complile time
    rtc.setDate(days, months, years);                                   // Set real time clock to complile date

    Serial.print(rtc.getHours()) ; 
    Serial.print(":"); 
    Serial.println(rtc.getMinutes());


   //--------- DISPLAY
    
    Serial.println("DISPLAY START");    
    
    display.begin();                                                    // Initiaite display
    display.clearDisplay();                                             // clear display (Adafrtuit GFX Library)
    display.setTextSize(1);                                             // Set text size (Adafrtuit GFX Library)
    display.setRotation(ROTATION);                                      // Set rotation (Adafrtuit GFX Library)
    display.setTextColor(BLACK, WHITE);                                 // Set text color foreground and background (Adafrtuit GFX Library)

    for (int i = 0; i <= (display.width() / CHAR_H); i++) {             // Set line numbers
        LINE[i] = i * CHAR_H;
        Serial.print(i * CHAR_H); Serial.print(",");
    }
    
    Serial.println("DISPLAY FINISHED");


    
    //--------- SPI FLASH
    if (!flash.begin(FLASH_TYPE)) {                                     // Initiate file system
        Serial.println("FLASH INIT FAIL");
        while(1);
    }
    Serial.println("FLASH INIT SUCCESS");
    
    Serial.print("Flash chip JEDEC ID: 0x"); 
    Serial.println(flash.GetJEDECID(), HEX); 

    if (!pythonfs.begin()) {                                            // Mount CircuitPython file system
        Serial.println("MOUNT FAIL"); 
        while(1); 
    }  
    
    Serial.println("MOUNT SUCCESS");
    
    //--------- LOG FILE
    File data = pythonfs.open(FILE_NAME, FILE_WRITE);                 // Circuit Python File Open       
    if (data) {                                                         // Write header record
        data.print("date,");
        data.print("time,");      
        data.print("batt(v),");
        data.print("uvi,");
        data.print("med(mins),");
        data.print("alt(m),");
        data.print("temp(c),");
        data.print("temp(f),");
        data.print("baro(mb),");
        data.print("baro(hg),");        
        data.print("humd(%),");
        data.print("dewpoint(f),");  
        data.println("heat index(f)");
        data.close();
        
        Serial.print("date,");
        Serial.print("time,");      
        Serial.print("batt(v),");
        Serial.print("uvi,");
        Serial.print("med(mins),");
        Serial.print("alt(m),");
        Serial.print("temp(c),");
        Serial.print("temp(f),");
        Serial.print("baro(mb),");
        Serial.print("baro(hg),");        
        Serial.print("humd(%),");
        Serial.print("dewpoint(f),");  
        Serial.println("heat index(f)");
        
        Serial.println("HEADER1: SUCCESS");
    }
    else {
        
        Serial.println("HEADER: FAIL");
    }


    
    Serial.println("FILEOPS FINISHED");


 
    //--------- SENSORS
    uv.begin(VEML6070_1_T);                                             // Initiate VEML6070 - pass int time constant

    Serial.println("VEML6070 SUCCESS");
    
	if (! bme.begin(BME280_ADDRESS)) {                                       // Initiate BME280

        Serial.println("BME280 FAIL");
	    
	    while (1);  
    }

    Serial.println("BME280 SUCCESS");

    //--------- TIMERS
    timer.setInterval(2000, GetReadings);                               // get sensor readings every 2 seconds
    timer.setInterval(5000, FileWrite);                                // write readings to flash every 20 seconds

    chime(1);                                                           // chime (x) time(s)

    Serial.println("SETUP DONE");
}

int reading         = LOW;                              // button state
int state           = HIGH;                             // button transient state
int previous        = LOW;                              // stored button state 
long time           = 0;                                // button timer
long debounce       = 200;                              // check for phantom presses (ms)


void loop() {
    timer.run();                                                        // Initiate timed events
    checkButton();                                                      // check for button press
}

void chime(int times) {
    for (int i = 0; i < times; i++) {
        for (int ii = 0; ii < 3; ii++) {
            int melody[] = {506, 1911};
            tone(A1, melody[ii], 300);
            delay(300);
        }
    }
}

void checkButton() {
    reading = digitalRead(BUTTON);    
    if (reading == HIGH && previous == LOW && millis() - time > debounce) {    
        if (state == HIGH) state = LOW; 
        else state = HIGH; 
        time = millis();    
    }   
    previous = reading; 
    if (state == HIGH) { 
        displayWrite("Min Erythml Dose");
    }
    else { 
        displayWrite("Sensor Readings ");
    }
}

void GetReadings() {                                                    // Get readings by timer interval
    sprintf(DATE, "%02d%02d%02d",                                       // Get M0 date from running real time clock
                rtc.getYear(),rtc.getMonth(),rtc.getDay());
    sprintf(TIME, "%02d%02d%02d",                                       // Get M0 time from running real time clock
                rtc.getHours(),rtc.getMinutes(), rtc.getSeconds());    

    VOLTS       = (analogRead(VBAT_PIN) * 6.6)/1024;                    // Get FEATHER battery voltage reading
    UVINDEX     = uv.readUV();                                          // Get VEML6070 UV reading
    TEMP_C      = bme.readTemperature();                                // Get BME280 temperature (Celsius) reading
    HUMD        = bme.readHumidity();                                   // Get BME280 relative humididty (%) reading
    BARO_PA     = bme.readPressure();                                   // Get BME280 barometric pressure (Pascals) reading   
    ALT_M       = bme.readAltitude(SEALEVELPRESSURE_HPA);                         // Get BME280 calculated altitude reading
    UVINDEX     /= 100.0;                                               // Convert reading to UV Index
    TEMP_F      = c2f(TEMP_C);                                          // Convert Celsius reading to Fahrenheit
    BARO_MB     = BARO_PA / 100;                                        // Convert Pascals reading to millibars
    BARO_HG     = pa2hg(BARO_PA);                                       // Convert Pascals reading to Inches Mercury
    if (ALT_M < 10.00) ALT_M = 0;                                       // Zero altitude if under 10 meters
    DEWPOINT    = dewPointF(TEMP_C, HUMD);                              // Calculate Dew Point
    HEATIX      = heatIndex(TEMP_F, HUMD);                              // Calculate Heat Index
    MINS2MED    = med();                                                // Calculate Minutes to Minimal Erythemal Dose (MED)

    Serial.print("READING:");   Serial.print(" | ");
    Serial.print(DATE);         Serial.print(" | ");
    Serial.print(TIME);         Serial.print(" | ");       
    Serial.print(VOLTS);        Serial.print(" | ");
    Serial.print(UVINDEX);      Serial.print(" | ");
    Serial.print(TEMP_C);       Serial.print(" | ");
    Serial.print(HUMD);         Serial.print(" | ");
    Serial.print(BARO_MB);      Serial.print(" | ");
    Serial.print(MINS2MED);     Serial.print(" | ");
    Serial.print(ALT_M);        Serial.print(" | ");
    Serial.print(TEMP_F);       Serial.print(" | ");        
    Serial.print(BARO_MB);      Serial.print(" | ");
    Serial.print(BARO_HG);      Serial.print(" | ");
    Serial.print(DEWPOINT);     Serial.print(" | ");   
    Serial.println(HEATIX);
}

void displayWrite(String statmsg) {                                     // Write display
    header(VOLTS);                                                      // Draw header
    body();                                                             // Draw body
    display.setCursor(0, display.height()-CHAR_H);                      // Set curser to last line
    display.print(statmsg);                                             // Print passed status message
    display.refresh();                                                  // Write screen
}   

void header(double batt) {                                              // Draw header
    char batt_d[4] ; 
    dtostrf(batt, 2, 1, batt_d);
    char head[13] = "00:00   5.5v";
    sprintf(head, "%02d:%02d:%02d %sv", 
            rtc.getHours(),rtc.getMinutes(), rtc.getSeconds(), batt_d);
    display.setCursor(0, LINE[0]);
    display.print(head);
    
    char head2[16] = "12/22 9000 recs";
    sprintf(head2, "%02d/%02d  %i recs", 
            rtc.getMonth(),rtc.getDay(), RECORDS);
    display.setCursor(0, LINE[1]);
    display.print(head2);
    
    // ---------- BATTERY ICON VARIABLES
    int BATTICON_WIDTH      = 3 * CHAR_W;                               // Makes icon width 3 characters wide
    int BATTICON_STARTX     = display.width() - BATTICON_WIDTH;         // Starting x position
    int BATTICON_STARTY     = LINE[0];                                  // Starting y position
    int BATTICON_BARWIDTH3  = ((BATTICON_WIDTH) / 4);                   // Bar width as equal divisions of width


    // ---------- DRAW BATTERY ICON
    display.drawLine( BATTICON_STARTX + 1, 
                      BATTICON_STARTY,
                      BATTICON_STARTX + BATTICON_WIDTH - 4,
                      BATTICON_STARTY, 
                      BLACK);
                      
    display.drawLine( BATTICON_STARTX, 
                      BATTICON_STARTY + 1,
                      BATTICON_STARTX, 
                      BATTICON_STARTY + 5, 
                      BLACK);
                      
    display.drawLine( BATTICON_STARTX + 1, 
                      BATTICON_STARTY + 6,
                      BATTICON_STARTX + BATTICON_WIDTH - 4,
                      BATTICON_STARTY + 6, 
                      BLACK);
                      
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 1, 
                      BLACK);
                      
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 2,
                      BATTICON_STARTY + 1, 
                      BLACK);
                      
    display.drawLine( BATTICON_STARTX + BATTICON_WIDTH - 1,
                      BATTICON_STARTY + 2, 
                      BATTICON_STARTX + 
                      BATTICON_WIDTH - 1, 
                      BATTICON_STARTY + 4, 
                      BLACK);
                      
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 2,
                      BATTICON_STARTY + 5, 
                      BLACK);
                      
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 5, 
                      BLACK);
                      
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 6, 
                      BLACK);


    // ---------- FILL BATTERY ICON
    if (batt > 4.19) {                                                  // FULL
        display.fillRect(BATTICON_STARTX + 2,                           // x
                         BATTICON_STARTY + 2,                           // y
                         BATTICON_BARWIDTH3 * 3,                        // w
                         3,                                             // h
                         BLACK);                                   // color
        return;      
    }
    
    if (batt > 4.00) {                                                  // 3 bars
        for (uint8_t i = 0; i < 3; i++) {
            display.fillRect(BATTICON_STARTX + 2 + 
                            (i * BATTICON_BARWIDTH3),
                             BATTICON_STARTY + 2,
                             BATTICON_BARWIDTH3 - 1,
                             3, 
                             BLACK);
        }
        return;
    }
    if (batt > 3.80) {                                                  // 2 bars
        for (uint8_t i = 0; i < 2; i++) {                   
            display.fillRect(BATTICON_STARTX + 2 + 
                            (i * BATTICON_BARWIDTH3),
                             BATTICON_STARTY + 2, 
                             BATTICON_BARWIDTH3 - 1, 
                             3, 
                             BLACK);
        }
        return;
    }
    if (batt > 3.40) {                                                  // 1 bar
            display.fillRect(BATTICON_STARTX + 2,
                             BATTICON_STARTY + 2, 
                             BATTICON_BARWIDTH3 - 1, 
                             3, 
                             BLACK);                      
    }
}

void body() {
    // ----------- UV INDEX   
    char uvi_d[4]; 
    dtostrf(UVINDEX, 3, 1, uvi_d);

    char uvi[16] = "UVi 5.5";
    sprintf(uvi, "UV index %s", uvi_d);
    display.setCursor(0, LINE[2]);
    display.print(uvi);
    // Serial.print(uvi); Serial.print("|");
    
    // ----------- DRAW UVI BAR
    display.fillRect(MARGIN,                                                // clear bar
                    LINE[3],                   
                    display.width() - (MARGIN * 2),                         // length
                    CHAR_H,                                                 // 1 character high
                    WHITE); 
                    
    display.drawRect(MARGIN,                                                // Draw clear bar
                    LINE[3], 
                    display.width() - (MARGIN * 2),                         // length
                    CHAR_H,                                                 // 1 character high
                    BLACK);
                    
    display.fillRect(MARGIN,                                                // Fill bar to last UV index read
                    LINE[3],
                    (UVINDEX * ((display.width() - (MARGIN * 2)) / 16)),    // cover 16 divisions
                    CHAR_H,                                                 // 1 character high
                    BLACK);

// ---------- Mins to Minimal Erythemal Dose 
    char med[16] = "MED in 555 min";
    sprintf(med, "MED in %i min", MINS2MED);
    display.setCursor(0, LINE[4]); 
    display.print(med);
    // Serial.print(med); Serial.print("|");
    
// ---------- ALTITUDE
    char alt_d[4];
    dtostrf(ALT_M, 4, 0, alt_d);
 
    char alt[16] = "Alt (mtrs) 5500";
    sprintf(alt, "Alt (mtrs) %s", alt_d);
    display.setCursor(0, LINE[5]); 
    display.print(alt);
    // Serial.print(alt); Serial.print("|");

// ---------- TEMPC  TEMPF
    char temp_c_d[3]; 
    dtostrf(TEMP_C, 3, 0, temp_c_d);

    char temp_f_d[3]; 
    dtostrf(TEMP_F, 3, 0, temp_f_d);

    char temps[16] = "Temps 30C 67F";
    sprintf(temps, "Temps %sC %sF", temp_c_d, temp_f_d);
    display.setCursor(0, LINE[6]); 
    display.print(temps);
    // Serial.print(temps); Serial.print("|");

// ---------- HUMD  BARO
    char humd_d[3]; 
    dtostrf(HUMD, 3, 0, humd_d);

    char baro_d[5];
    dtostrf(BARO_HG, 2, 2, baro_d);

    char humdbaro[16] = "67%rh 29.93hg";
    sprintf(humdbaro, "%s%%rh %shg", humd_d, baro_d);
    display.setCursor(0, LINE[7]); 
    display.print(humdbaro);
    // Serial.print(humdbaro); Serial.print("|");

// ---------- DEWPOINT
    char dew_d[2]; 
    dtostrf(DEWPOINT, 2, 0, dew_d);

    char dew[16] = "dewpoint 67f";
    sprintf(dew, "dewpoint %sf", dew_d);
    display.setCursor(0, LINE[8]); 
    display.print(dew);
    // Serial.print(dew); Serial.print("|");

// ---------- HEAT INDEX
    char heat_d[3]; 
    dtostrf(HEATIX, 3, 0, heat_d);

    char heat[16] = "heat index 111f";
    sprintf(heat, "heat index %sf", heat_d);
    display.setCursor(0, LINE[9]);
    display.print(heat);
    // Serial.println(heat);
}

void FileWrite() {
    Serial.println("LOG");
    File data = pythonfs.open("uvlog.csv", FILE_WRITE);                 // Circuit Python File Open
      if (data) {     
    								
        data.print(DATE);                                   data.print(",");
        data.print(TIME);                                   data.print(",");       
        data.print((analogRead(VBAT_PIN) * 6.6)/1024, 1);   data.print(",");
        data.print(UVINDEX, 1);                             data.print(",");
        data.print(med());                                  data.print(",");
        data.print(ALT_M,1);                                data.print(",");
        data.print(TEMP_C,0);                               data.print(",");
        data.print(TEMP_F,0);                               data.print(",");        
        data.print(BARO_MB,2);                              data.print(",");
        data.print(HUMD,0);                                 data.print(",");
        data.print(DEWPOINT,0);                             data.print(",");   
        data.println(HEATIX,0);
        data.close();
        RECORDS++;
      
        Serial.print("FILE WRITE: ");   Serial.print(" | ");
        Serial.print(DATE);             Serial.print(" | ");
        Serial.print(TIME);             Serial.print(" | ");       
        Serial.print(VOLTS);            Serial.print(" | ");
        Serial.print(UVINDEX);          Serial.print(" | ");
        Serial.print(MINS2MED);         Serial.print(" | ");
        Serial.print(ALT_M);            Serial.print(" | ");
        Serial.print(TEMP_C);           Serial.print(" | ");
        Serial.print(TEMP_F);           Serial.print(" | ");        
        Serial.print(BARO_MB);          Serial.print(" | ");
        Serial.print(HUMD);             Serial.print(" | ");
        Serial.print(DEWPOINT);         Serial.print(" | ");   
        Serial.print(HEATIX);           Serial.print(" | ");
        Serial.println(RECORDS);
        Serial.println();     
    }
}  	
