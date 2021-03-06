#include "application.h"

// Start up in SEMI_AUTOMATIC mode to be able to display
// rainbows while trying to connect to wifi.
SYSTEM_MODE(SEMI_AUTOMATIC);


// NeoPixel init stuffs
#include "neopixel.h"
#define PIXEL_COUNT 121
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, D0, WS2812B);


// Timers
#include "elapsedMillis.h"
elapsedMillis timerEffect;
uint32_t intervalEffect;

elapsedMillis timerConnect;
static const uint32_t intervalConnect = 15000;

elapsedMillis timerBootDelay = 0;
static const uint32_t intervalBootDelay = 10000;

elapsedMillis timerUpdateTime = 0;
static const uint32_t intervalUpdateTime = 86400000;


// Function prototypes
void doWord(const uint8_t *w);
void undoWord(const uint8_t *w);
void randomColor();
void blackOut();
void displayDigit(uint8_t d, byte n);
void displayDigit(uint8_t d, byte n, uint8_t c[3]);
void rainbow(uint8_t wait);
uint32_t Wheel(byte WheelPos);
void doTime();
void displayDefaultText();
void displayHour();
void displayMinute();


// Color holder
uint8_t color[3] = {0, 0, 64};


// Word pixels
static const uint8_t wordIt[8] = {119, 120};
static const uint8_t wordIs[8] = {116, 117};
static const uint8_t wordHalf[8] = {114, 113, 112, 111};

static const uint8_t wordQuarter[8] = {109, 108, 107, 106, 105, 104, 103};
static const uint8_t wordTen[8] = {101, 100, 99};

static const uint8_t wordTwenty[8] = {98, 97, 96, 95, 94, 93};
static const uint8_t wordFive[8] = {91, 90, 89, 88};

static const uint8_t wordTo[8] = {86, 85};
static const uint8_t wordMinutes[8] = {83, 82, 81, 80, 79, 78, 77};

static const uint8_t wordPast[8] = {75, 74, 73, 72};
static const uint8_t wordSEVEN[8] = {70, 69, 68, 67, 66};

static const uint8_t wordNINE[8] = {64, 63, 62, 61};
static const uint8_t wordFIVE[8] = {59, 58, 57, 56};

static const uint8_t wordTHREE[8] = {53, 52, 51, 50, 49};
static const uint8_t wordSIX[8] = {47, 46, 45};

static const uint8_t wordEIGHT[8] = {37, 36, 35, 34, 33};
static const uint8_t wordFOUR[8] = {42, 41, 40, 39};

static const uint8_t wordTWELVE[8] = {31, 30, 29, 28, 27, 26};
static const uint8_t wordONE[8] = {24, 23, 22};

static const uint8_t wordELEVEN[8] = {16, 15, 14, 13, 12, 11};
static const uint8_t wordTWO[8] = {20, 19, 18};

static const uint8_t wordTEN[8] = {10, 9, 8};
static const uint8_t wordOClock[8] = {6, 5, 4, 3, 2, 1, 0};


// EEPROM
// Address 0 = 117 if values have been saved
// Address 1 = 0/1 for -/+ of time zone offset
// Address 2 = Time zone offset (positive integer)
// Address 3 = 12/24 for hour format
// Address 4 = Effect mode
// Address 5 = Red
// Address 6 = Green
// Address 7 = Blue
// Address 8 = Rainbow delay

int8_t timeZone = 0;
bool time12Hour = false;
bool resetFlag = false;
elapsedMillis timerReset = 0;


// Effect modes
// 0 = no effect
// 1 = rainbow
uint8_t EFFECT_MODE = 0;
uint8_t LAST_EFFECT_MODE = EFFECT_MODE;
uint8_t currEffect = EFFECT_MODE;
uint16_t RAINBOW_DELAY = 50;
uint8_t LAST_MINUTE = 0;


void setup() {
    // Dim the onboard LED so it's less distracting
    RGB.control(true);
    RGB.brightness(128);
    RGB.control(false);


    // Initialize NeoPixels
    strip.begin();
    strip.show();


    // Connect to the cloud
    Spark.connect();
    while(!Spark.connected()) {
        // And do a little rainbow dance while we wait
        rainbow(10);
        delay(10);
        Spark.process();
    }


    // Set the timer to sync the time immediately
    timerUpdateTime = intervalUpdateTime;


    // Do a "wipe" to clear away the rainbow
    for(uint8_t i=0; i<PIXEL_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255));

        if(i>0)
            strip.setPixelColor(i-1, strip.Color(0, 0, 0));

        strip.show();
        delay(10);
    }


    // Declare remote function
    Spark.function("function", fnRouter);


    // See if this EEPROM has saved data
    if(EEPROM.read(0)==117) {
        // Set the time zone
        if(EEPROM.read(1)==0)
            timeZone = EEPROM.read(2)*-1;
        else
            timeZone = EEPROM.read(2);

        // Set the hour format
        if(EEPROM.read(3)==12)
            time12Hour = true;
        else
            time12Hour = false;

        EFFECT_MODE = EEPROM.read(4);
        LAST_EFFECT_MODE = EFFECT_MODE;

        color[0] = EEPROM.read(5);
        color[1] = EEPROM.read(6);
        color[2] = EEPROM.read(7);

        RAINBOW_DELAY = EEPROM.read(8);

    // If data has not been saved, "initialize" the EEPROM
    } else {
        // Initialize
        EEPROM.write(0, 117);
        // Time zone +/-
        EEPROM.write(1, 0);
        // Time zone
        EEPROM.write(2, 0);
        // Hour format
        EEPROM.write(3, 24);
        // Effect mode
        EEPROM.write(4, 0);
        // Red
        EEPROM.write(5, 0);
        // Green
        EEPROM.write(6, 255);
        // Blue
        EEPROM.write(7, 128);
        // Rainbow delay
        EEPROM.write(8, RAINBOW_DELAY);
    }

    // Set the timezone
    Time.zone(timeZone);


    // Blank slate
    blackOut();
    strip.show();
}


void loop() {
    // Process effects
    doEffectMode();


    // If we issued a remote reboot command, handle it after a small delay
    // (It doesn't seem to work very well otherwise)
    if(timerReset>=500) {
        if(resetFlag) {
            System.reset();
            resetFlag = false;
        }

        timerReset = 0;
    }


    // Try to connect to the cloud if we aren't connected, and
    // we've waited for a reasonable delay
    if(!Spark.connected() && timerConnect>=intervalConnect) {
        Spark.connect();
        delay(10);
        Spark.process();

        timerConnect = 0;

    // If we're connected, handle background processes
    } else if(Spark.connected())
        Spark.process();


    // If we're connected and waited long enough, sync the time
    if(Spark.connected() && timerUpdateTime>=intervalUpdateTime) {
        Spark.syncTime();
        timerUpdateTime = 0;
    }
}


// Cloud API function
int fnRouter(String command) {
    command.trim();
    command.toUpperCase();

    // Get time zone offset
    if(command.equals("GETTIMEZONE")) {
        return timeZone;


    // Set time zone offset
    } else if(command.substring(0, 12)=="SETTIMEZONE,") {
        timeZone = command.substring(12).toInt();
        Time.zone(timeZone);

        if(timeZone>-1) {
            EEPROM.write(1, 1);
            EEPROM.write(2, timeZone);
        } else {
            EEPROM.write(1, 0);
            EEPROM.write(2, timeZone * -1);
        }

        intervalEffect = 0;
        blackOut();
        doEffectMode();

        return timeZone;


    // Lazy way to reboot
    } else if(command.equals("REBOOT")) {
        resetFlag = true;
        return 1;


    // Set red
    } else if(command.substring(0, 7)=="SETRED,") {
        color[0] = command.substring(7).toInt();
        EEPROM.write(5, color[0]);
        intervalEffect = 0;

        return color[0];


    // Set green
    } else if(command.substring(0, 9)=="SETGREEN,") {
        color[1] = command.substring(9).toInt();
        EEPROM.write(6, color[1]);
        intervalEffect = 0;

        return color[1];


    // Set blue
    } else if(command.substring(0, 8)=="SETBLUE,") {
        color[2] = command.substring(8).toInt();
        EEPROM.write(7, color[2]);
        intervalEffect = 0;

        return color[2];


    // Set RGB
    } else if(command.substring(0, 7)=="SETRGB,") {
        color[0] = command.substring(7, 10).toInt();
        color[1] = command.substring(11, 14).toInt();
        color[2] = command.substring(15, 18).toInt();

        EEPROM.write(5, color[0]);
        EEPROM.write(6, color[1]);
        EEPROM.write(7, color[2]);

        intervalEffect = 0;

        return 1;


    // Random color
    } else if(command.equals("RANDOMCOLOR")) {
        randomColor();
        intervalEffect = 0;

        return 1;


    // Set effect mode
    } else if(command.substring(0, 10)=="SETEFFECT,") {
        EFFECT_MODE = command.substring(10).toInt();
        EEPROM.write(4, EFFECT_MODE);
        intervalEffect = 0;

        return EFFECT_MODE;


    // Get effect mode
    } else if(command.equals("GETEFFECTMODE")) {
        return EFFECT_MODE;


    // Get pixel color
    } else if(command.substring(0, 14)=="GETPIXELCOLOR,") {
        return strip.getPixelColor(command.substring(14).toInt());


    // Set rainbow effect delay
    } else if(command.substring(0, 16)=="SETRAINBOWDELAY,") {
        RAINBOW_DELAY = command.substring(16).toInt();
        intervalEffect = RAINBOW_DELAY;
        EEPROM.write(8, RAINBOW_DELAY);
        return RAINBOW_DELAY;


    // Get rainbow effect delay
    } else if(command.equals("GETRAINBOWDELAY")) {
        return RAINBOW_DELAY;


    // Turn on one pixel
    } else if(command.substring(0, 8)=="PIXELON,") {
        uint8_t pixel = command.substring(8).toInt();
        strip.setPixelColor(pixel, strip.Color(color[0], color[1], color[2]));
        strip.show();

        return pixel;


    // Turn off one pixel
    } else if(command.substring(0, 9)=="PIXELOFF,") {
        uint8_t pixel = command.substring(9).toInt();
        strip.setPixelColor(pixel, strip.Color(0, 0, 0));
        strip.show();

        return pixel;


    // Display a word
    } else if(command.substring(0,7)=="DOWORD,") {
        String w = command.substring(7);
        if(w.equals("IT"))
            doWord(wordIt);
        else if(w.equals("IS"))
            doWord(wordIs);
        else if(w.equals("TEN"))
            doWord(wordTen);
        else if(w.equals("HALF"))
            doWord(wordHalf);
        else if(w.equals("QUARTER"))
            doWord(wordQuarter);
        else if(w.equals("TWENTY"))
            doWord(wordTwenty);
        else if(w.equals("FIVE"))
            doWord(wordFive);
        else if(w.equals("MINUTES"))
            doWord(wordMinutes);
        else if(w.equals("PAST"))
            doWord(wordPast);
        else if(w.equals("TO"))
            doWord(wordTo);
        else if(w.equals("SEVEN"))
            doWord(wordSEVEN);
        else if(w.equals("ELEVEN"))
            doWord(wordELEVEN);
        else if(w.equals("NINE"))
            doWord(wordNINE);
        else if(w.equals("SIX"))
            doWord(wordSIX);
        else if(w.equals("TWO"))
            doWord(wordTWO);
        else if(w.equals("ONE"))
            doWord(wordONE);
        else if(w.equals("EIGHT"))
            doWord(wordEIGHT);
        else if(w.equals("THREE"))
            doWord(wordTHREE);
        else if(w.equals("FIVE2"))
            doWord(wordFIVE);
        else if(w.equals("FOUR"))
            doWord(wordFOUR);
        else if(w.equals("TEN2"))
            doWord(wordTEN);
        else if(w.equals("TWELVE"))
            doWord(wordTWELVE);
        else if(w.equals("OCLOCK") || w.equals("O'CLOCK"))
            doWord(wordOClock);

        strip.show();

        return command.length()-7;


    // Un-display a word
    } else if(command.substring(0,9)=="UNDOWORD,") {
        String w = command.substring(9);
        if(w.equals("IT"))
            undoWord(wordIt);
        else if(w.equals("IS"))
            undoWord(wordIs);
        else if(w.equals("TEN"))
            undoWord(wordTen);
        else if(w.equals("HALF"))
            undoWord(wordHalf);
        else if(w.equals("QUARTER"))
            undoWord(wordQuarter);
        else if(w.equals("TWENTY"))
            undoWord(wordTwenty);
        else if(w.equals("FIVE"))
            undoWord(wordFive);
        else if(w.equals("MINUTES"))
            undoWord(wordMinutes);
        else if(w.equals("PAST"))
            undoWord(wordPast);
        else if(w.equals("TO"))
            undoWord(wordTo);
        else if(w.equals("SEVEN"))
            undoWord(wordSEVEN);
        else if(w.equals("ELEVEN"))
            undoWord(wordELEVEN);
        else if(w.equals("NINE"))
            undoWord(wordNINE);
        else if(w.equals("SIX"))
            undoWord(wordSIX);
        else if(w.equals("TWO"))
            undoWord(wordTWO);
        else if(w.equals("ONE"))
            undoWord(wordONE);
        else if(w.equals("EIGHT"))
            undoWord(wordEIGHT);
        else if(w.equals("THREE"))
            undoWord(wordTHREE);
        else if(w.equals("FIVE2"))
            undoWord(wordFIVE);
        else if(w.equals("FOUR"))
            undoWord(wordFOUR);
        else if(w.equals("TEN2"))
            undoWord(wordTEN);
        else if(w.equals("TWELVE"))
            undoWord(wordTWELVE);
        else if(w.equals("OCLOCK") || w.equals("O'CLOCK"))
            undoWord(wordOClock);

        strip.show();

        return command.length()-9;

    // Return the Unix epoch time as the clock knows it
    } else if(command.equals("GETTIME"))
        return Time.now();


    return -1;
}


// Generate a random color
void randomColor() {
    color[0] = random(32, 255);
    color[1] = random(32, 255);
    color[1] = random(32, 255);
}


// Turn off all pixels
void blackOut() {
    // Black it out
    for(uint8_t x=0; x<PIXEL_COUNT; x++)
        strip.setPixelColor(x, strip.Color(0, 0, 0));
}


// Display a word
void doWord(const uint8_t *w) {
    for(uint8_t i=0; i<sizeof(w)*2; i++)
        strip.setPixelColor(w[i], strip.Color(color[0], color[1], color[2]));
}


// Turn off a word
void undoWord(const uint8_t *w) {
    for(uint8_t i=0; i<sizeof(w)*2; i++)
        strip.setPixelColor(w[i], strip.Color(0, 0, 0));
}


// Display the rainbow
void rainbow(uint8_t wait) {
    uint16_t i, j;

    for(j=0; j<256; j++) {
        for(i=0; i<strip.numPixels(); i++) {
            strip.setPixelColor(i, Wheel((i+j) & 255));
        }

        strip.show();
        delay(wait);
    }
}


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    if(WheelPos < 85) {
        return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    } else if(WheelPos < 170) {
        WheelPos -= 85;
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    } else {
        WheelPos -= 170;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
}


void doTime() {
    blackOut();

    displayDefaultText();
    displayHour();
    displayMinute();

    strip.show();
}


// Display the words that are always displayed: "It is ... o'clock"
void displayDefaultText() {
    // it
    doWord(wordIt);

    // is
    doWord(wordIs);

    // o'clock
    doWord(wordOClock);
}


// Calculate and display the hour
void displayHour() {
    uint8_t h = Time.hourFormat12();
    uint8_t m = Time.minute();

    // Past / to
    if(m>=4 && m<=30)
        doWord(wordPast);
    else if(m>=31 && m<=58) {
        h++;

        doWord(wordTo);
    }

    if(h>12)
        h = 1;


    // hour
    switch(h) {
        case 1:
            doWord(wordONE);
            break;
        case 2:
            doWord(wordTWO);
            break;
        case 3:
            doWord(wordTHREE);
            break;
        case 4:
            doWord(wordFOUR);
            break;
        case 5:
            doWord(wordFIVE);
            break;
        case 6:
            doWord(wordSIX);
            break;
        case 7:
            doWord(wordSEVEN);
            break;
        case 8:
            doWord(wordEIGHT);
            break;
        case 9:
            doWord(wordNINE);
            break;
        case 10:
            doWord(wordTEN);
            break;
        case 11:
            doWord(wordELEVEN);
            break;
        case 12:
            doWord(wordTWELVE);
            break;
    }
}


// Display the minute offset ("rounded" to 5-minute intervals)
void displayMinute() {
    uint8_t m = Time.minute();

    // Some times don't need "minutes" (half, quarter)
    bool isMinutes = true;


    // Five minutes
    if(m<=3) {
        isMinutes = false;

    } else if(m>=4 && m<=8) {
        doWord(wordFive);

    // Ten minutes
    } else if(m>=9 && m<=12) {
        doWord(wordTen);

    // Quarter
    } else if(m>=13 && m<=18) {
        doWord(wordQuarter);
        isMinutes = false;

    // Twenty
    } else if(m>=19 && m<=22) {
        doWord(wordTwenty);

    // Twenty five
    } else if (m>=23 && m<=27) {
        doWord(wordTwenty);
        doWord(wordFive);

    // Half past
    } else if(m>=28 && m<=32) {
        doWord(wordHalf);
        isMinutes = false;

    // Twenty five
    } else if(m>=33 && m<=37) {
        doWord(wordTwenty);
        doWord(wordFive);

    // Twenty
    } else if(m>=38 && m<=42) {
        doWord(wordTwenty);

    // Quarter
    } else if(m>=41 && m<=47) {
        doWord(wordQuarter);
        isMinutes = false;

    // Ten minutes
    } else if(m>=48 && m<=52) {
        doWord(wordTen);

    // Five minutes
    } else if(m>=53 && m<=58) {
        doWord(wordFive);

    } else if(m>=59)
        isMinutes = false;


    if(isMinutes)
        doWord(wordMinutes);
}


// Process the current effect mode
void doEffectMode() {
    // If the effect mode has changed since we last did an effect,
    // wipe the slate.
    if(EFFECT_MODE!=LAST_EFFECT_MODE) {
        blackOut();
        LAST_EFFECT_MODE = EFFECT_MODE;
        timerEffect = intervalEffect;
    }


    // Update the effect if the appropriate interval has been reached
    if(timerEffect>=intervalEffect) {
        timerEffect = 0;

        switch(EFFECT_MODE) {
            case 1: // Rainbow
                doEffectRainbow();
                intervalEffect = RAINBOW_DELAY;
                break;

            case 0: // Time
            default:
                doTime();
                intervalEffect = 1000;
        }
    }
}


// The rainbow effect
void doEffectRainbow() {
    uint16_t i, j;


    // Wipe the slate if the minute has changed
    if(Time.minute()!=LAST_MINUTE) {
        blackOut();
        LAST_MINUTE = Time.minute();
    }


    // "Display" the words
    displayDefaultText();
    displayHour();
    displayMinute();


    // Loop through each pixel and give it a rainbow color if the pixel is
    // displayed as part of a word.
    for(j=0; j<256; j++) {
        if(EFFECT_MODE!=1) break;

        for(i=0; i<strip.numPixels(); i++) {
            if(strip.getPixelColor(i)>0)
                strip.setPixelColor(i, Wheel((i+j) & 255));
        }

        strip.show();
        delay(RAINBOW_DELAY);
        Spark.process();
    }
}