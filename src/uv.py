import SPI
import SPIFlash
import SimpleTimer
import RTCZero
import Adafruit_SharpMem
import Adafruit_GFX
import BME280
import VEML6070


# ================ GLOBAL DECLARATIONS ================
# SPI flash chip type
FLASH_TYPE SPIFLASHTYPE_W25Q16BV
# flash chip CS pin
FLASH_SS SS1
# use SPI bus 2
FLASH_SPI_PORT SPI1
# initiate SPI flash system (hardware SPI)
SPIFlash flash(FLASH_SS, &FLASH_SPI_PORT)       
FILE_NAME 'uvlog.csv'
# buzzer pin
SPEAKER A1
# battery monitoring pin                         
VBAT_PIN A7
# button pin                         
BUTTON 5
# initiate function timers                              
SimpleTimer timer
# initiate real time clock                              
RTCZero rtc                                     
SHARP_CS 12
# 96px x 96px  16 chr x 12 lines
SharpMem display(SCK, MOSI, SHARP_CS)
# foreground color
BLACK 0
# background color                         
WHITE 1
# display size 1 character width                                 
CHAR_W 6
# display size 1 character height
CHAR_H 8
# display orientation
ROTATION 2
# margin for graphics
MARGIN 3
# initiate BME280 barometer temp humd sensor
BME280 bme
# BME280 I2C Address
BME280_ADDRESS 0x76
# sea level ref reading 1013.25 mb
SEALEVELPRESSURE_HPA (1017.8)
# initiate Adafruit VEML6070 UVB sensor
VEML6070 uv = VEML6070()

# ================ GLOBAL VARIABLES ================
# Fitzpatrick Score 0-32
int FITZ = 13
# SPF Applied 0-100 
int SPF = 30
# On Water bool
bool H2O = 0
# Snow Present bool
bool SNOW = 0
# UV Index 0.00-26.00
double UVINDEX
# Barometer (Pa) 0-1086
double BARO_MB
# Altitude (m) 0-11000
double ALT_M
# Mins to Minimal Erythemal Dose 0-inf
int MINS2MED
# Temperature degrees (Celsius)  0-50
double TEMP_C
# Temperature degrees (Fahrenheit) 0-120
double TEMP_F
# Barometer (pascals) 0-10860
double BARO_PA
# Barometer (inches mercury)
double BARO_HG
# Relative humididty (%) 0-100
double HUMD
# Dew Point (Fahrenheit) 0-100
double DEWPOINT
# Heat Index (Fahrenheit) 0-150
double HEATIX
# volts measured
double VOLTS
# Date stamp
char DATE[10]
# Time stamp
char TIME[10]
# Number of records written
int RECORDS
# line numbers ea 1 ch high
int LINE[13] = {0,8,16,24,32,40,48,56,64,72,80,88,96}


# ================ SPECIAL FUNCTIONS ================ 
# mins to Minimal Erythemal Dose (MED)
double med ()
    # UV index factoring
    # altitude factor (1.2 x meters above sea level)
    # on water UVindex factor (1.5x)
    # on snow UVindex factor (1.85x)
    # Fitzpatrick score @ 1 UV idx secs to MED
    double uvi_f = (UVINDEX * (ALT_M * 1.2)) + (UVINDEX * (H2O * 1.5)) +                   (UVINDEX * (SNOW * 1.85))                       
    
    # 5th order polynomial plot
    double s2med_b = (-3.209E-5 * pow(FITZ, 5)) + (2.959E-3 * pow(FITZ, 4)) -                     (0.103 * pow(FITZ, 3)) + (1.664* pow(FITZ, 2)) + (3.82 * FITZ) + 34.755                     
    
    # secs to MED at UVindex 1 + UV index amplifiing factors
    double s2med = ((s2med_b / uvi_f) * SPF)                                               
    # convert secs to mins
    int m2med = s2med / 60
    # max at 6 hours
    if (m2med > 480) m2med = 480
    return m2med

# Celcius to Fahrenheit
double c2f(double celsius) return ((celsius * 9) / 5) + 32
# Pascals to in. mercury
double pa2hg(double pascals) return (pascals * 0.000295333727)
# calc Dew Point
double dewPointF(double cel, int hum) double Td = (237.7 * ((17.271 * cel) / (237.7 + cel) + log(hum * 0.01))) / (17.271 - ((17.271 * cel) / (237.7 + cel) + log(hum * 0.01))) return c2f(Td)
# Heat Index
double heatIndex(double tempF, double humd_l)     
    if (tempF < 80 || humd_l < 40) double pass = tempF
    return pass
    double c1=-42.38, c2=2.049
    double c3=10.14, c4=-0.2248
    double c5=-6.838e-3, c6=-5.482e-2
    double c7=1.228e-3, c8=8.528e-4
    double c9=-1.99e-6
    double t = tempF
    double r = humd_l
    double a = ((c5 * t) + c2) * t + c1
    double b = ((c7 * t) + c4) * t + c3
    double c = ((c9 * t) + c8) * t + c6
    double rv = (c * r + b) * r + a
    return rv

char *dtostrf (double val, signed char width, unsigned char prec, char *sout)
    # convert     floats to fixed string
    # val         Your float variable;
    # width       Length of the string that will be created INCLUDING decimal point;
    # prec        Number of digits after the deimal point to print;
    # sout        Destination of output buffer
    char fmt[20]
    sprintf(fmt, "%%%d.%df", width, prec)
    sprintf(sout, fmt, val)
    return sout

# ================ SETUP ================ 
    Serial.begin(115200)                # start serial port for debugging
	pinMode(VBAT_PIN, INPUT)            # set battery pin
    pinMode(BUTTON, INPUT_PULLUP)       # set button pin

    # ---------- REAL TIME CLOCK
    print('REAL TIME CLOCK')
    # set local time variables
	byte seconds, minutes, hours
    int thour, tminute, tsecond
    int tmonth, tday, tyear
    char s_month[5]
    static const char month_names[] = 'JanFebMarAprMayJunJulAugSepOctNovDec'
    # initiate real time clock
    rtc.begin()
    # parse date out of epic
    sscanf(__DATE__, "%s %d %d", s_month, &tday, &tyear)
    # parse time out of epic
    sscanf(__TIME__, "%d:%d:%d", &thour, &tminute, &tsecond)
    # parse month out of epic
    tmonth = (strstr(month_names, s_month) - month_names) / 3
    # adjust epic year from start
    years = tyear - 2000
    # set month
    months =  tmonth + 1
    # set day
    days = tday
    # set hour
    hours = thour
    # set minute
    minutes = tminute
    # set second
    seconds = tsecond
    # set real time clock to complile time
    rtc.setTime(hours, minutes, seconds)
    # set real time clock to complile date
    rtc.setDate(days, months, years)

    # ---------- DISPLAY
    print('DISPLAY START')
    display.begin()
    display.clearDisplay()
    display.setTextSize(1)
    display.setRotation(ROTATION)
    display.setTextColor(BLACK, WHITE)

    # ---------- SPI FLASH
    print('SPI FLASH')
    # initiate file system
    if (!flash.begin(FLASH_TYPE))                    
        displayWrite  ("FLASH INIT FAIL ")
        Serial.println("FLASH INIT FAIL ")
        while(1)
    print('Flash chip JEDEC ID: 0x')
    print(flash.GetJEDECID(), HEX)
    if (!pythonfs.begin()) 
        displayWrite  ('MOUNT: FAIL     ')
        print('MOUNT: FAIL     ')
        while(1)
    displayWrite('MOUNT: SUCCESS  ')
    print('MOUNT: SUCCESS  ')
    if (pythonfs.exists('data.txt'))
        File data = pythonfs.open('data.txt', FILE_READ)
        Serial.println('Print data.txt')
        while (data.available()) char c = data.read() print(c)
        print()
    else 
        displayWrite ('FILE READ: FAIL ')
        Serial.println('FILE READ: FAIL ')

    # ---------- SET UP LOG FILE
    print('SET UP LOG FILE')
    File data = pythonfs.open('uvlog.csv', FILE_WRITE)
    if (data)
        data.print('date,')
        data.print('time,')
        data.print('batt(v),')
        data.print('uvi,')
        data.print('med(mins),')
        data.print('alt(m),')
        data.print('temp(c),')
        data.print('temp(f),')
        data.print('baro(mb),')
        data.print('humd(%),')
        data.print('dewpoint(f),')
        data.print('heat index(f),')
        data.close()

        print('date,')
        print('time,')
        print('batt(v),')
        print('uvi,')
        print('med(mins),')
        print('alt(m),')
        print('temp(c),')
        print('temp(f),')
        print('baro(mb),')
        print('baro(hg),')
        print('humd(%),')
        print('dewpoint(f),')
        print(heat index(f)'')
        displayWrite  ('HEADER: SUCCESS ')
        print('HEADER: SUCCESS ')
    else
        displayWrite('HEADER: FAIL    ')
        print('HEADER: FAIL    ')
    displayWrite('FILEOPS FINISHED')
    print('FILEOPS FINISHED')

    # ---------- SENSORS
    # initiate VEML6070 - pass int time 
    uv.begin(VEML6070_1_T)
    # initiate BME280
	if (! bme.begin(BME280_ADDRESS))                 
        displayWrite('BME280 FAIL     ')
        print('BME280 FAIL     ')
	    while (1)
    
    # write readings to SD every 20 seconds
    timer.setInterval(20000, FileWrite)
    
    # get sensor readings every 2 seconds
    timer.setInterval(2000, GetReadings)            

    # chime (x) time(s)
    chime(1)
    
    print('SETUP DONE')
# -----------------------------------------

# ================ LOOP ================
int state       = HIGH;                                 // button state
int reading;                                            // button transient state
int previous    = LOW;                                  // stored button state 
long time       = 0;                                    // button timer
long debounce   = 200;                                  // check for phantom presses (ms)

void loop() {
    timer.run();                                        // check timers
    checkButton();                                      // check for button press
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
    if (reading == HIGH && previous == 
            LOW && millis() - time > debounce) {
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


void displayWrite(String statmsg) {                     // draw screen
    header();
    body();
    display.setCursor(0, display.height()-CHAR_H);
    display.print(statmsg);
    display.refresh();
}

void header() {                                         // draw header
    double batt_l = ((analogRead(VBAT_PIN) * 6.6)/1024);
    char batt_d[4];
    dtostrf(batt_l, 2, 1, batt_d);
    char head[13];
    sprintf(head, "%02d:%02d:%02d %sv",
        rtc.getHours(),rtc.getMinutes(), rtc.getSeconds(), batt_d);
    display.setCursor(0, LINE[0]);
    display.print(head);

    char head2[16];
    sprintf(head2, "%02d/%02d  %i recs", rtc.getMonth(),rtc.getDay(), RECORDS);
    display.setCursor(0, LINE[1]);
    display.print(head2);

    # ---------- BATTERY ICON VARIABLES
    int BATTICON_WIDTH = 3 * CHAR_W                         # icon width 3 characters wide
    int BATTICON_STARTX = display.width() - BATTICON_WIDTH 
    int BATTICON_STARTY = LINE[0]                           # start y position
    int BATTICON_BARWIDTH3 = ((BATTICON_WIDTH) / 4)         # bar width as equal divs of width

    # ---------- DRAW BATTERY ICON
    display.drawLine( BATTICON_STARTX + 1, BATTICON_STARTY, BATTICON_STARTX + BATTICON_WIDTH - 4,
                      BATTICON_STARTY, BLACK)
    display.drawLine( BATTICON_STARTX, BATTICON_STARTY + 1, 
                      BATTICON_STARTX, BATTICON_STARTY + 5, BLACK)
    display.drawLine( BATTICON_STARTX + 1, BATTICON_STARTY + 6,
                      BATTICON_STARTX + BATTICON_WIDTH - 4, BATTICON_STARTY + 6, BLACK)
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3, BATTICON_STARTY + 1, BLACK)
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 2, BATTICON_STARTY + 1, BLACK)
    display.drawLine( BATTICON_STARTX + BATTICON_WIDTH - 1, BATTICON_STARTY + 2, 
                      BATTICON_STARTX + BATTICON_WIDTH - 1, BATTICON_STARTY + 4, BLACK)
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 2, BATTICON_STARTY + 5, BLACK)
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3, BATTICON_STARTY + 5, BLACK)
    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3, BATTICON_STARTY + 6, BLACK)

    # ---------- FILL BATTERY ICON
    # FULL
    if (batt_l > 4.19) {                                    
        display.fillRect(BATTICON_STARTX + 2, BATTICON_STARTY + 2, BATTICON_BARWIDTH3 * 3, 3, BLACK)
        return
    }
    # 3 bars
    if (batt_l > 4.00) {                                
        for (uint8_t i = 0; i < 3; i++) { 
            display.fillRect(BATTICON_STARTX + 2 + (i * BATTICON_BARWIDTH3),
                             BATTICON_STARTY + 2, BATTICON_BARWIDTH3 - 1, 3, BLACK)
        }
        return
    }
    # 2 bars
    if (batt_l > 3.80) {                                
        for (uint8_t i = 0; i < 2; i++) { display.fillRect(BATTICON_STARTX + 2 + 
            (i * BATTICON_BARWIDTH3), BATTICON_STARTY + 2, 
            BATTICON_BARWIDTH3 - 1, 3, BLACK)
        }
        return
    }
    # 1 bar
    if (batt_l > 3.40) { display.fillRect(BATTICON_STARTX + 2, BATTICON_STARTY + 2, 
                         BATTICON_BARWIDTH3 - 1, 3, BLACK)
    }
}

void body() {
    # ---------- UV INDEX
    char uvi_d[4]
    dtostrf(UVINDEX, 2, 1, uvi_d)
    char uvi[16]
    sprintf(uvi, "UV index %s", uvi_d)
    display.setCursor(0, LINE[2])
    display.print(uvi)

    # ---------- DRAW UVI BAR
    display.fillRect(MARGIN,                            // clear bar
                    LINE[3],
                    display.width() - (MARGIN * 2),
                    CHAR_H,
                    WHITE)

    display.drawRect(MARGIN,                            // draw clear bar
                    LINE[3], 
                    display.width() - (MARGIN * 2),
                    CHAR_H,
                    BLACK)

    display.fillRect(MARGIN,                            // fill bar w/ last UV index reading
                    LINE[3],
                    (UVINDEX * ((display.width() -      // cover 16 divisions
                            (MARGIN * 2)) / 16)),
                    CHAR_H, BLACK)


    # ---------- Mins to Minimal Erythemal Dose 
    char m2med[16];
    sprintf(m2med, "MED in %i min", MINS2MED);
    display.setCursor(0, LINE[4]); 
    display.print(m2med);


    # ---------- ALTITUDE
    char alt_d[4]
    dtostrf(ALT_M, 4, 0, alt_d)
 
    char alt[16]
    sprintf(alt, "Alt %s mtrs", alt_d)
    display.setCursor(0, LINE[5])
    display.print(alt)

    # ---------- TEMPC  TEMPF
    char temp_c_d[3]
    dtostrf(TEMP_C, 3, 0, temp_c_d)

    char temp_f_d[3]
    dtostrf(TEMP_F, 3, 0, temp_f_d)

    char temps[16]
    sprintf(temps, "Temps %sC %sF", temp_c_d, temp_f_d)
    display.setCursor(0, LINE[6])
    display.print(temps)

    # ---------- HUMD  BARO
    char humd_d[2]
    dtostrf(HUMD, 2, 0, humd_d)

    char baro_d[5]
    dtostrf(BARO_HG, 2, 2, baro_d)

    char humdbaro[16]
    sprintf(humdbaro, "%s%%rh %shg", humd_d, baro_d)
    display.setCursor(0, LINE[7])
    display.print(humdbaro)

    # ---------- DEWPOINT
    char dew_d[2]
    dtostrf(DEWPOINT, 2, 0, dew_d)
    char dew[16]
    sprintf(dew, "Dewpoint %sf", dew_d)
    display.setCursor(0, LINE[8])
    display.print(dew)

    # ---------- HEAT INDEX
    char heat_d[3]
    dtostrf(HEATIX, 3, 0, heat_d)
    char heat[16]
    sprintf(heat, "Heat Index %sf", heat_d)
    display.setCursor(0, LINE[9])
    display.print(heat)
}

# readings by timer interval
void GetReadings()                               
    # date from real time clock
    sprintf(DATE, "%02d%02d%02d", rtc.getYear(),rtc.getMonth(),rtc.getDay())
    # time from real time clock
    sprintf(TIME, "%02d%02d%02d", rtc.getHours(),rtc.getMinutes(), rtc.getSeconds())
    # VEML6070 UV reading
    UVINDEX = uv.readUV()
    # convert to UV Index
    UVINDEX /= 100.0
    # FEATHER battery voltage reading
    VOLTS = (analogRead(VBAT_PIN) * 6.6)/1024
    # BME280 temperature (Celsius) reading
    TEMP_C = bme.readTemperature()
    # convert Celsius reading to Fahrenheit
    TEMP_F = c2f(TEMP_C)
    # BME280 barometric pressure (Pascals)
    BARO_PA = bme.readPressure()
    # convert Pascals reading to millibars
    BARO_MB = BARO_PA / 100
    # convert Pascals reading to Inches Mercury
    BARO_HG = pa2hg(BARO_PA)
    # BME280 calculated altitude reading
    ALT_M = bme.readAltitude(SEALEVELPRESSURE_HPA)
    # zero altitude if under 10 meters
    if (ALT_M < 10.00) ALT_M = 0
    # BME280 relative humididty (%) reading
    HUMD = bme.readHumidity()
    # calc Dew Point
    DEWPOINT = dewPointF(TEMP_C, HUMD)
    # calc Heat Index
    HEATIX = heatIndex(TEMP_F, HUMD)
    # calc mins to Minimal Erythemal Dose (MED)
    MINS2MED = med()                                
    print('READINGS:')
    print(DATE)
    print(' | ')
    print(TIME)
    print(' | ')
    print(VOLTS)
    print(' | ')
    print(UVINDEX)
    print(' | ')
    print(MINS2MED)
    print(' | ')
    print(TEMP_C)
    print(' | ')
    print(TEMP_F)
    print(' | ')
    print(HUMD)
    print(' | ')
    print(ALT_M)
    print(' | ')
    print(BARO_PA)
    print(' | ')
    print(BARO_MB)
    print(' | ')
    print(BARO_HG)
    print(' | ')
    print(DEWPOINT)
    print(' | ')
    println(HEATIX)

FileWrite()
    File data = pythonfs.open("uvlog.csv", FILE_WRITE)
  	if (data) {	
  	    data.print(DATE)			    
        data.print(',')
    	data.print(TIME)
        data.print(',')
    	data.print(VOLTS)  	        
        data.print(',')
    	data.print(UVINDEX, 1)		    
        data.print(',')
        data.print(med(),0)            
        data.print(',')
    	data.print(ALT_M,1)		    
        data.print(',')
        data.print(TEMP_C,0)          
        data.print(',')
        data.print(TEMP_F,0)          
        data.print(',')
        data.print(BARO_MB,0)          
        data.print(',')
        data.print(HUMD,0)             
        data.print(',')
        data.print(DEWPOINT,0)         
        data.print(',')
        data.println(HEATIX,0)
    	data.close()

        print('FILE WRITE: ')
        print(DATE)
        print(' | ')
        print(TIME)
        print(' | ')
        print(VOLTS)
        print(' | ')
        print(UVINDEX)
        print(' | ')
        print(MINS2MED)
        print(' | ')
        print(ALT_M)
        print(' | ')
        print(TEMP_C)
        print(' | ')
        print(TEMP_F)
        print(' | ')
        print(BARO_MB)
        print(' | ')
        print(HUMD)
        print(' | ')
        print(DEWPOINT)
        print(' | ')
        print(HEATIX)
        print(' | ')
        println(RECORDS)
		RECORDS++
