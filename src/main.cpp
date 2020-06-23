#include <Arduino.h>
#include <LiquidCrystal.h>


void OnWeldBtn();
void OnMenuBtn();
void WedingImpulse();
void MenuProc();

enum eMenuItem {IMP_DUR, PAUSE_BW_IMP, IMP_NUM, MENU_EXIT};


float g_fInputVoltage = 0.0, g_fWeldingVoltage = 0.0, g_fInMul = 5.0, g_fWeldMul = 4.0;
char g_chOutStr[17];
char g_chInputVoltage[6];
char g_chWeldingVoltage[6];
unsigned long g_ulmillsPrev = 0;
unsigned long g_ulmillsCurrent = 0;
const unsigned long g_ulmillsInteval = 100;

// Impulse duration/pause between impulses, ms 
const unsigned long g_ulImpDurMin = 50;
const unsigned long g_ulImpDurMax = 1000;

unsigned long g_ulImpDur = 100;
unsigned long g_ulPause = 100;
unsigned long g_ulNumOfImp = 2;
byte g_btMenuItem = IMP_DUR;

// Welding button is pressed
volatile bool g_bWeldButIsPressed = false; 

// Menu button is pressed
volatile bool g_bMenuButIsPressed = false; 


// Welding button pin (interrupt)
const byte g_btWeldBtnPin = 2;

// Menu button pin  (interrupt)
const byte g_btMenuBtnPin = 3;

// Menu UP button pin
const byte g_btMenuUpBtnPin = 4;

// Menu down button pin
const byte g_btMenuDownBtnPin = 5;

// Welding output Impuls pin
const byte g_btWeldingImpulsPin = 6;

// Output pin (13) - turn off ionistor from power supply
const byte g_btPwOFFIonistorPin = LED_BUILTIN;


// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 7, d5 = 8, d6 = 9, d7 = 10;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


void setup() {
    // set up the LCD's number of columns and rows:
    lcd.begin(16, 2);
    // Print a message to the LCD.
    lcd.print("Spot welding");
    lcd.setCursor(0, 1);
    lcd.print("by RX3AKZ");
  
    // Input buttons
    pinMode(g_btWeldBtnPin, INPUT_PULLUP);
    pinMode(g_btMenuBtnPin, INPUT_PULLUP); 
    pinMode(g_btMenuUpBtnPin, INPUT_PULLUP);
    pinMode(g_btMenuDownBtnPin, INPUT_PULLUP);

    // Outputs
    pinMode(g_btWeldingImpulsPin, OUTPUT);
    pinMode(g_btPwOFFIonistorPin, OUTPUT);

    // Set outputs
    digitalWrite(g_btWeldingImpulsPin, HIGH); // pin to invertion amp 
    digitalWrite(g_btPwOFFIonistorPin, HIGH); //

    delay(2000);
  
    g_fInMul = (g_fInMul * 5)/1023.0;
    g_fWeldMul = (g_fWeldMul * 5)/1023.0;
    
    attachInterrupt(digitalPinToInterrupt(g_btWeldBtnPin), OnWeldBtn, LOW);
    attachInterrupt(digitalPinToInterrupt(g_btMenuBtnPin), OnMenuBtn, LOW);

    // Serial.begin(9600);
}

void loop() {
    unsigned long l_ulCyclesCount = 0;
 
    g_fInputVoltage = 0.0;
    g_fWeldingVoltage = 0.0;
    
    g_bWeldButIsPressed = false;

    g_ulmillsPrev =  millis();
        
    do {
        // Input Voltage accumulation
        g_fInputVoltage += analogRead(A0);
  
        // Welding Voltage accumulation
        g_fWeldingVoltage += analogRead(A1);
        
        // Increment Cecles count
        l_ulCyclesCount++;

        // Was interupts from buttons ? 
        if (g_bWeldButIsPressed || g_bMenuButIsPressed ) {
            break;    
        }
    
        // On the rest
        delay(10);
        
        g_ulmillsCurrent = millis();
    } while( (g_ulmillsCurrent - g_ulmillsPrev) < g_ulmillsInteval );

    // Input Voltage
    g_fInputVoltage = g_fInMul * (g_fInputVoltage/l_ulCyclesCount);
    dtostrf(g_fInputVoltage, 5, 2, g_chInputVoltage);

    //Welding Voltage21
    g_fWeldingVoltage = g_fWeldMul * (g_fWeldingVoltage/l_ulCyclesCount);
    dtostrf(g_fWeldingVoltage, 5, 2, g_chWeldingVoltage);

    // Voltage on LCD
    sprintf(g_chOutStr, "V: %s %s", g_chInputVoltage, g_chWeldingVoltage);
    lcd.setCursor(0, 0);
    lcd.print(g_chOutStr);

    if (g_bWeldButIsPressed) {
        WedingImpulse();    
    }
    else if(g_bMenuButIsPressed) {
        void MenuProc();
    }

    lcd.setCursor(10, 1);
    // print the number of seconds since reset:
    lcd.print(millis() / 1000);
    delay(1000); 
}


void WedingImpulse(){

}

void MenuProc() {
    int l_iMenuUpBtn, l_iMenuDownBtn;
    
    while (true) {

        // Read buttons state
        l_iMenuUpBtn = digitalRead(g_btMenuUpBtnPin);
        l_iMenuDownBtn = digitalRead(g_btMenuUpBtnPin); 

        switch (g_btMenuItem) {
        case IMP_DUR:
            if(l_iMenuUpBtn) {
                g_ulImpDur += g_ulImpDurMin;
                if (g_ulImpDur > g_ulImpDurMax)
                    g_ulImpDur = g_ulImpDurMax;
            }
            else if( l_iMenuDownBtn) {
                g_ulImpDur -= g_ulImpDurMin;
                if (g_ulImpDur < g_ulImpDurMin)
                    g_ulImpDur = g_ulImpDurMin;
            }
            break;
        
        default:
            break;
        }



    }




}

void OnWeldBtn() {
    g_bWeldButIsPressed = true;
    
    //Disable interrupt from buttons
    detachInterrupt(digitalPinToInterrupt(g_btWeldBtnPin));
    detachInterrupt(digitalPinToInterrupt(g_btMenuBtnPin));
}

void OnMenuBtn() {
    g_bMenuButIsPressed = true;
    
    //Disable interrupt from buttons
    detachInterrupt(digitalPinToInterrupt(g_btWeldBtnPin));
    detachInterrupt(digitalPinToInterrupt(g_btMenuBtnPin));
}

