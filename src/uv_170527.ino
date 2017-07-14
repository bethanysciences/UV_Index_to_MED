#include "SPI.h"
#include "Adafruit_SPIFlash.h"
#include "Adafruit_SPIFlash_FatFs.h"
#include "SimpleTimer.h"
#include "RTCZero.h"
#include "Adafruit_SharpMem.h"
#include "Adafruit_GFX.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"
#include "Adafruit_VEML6070.h"

#define FLASH_TYPE      SPIFLASHTYPE_W25Q16BV
#define FLASH_SS        SS1                    			// Flash chip SS pin.
#define FLASH_SPI_PORT  SPI1                   			// Flash SPI port
Adafruit_SPIFlash flash(FLASH_SS, &FLASH_SPI_PORT);     // Use hardware SPI
// Adafruit_W25Q16BV_FatFs fatfs(flash);                  // FAT file system
Adafruit_M0_Express_CircuitPython pythonfs(flash);      // CirPi File ystem
#define FILE_NAME		"uvlog.csv"

#define SPEAKER			A1
#define VBAT_PIN		A7
#define BUTTON     		5

SimpleTimer timer;
RTCZero rtc;

#define SHARP_CS 		12
Adafruit_SharpMem display(SCK, MOSI, SHARP_CS); // 96 x 96  16 chr x 12 lines
#define BLACK           0
#define TEXT_COLOR      0
#define WHITE           1
#define CHAR_W          6
#define CHAR_H          8
#define ROTATION        2

#define BME280_ADDRESS 			0x76
#define SEALEVELPRESSURE_HPA 	(1017.8)      // sea level ref reading 1013.25 mb 
Adafruit_BME280 bme;

Adafruit_VEML6070 uv = Adafruit_VEML6070();

char DATE[10]    = "171222";
char TIME[10]    = "231259";
int FITZ          = 13;                         // Fitzpatrick Score 0-32
int SPF           = 30;                         // SPF Applied       0-100
bool H2O          = 0;                          // On Water          bool
bool SNOW         = 0;                          // Snow Present      bool 
double UVINDEX    = 6.00;                       // UV Index          0.00-26.00
double BARO_MB    = 3000.00;                    // Barometer (Pa)    0-1,086
double ALT_M      = 1000.00;                    // Altitude (m)      0-11,000
double TEMP_C; 
double TEMP_F;
double BARO_PA;
double BARO_HG;     
double HUMD; 
double DEWPOINT;
double HEATIX;
double VOLTS      = 5.1;
int RECORDS       = 0;                          // Number of records written

double med () {   
    double uvi_f =  (UVINDEX * (H2O * 1.5)) +
                    (UVINDEX * (SNOW * 1.85)) +
                    (UVINDEX * (ALT_M * 1.2));                   
    double t2b_b =  (-3.209E-5 * pow(FITZ, 5)) +
                    (2.959E-3 * pow(FITZ, 4)) -
                    (0.103 * pow(FITZ, 3)) +
                    (1.664* pow(FITZ, 2)) +
                    (3.82 * FITZ) + 
                    34.755;
    return ((t2b_b / uvi_f) * SPF) / 60 ;       // convert seconds to minutes
}
double c2f(double celsius) { 
    return ((celsius * 9) / 5) + 32; 
}
double pa2hg(double pascals) { 
    return (pascals * 0.000295333727); 
}
double dewPointF(double cel, int hum) {
    double Td = (237.7 * ((17.271 * cel) / (237.7 + cel) + log(hum * 0.01))) / 
                (17.271 - ((17.271 * cel) / (237.7 + cel) + log(hum * 0.01)));
    return c2f(Td);
}
double heatIndex(double tempF, double humidity) {
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
char *dtostrf (double val, signed char width, unsigned char prec, char *sout) { 
// val      Your float variable;
// width    Length of the string that will be created INCLUDING decimal point;
// prec     Number of digits after the deimal point to print;
// sout     Destination of output buffer
    char fmt[20];
    sprintf(fmt, "%%%d.%df", width, prec);
    sprintf(sout, fmt, val);
    return sout;
}

void setup() {
    Serial.begin(115200);
	pinMode(VBAT_PIN, INPUT);
    pinMode(BUTTON, INPUT_PULLUP);  
	byte seconds, minutes, hours, days, months, years;
    char s_month[5];
    int tmonth, tday, tyear, thour, tminute, tsecond;
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    rtc.begin();    
    sscanf(__DATE__, "%s %d %d", s_month, &tday, &tyear);
    sscanf(__TIME__, "%d:%d:%d", &thour, &tminute, &tsecond);
    tmonth = (strstr(month_names, s_month) - month_names) / 3;
    years = tyear - 2000; months = tmonth + 1; days = tday; 
    hours = thour; minutes = tminute; seconds = tsecond;
    rtc.setTime(hours, minutes, seconds); 
    rtc.setDate(days, months, years);
    
    display.begin();
    display.clearDisplay();
    display.setTextSize(1);
    display.setRotation(ROTATION);
    display.setTextColor(BLACK, WHITE);

//---------
    if (!flash.begin(FLASH_TYPE)) {
        displayWrite  ("FLASH INIT FAIL ");
        Serial.println("FLASH INIT FAIL ");
        while(1);
    }
    Serial.print("Flash chip JEDEC ID: 0x"); Serial.println(flash.GetJEDECID(), HEX);
    
    if (!pythonfs.begin()) {                          // mount circuitpython filesystem
        displayWrite  ("MOUNT: FAIL     ");
        Serial.println("MOUNT: FAIL     ");
        while(1);
    }
    displayWrite("MOUNT: SUCCESS  ");
    Serial.println("MOUNT: SUCCESS  ");
    if (pythonfs.exists("data.txt")) {
        File data = pythonfs.open("data.txt", FILE_READ);
        Serial.println("Print data.txt");
        while (data.available()) {
            char c = data.read();
            Serial.print(c);
        }
        Serial.println();
    }
    else {
        displayWrite  ("FILE READ: FAIL ");
        Serial.println("FILE READ: FAIL ");
    }
    File data = pythonfs.open("uvlog.csv", FILE_WRITE);         // CirPi File Open       
    if (data) {                                                 // Write header
        data.print("date,");
        data.print("time,");      
        data.print("batt(v),");
        data.print("uvi,");
        data.print("med(mins),");
        data.print("alt(m),");
        data.print("temp(c),");
        data.print("temp(f),");
        data.print("baro(mb),");
        data.print("humd(%),");
        data.print("dewpoint(f),");  
        data.println("heat index(f),");
        data.close();
        displayWrite  ("HEADER: SUCCESS ");
        Serial.println("HEADER: SUCCESS ");
    }
    else {
        displayWrite  ("HEADER: FAIL    ");
        Serial.println("HEADER: FAIL    ");
    }
    displayWrite  ("FILEOPS FINISHED");
    Serial.println("FILEOPS FINISHED");
//---------
   
    uv.begin(VEML6070_1_T);                             // pass int time constant
    
	if (! bme.begin(BME280_ADDRESS)) {                  // spark up BME280
        displayWrite  ("BME280 FAIL     ");
        Serial.println("BME280 FAIL     ");
	    while (1);  
    }
	
    timer.setInterval(20000, FileWrite);                // write readings to SD every 2 seconds
    timer.setInterval(2000, GetReadings);               // get sensor readings every 2 seconds
    chime(1);
}

int state       = HIGH;
int reading;
int previous    = LOW;
long time       = 0;
long debounce   = 200;

void loop() {
    timer.run();                                        // Runs timed events
    checkButton();
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

void checkButton() {                                    // button handling
    reading = digitalRead(BUTTON);    
    if (reading == HIGH && previous == LOW && millis() - time > debounce) {    
        if (state == HIGH) state = LOW; 
        else state = HIGH; 
        time = millis();    
    }   
    previous = reading; 
    if (state == HIGH) { 
        displayWrite("BUTTON PRESSED  "); 
    }
    else { 
        displayWrite("MAIN SCREEN     ");
    }
}

void displayWrite(String statmsg) {
    // if (statmsg != "") {display.setCursor(0, display.height()-CHAR_H); display.print(statmsg); display.refresh(); return; }
    header(((analogRead(VBAT_PIN) * 6.6)/1024));
    body();
    display.setCursor(0, display.height()-CHAR_H); display.print(statmsg);
    display.refresh();
}

void header(float _battery) {
    char batt_d[10] ; 
    dtostrf(_battery, 3, 1, batt_d);                     // convert float to 1 dec place 
    char head[15] = "00:00:00 5.5v";
    sprintf(head, "%02d:%02d:%02d %sv", rtc.getHours(),rtc.getMinutes(),rtc.getSeconds(), batt_d);
    display.setCursor(0, 0); display.print(head);
    char head2[16] = "12/22 9000 recs";
    sprintf(head2, "%02d/%02d  %i recs", rtc.getMonth(),rtc.getDay(), RECORDS);
    display.setCursor(0, (1 * CHAR_H) + 1); display.print(head2);
        
    int BATTTEXT_WIDTH      = 4 * CHAR_W;
    int BATTICON_WIDTH      = 3 * CHAR_W;
    int BATTTEXT_STARTX     = display.width() - (BATTTEXT_WIDTH + BATTICON_WIDTH);
    int BATTTEXT_STARTY     = 0;
    int BATTICON_STARTX     = display.width() - BATTICON_WIDTH;
    int BATTICON_STARTY     = 0;
    int BATTICON_BARWIDTH3  = ((BATTICON_WIDTH) / 4);
    
    display.setCursor(BATTTEXT_STARTX, BATTTEXT_STARTY);
    display.drawLine( BATTICON_STARTX + 1, BATTICON_STARTY,
                      BATTICON_STARTX + BATTICON_WIDTH - 4,
                      BATTICON_STARTY, TEXT_COLOR);
    display.drawLine( BATTICON_STARTX, BATTICON_STARTY + 1,
                      BATTICON_STARTX, BATTICON_STARTY + 5, TEXT_COLOR);
    display.drawLine( BATTICON_STARTX + 1, BATTICON_STARTY + 6,
                      BATTICON_STARTX + BATTICON_WIDTH - 4,
                      BATTICON_STARTY + 6, TEXT_COLOR);
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 1, TEXT_COLOR);
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 2,
                      BATTICON_STARTY + 1, TEXT_COLOR);
    display.drawLine( BATTICON_STARTX + BATTICON_WIDTH - 1,
                      BATTICON_STARTY + 2, BATTICON_STARTX + 
                      BATTICON_WIDTH - 1, BATTICON_STARTY + 4, TEXT_COLOR);
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 2,
                      BATTICON_STARTY + 5, TEXT_COLOR);
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 5, TEXT_COLOR);
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 6, TEXT_COLOR);
                           
    if (_battery > 4.19) {                                  // FULL
        display.fillRect(BATTICON_STARTX + 2,               // x
                         BATTICON_STARTY + 2,               // y
                         BATTICON_BARWIDTH3 * 3,            // w
                         3,                                 // h
                         TEXT_COLOR);                       // color
        return;      
    }
    
    if (_battery > 4.00) {                                  // 3 bars
        for (uint8_t i = 0; i < 3; i++) {                   // 3 bars
            display.fillRect(BATTICON_STARTX + 2 + (i * BATTICON_BARWIDTH3),
                             BATTICON_STARTY + 2,
                             BATTICON_BARWIDTH3 - 1,
                             3, 
                             TEXT_COLOR);
        }
        return;
    }
    if (_battery > 3.80) {                                  // 2 bars
        for (uint8_t i = 0; i < 2; i++) {                   
            display.fillRect(BATTICON_STARTX + 2 + (i * BATTICON_BARWIDTH3),
                             BATTICON_STARTY + 2, 
                             BATTICON_BARWIDTH3 - 1, 
                             3, 
                             TEXT_COLOR);
        }
        return;
    }
    if (_battery > 3.40) {                                 // 1 bar
            display.fillRect(BATTICON_STARTX + 2,
                             BATTICON_STARTY + 2, 
                             BATTICON_BARWIDTH3 - 1, 
                             3, 
                             TEXT_COLOR);                      
    }
}

void body() { 
    // display.height() display.width(), CHAR_H, CHAR_H
    
    // ----------- UVI    
    display.fillRect(0, (2 * CHAR_H) + 1, (UVINDEX * (display.width() / 12)) + 3, 8, TEXT_COLOR);   // UV index bar
    char uvi_d[10]; 
    dtostrf(UVINDEX, 3, 1, uvi_d);                       // convert float to 1 dec place 
    char uvi[] = "UVi 5.5";
    sprintf(uvi, "UVi %s", uvi_d);
    display.setCursor(0, (3 * CHAR_H) + 1); display.print(uvi);

    // ----------- Minimal Erythemal Dose
    char med_d[10];
    dtostrf(med(), 4, 0, med_d);                          // drop decimal on minutes
    char med[] = "MED 555 mins";
    sprintf(med, "MED %s mins", med_d);
    display.setCursor(0, (4 * CHAR_H) + 1); display.print(med);
    
    // ----------- ALTITUDE
    char alt_m_d[10]; 
    dtostrf(ALT_M, 5, 0, alt_m_d);                        // drop decimal on meters
    char alt_mS[20] = "Alt 5500 meters";
    sprintf(alt_mS, "Alt %s meters", alt_m_d);
    display.setCursor(0, (5 * CHAR_H) + 1); display.print(alt_mS);

   // ----------- TEMPC  TEMPF
    char temp_c_d[10]; 
    dtostrf(TEMP_C, 3, 0, temp_c_d);                        // drop decimal
    char temp_f_d[10]; 
    dtostrf(TEMP_F, 3, 0, temp_f_d);                        // drop decimal
    char temps[16] = "Temps 30C 67F";
    sprintf(temps, "Temps %sC %sF", temp_c_d, temp_f_d);
    display.setCursor(0, (6 * CHAR_H) + 1); display.print(temps);

   // ----------- HUMD  BARO
    char humd_d[10]; 
    dtostrf(HUMD, 3, 0, humd_d);                            // drop decimal
    char baro_pa_d[10]; 
    dtostrf(BARO_PA, 2, 2, baro_pa_d);                      // make 2 decimal places
    char humdbaro[16] = "67%rh 29.93hg";
    sprintf(humdbaro, "%srh %shg", humd_d, baro_pa_d);
    display.setCursor(0, (7 * CHAR_H) + 1); display.print(humdbaro);

   // ----------- DEWPOINT
    char dewpoint_d[10]; 
    dtostrf(DEWPOINT, 2, 0, dewpoint_d);                    // drop decimal
    char dew[16] = "dewpoint 67f";
    sprintf(dew, "dewpoint %sf", dewpoint_d);
    display.setCursor(0, (8 * CHAR_H) + 1); display.print(dew);

   // ----------- HEAT INDEX
    char heatix_d[10]; 
    dtostrf(HEATIX, 3, 0, heatix_d);                            // drop decimal
    char heat[16] = "heat index 111f";
    sprintf(heat, "heat index %sf", heatix_d);
    display.setCursor(0, (9 * CHAR_H) + 1); display.print(heat);
}

void GetReadings() {
    sprintf(DATE, "%02d%02d%02d", rtc.getYear(),rtc.getMonth(),rtc.getDay());
    sprintf(TIME, "%02d%02d%02d", rtc.getHours(),rtc.getMinutes(), rtc.getSeconds()); 
    UVINDEX     = uv.readUV();
    UVINDEX     /= 100.0;
    VOLTS       = (analogRead(VBAT_PIN) * 6.6)/1024;
    TEMP_C      = bme.readTemperature();
    TEMP_F      = c2f(TEMP_C);
    BARO_PA     = bme.readPressure();
    BARO_MB     = BARO_PA / 100;
    BARO_HG     = pa2hg(BARO_PA);
    if (bme.readAltitude(SEALEVELPRESSURE_HPA) > 0) ALT_M = bme.readAltitude(SEALEVELPRESSURE_HPA);
    else ALT_M  = 0;    
    HUMD        = bme.readHumidity();
    DEWPOINT    = dewPointF(TEMP_C, HUMD);
    HEATIX      = heatIndex(TEMP_F, HUMD);
}

void FileWrite() {
    File data = pythonfs.open("uvlog.csv", FILE_WRITE);
  	if (data) {												
  	    data.print(DATE);									data.print(",");
    	data.print(TIME);									data.print(",");       
    	data.print((analogRead(VBAT_PIN) * 6.6)/1024, 1);  	data.print(",");
    	data.print(UVINDEX, 1);							    data.print(",");
        data.print(med());                                  data.print(",");
    	data.print(ALT_M,1);			                    data.print(",");
        data.print(TEMP_C,0);                               data.print(",");
        data.print(TEMP_F,0);                               data.print(",");        
        data.print(BARO_MB,2);                              data.print(",");
        data.print(HUMD,0);                                 data.print(",");
        data.print(DEWPOINT,0);                             data.print(",");   
        data.println(HEATIX,0);
    	data.close();
		RECORDS++;
  	}
}  	
