#include <Arduino.h>
#include <avr/sleep.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <EncButton.h>



void OnWeldBtn();
void OnMenuBtn();
void WedingImpulse();
void MenuProc();
unsigned long ChangeValue(unsigned long ulValue,
                          unsigned long ulMin,                         
                          unsigned long ulMax,                         
                          int iMenuUpBtn,     
                          int iMenuDownBtn);
void initImpArr();
void OutputParamsOnLCD();
void Menu1(); 
void DrowMenu1(int Item, bool bOutDebug);
void Menu2NumPulses();
void DrowMenu2(int iMenuItem, bool bIsChanges);
void SetNumPulses(bool bView);
void SetPulsesParams();
void DrawParams(int l_iIndx, int l_iItem, bool l_bChangeParams);
void SaveParams();
void LoadParams();

enum eMenuItem {IMP_DUR, PAUSE_BW_IMP, 
                IMP_NUM, IMP_NUM_MAX = 10, MENU_EXIT = 3, 
                IMP_ARR_DIM=10, MENU1_DIM = 4, 
                MENU2_DIM = 3, MENU_CURSOR_OFFSET = 10, 
                MENU_NUMSTR_LEN = 3, MENU_STR_LEN = 16,
                };

enum eMenu1Items {MENU1_SETTINGS, MENU1_SAVE, MENU1_LOAD, MENU1_BACK};
enum eMenu2Items {MENU2_NUM_PULSES, MENU2_DURATIONS, MENU2_BACK};

float g_fInputVoltage = 0.0, g_fWeldingVoltage = 0.0, g_fInMul = 5.0, g_fWeldMul = 4.0;
char g_chOutStr[32];
char g_chInputVoltage[6];
char g_chWeldingVoltage[6];
unsigned long g_ulmillsPrev = 0;
unsigned long g_ulmillsCurrent = 0;
const unsigned long g_ulmillsInteval = 500;

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

// 
bool g_bFistTime = true;

// 
unsigned int g_uiCurrItem = 0;

struct sImpDur{
    unsigned long ul_millsImpDur;
    unsigned long ul_millsPause;  
};

sImpDur Impulses[IMP_ARR_DIM];

String g_strMenu1[] = {
    "Settings",
    "Save",
    "Load",
    "Back"
};

//
String g_strMenu2[] = {
    "Pulses: ",
    "Pulses Params",
    "Back"
};

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 7, d5 = 8, d6 = 9, d7 = 10;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Encoder library by AlexGyver (cool dude -:)
EncButton<EB_TICK, g_btMenuUpBtnPin, g_btMenuDownBtnPin, g_btMenuBtnPin> enc;  // Ecoder with button <A, B, KEY>

void setup() {
    // set up the LCD's number of columns and rows:
    lcd.begin(16, 2);
    // Print a message to the LCD.
    lcd.print("Spot welding");
    lcd.setCursor(0, 1);
    lcd.print("by RX3AKZ");

    // Init impulses array
    initImpArr();

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
    digitalWrite(g_btPwOFFIonistorPin, HIGH); // HIGH - Off, LOW - On

    delay(2000);
  
    g_fInMul = (g_fInMul * 5)/1023.0;
    g_fWeldMul = (g_fWeldMul * 5)/1023.0;
    
    g_bWeldButIsPressed = false;
    g_bMenuButIsPressed = false;

    digitalWrite(g_btPwOFFIonistorPin, LOW); // LOW - On

    attachInterrupt(digitalPinToInterrupt(g_btWeldBtnPin), OnWeldBtn, FALLING);
    //attachInterrupt(digitalPinToInterrupt(g_btMenuBtnPin), OnMenuBtn, FALLING);

    Serial.begin(9600);
}

void loop() {
    unsigned long l_ulCyclesCount = 0;
    unsigned int l_uiQuantityItems = 0;

    g_fInputVoltage = 0.0;
    g_fWeldingVoltage = 0.0;
    
    g_bWeldButIsPressed = false;
    g_bMenuButIsPressed = false;

    g_ulmillsPrev =  millis();
        
    if ( g_bFistTime ) {
        Serial.println("Start main loop");
        sprintf(g_chOutStr, "Sizeof(Impulses): %u", sizeof(Impulses));
        Serial.println(g_chOutStr);
    }

    do {
        // Input Voltage accumulation
        g_fInputVoltage += analogRead(A0);
  
        // Welding Voltage accumulation
        g_fWeldingVoltage += analogRead(A1);
        
        // Increment Cycles count
        l_ulCyclesCount++;

        enc.tick();
        // On the rest
        delay(10);
        
        if( enc.click() ) {
            g_bMenuButIsPressed = true;
            Serial.println("Button is pressed");        
        }
        else if( enc.isLeft() ) {
            Serial.println("Encoder to left");
            g_uiCurrItem++;

            sprintf(g_chOutStr, "Impulses: %lu", g_ulNumOfImp);
            Serial.println(g_chOutStr);

            l_uiQuantityItems = (int)g_ulNumOfImp * 2;
            sprintf(g_chOutStr, "QuantityItems: %u", l_uiQuantityItems);
            Serial.println(g_chOutStr);


            if( g_uiCurrItem > l_uiQuantityItems ) {
                g_uiCurrItem = 0;
            }
            sprintf(g_chOutStr, "CurrParams Item: %u", g_uiCurrItem);
            Serial.println(g_chOutStr);
        }
        else if( enc.isRight() ) {
            Serial.println("Encoder to right");
            l_uiQuantityItems = g_ulNumOfImp * 2;
            if( g_uiCurrItem == 0 ) {
                g_uiCurrItem = l_uiQuantityItems + 1;
            }
            g_uiCurrItem--;
            
            sprintf(g_chOutStr, "CurrParams Item: %u", g_uiCurrItem);
            Serial.println(g_chOutStr);
        } 

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

    // print welding params
    // Pos - 0, row - 2 
    lcd.setCursor(0, 1);
    //sprintf(g_chOutStr, "Impulses: %lu", g_ulNumOfImp);
    //lcd.print(g_chOutStr);
    OutputParamsOnLCD();

    if ( g_bFistTime ) {
        sprintf(g_chOutStr, "Welding: %i Menu:  %i",  g_bWeldButIsPressed, g_bMenuButIsPressed); 
        Serial.println(g_chOutStr);

        sprintf(g_chOutStr, "CurrParams Item: %u", g_uiCurrItem);
        Serial.println(g_chOutStr);

        g_bFistTime = false;
    }

    if (g_bWeldButIsPressed) {
        WedingImpulse();
        detachInterrupt( digitalPinToInterrupt(g_btWeldBtnPin) );
        g_bWeldButIsPressed = false;
        attachInterrupt( digitalPinToInterrupt(g_btWeldBtnPin), OnWeldBtn, FALLING );
    }
    else if(g_bMenuButIsPressed) {
        Menu1(); //MenuProc();
        g_bMenuButIsPressed = false;
    }
}

void WedingImpulse(){

    unsigned long l_ulIndx;

    // Power Off from ionistor 
    digitalWrite(g_btPwOFFIonistorPin, HIGH);
    delay(10);

    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Fire!!!!!!!!!!!");

    for(l_ulIndx = 0; l_ulIndx < g_ulNumOfImp; l_ulIndx++) {
        digitalWrite(g_btWeldingImpulsPin, LOW);
        delay(g_ulImpDur);
        digitalWrite(g_btWeldingImpulsPin, HIGH);
        delay(g_ulPause);
    }

    lcd.clear();

    // Power On to ionistor 
    digitalWrite(g_btPwOFFIonistorPin, LOW);

    g_bWeldButIsPressed = false;
    g_bMenuButIsPressed = false;
    attachInterrupt(digitalPinToInterrupt(g_btWeldBtnPin), OnWeldBtn, FALLING);
    //attachInterrupt(digitalPinToInterrupt(g_btMenuBtnPin), OnMenuBtn, FALLING);
}

void MenuProc() {
    int l_iMenuUpBtn, l_iMenuDownBtn;
    bool l_bFirstPass = true;

    g_btMenuItem = IMP_DUR; 
    Serial.println("Menu proc");
    Serial.print("Menu Item: ");
    Serial.println(g_btMenuItem);
    delay(100);

    lcd.clear();

    while (true) {

        // Read Up/Down buttons state
        l_iMenuUpBtn = digitalRead(g_btMenuUpBtnPin);
        l_iMenuDownBtn = digitalRead(g_btMenuDownBtnPin); 

        if(l_iMenuUpBtn == HIGH &&  l_iMenuDownBtn == HIGH && !l_bFirstPass) {
            g_btMenuItem++;
            if(g_btMenuItem > MENU_EXIT )
                g_btMenuItem = IMP_DUR;

        }
    
        Serial.print("Menu Buttons: ");
        Serial.print(l_iMenuUpBtn);
        Serial.print(",");
        Serial.println(l_iMenuDownBtn);
        
        Serial.print("Menu Item: ");
        Serial.println(g_btMenuItem);

        switch (g_btMenuItem) {
        case IMP_DUR:
            g_ulImpDur = ChangeValue(g_ulImpDur,
                                     g_ulImpDurMin,
                                     g_ulImpDurMax,
                                     l_iMenuUpBtn,
                                     l_iMenuDownBtn);
            sprintf(g_chOutStr, "Imp Dur: %lu   ", g_ulImpDur);
            Serial.println(g_chOutStr);                                     
            break;
        case PAUSE_BW_IMP:
            g_ulPause = ChangeValue(g_ulPause,
                                    g_ulImpDurMin,
                                    g_ulImpDurMax,
                                    l_iMenuUpBtn,
                                    l_iMenuDownBtn);
            sprintf(g_chOutStr, "Pause: %lu  ", g_ulPause);
            Serial.println(g_chOutStr);                        
            break;

        case IMP_NUM:                                        
            g_ulNumOfImp = ChangeValue(g_ulNumOfImp,
                                       1,
                                       10,
                                       l_iMenuUpBtn,
                                       l_iMenuDownBtn);
            sprintf(g_chOutStr, "Pulses: %lu     ", g_ulNumOfImp);
            Serial.println(g_chOutStr);                           
            break;

        case MENU_EXIT:
            Serial.println("Exit"); 
            if(l_iMenuUpBtn == LOW || l_iMenuDownBtn == LOW) {
                g_bWeldButIsPressed = false;
                g_bMenuButIsPressed = false;
                attachInterrupt(digitalPinToInterrupt(g_btWeldBtnPin), OnWeldBtn, FALLING);
                attachInterrupt(digitalPinToInterrupt(g_btMenuBtnPin), OnMenuBtn, FALLING);
                return;                
            }
            sprintf(g_chOutStr, "Exit           ");
            break;

        default:
            break;
        }

        delay(100);

        lcd.setCursor(0, 0);
        lcd.print("Menu:          ");
        lcd.setCursor(0, 1);
        lcd.print(g_chOutStr);
        
        Serial.print("Menu Item: ");
        Serial.println(g_btMenuItem);
        delay(100);

        attachInterrupt(digitalPinToInterrupt(g_btMenuBtnPin), OnMenuBtn, FALLING);
        set_sleep_mode(SLEEP_MODE_PWR_DOWN); // PowerDown
        sleep_mode(); // on the rest
        delay(50);
        l_bFirstPass = false;
    }
}

unsigned long ChangeValue(unsigned long ulValue,
                          unsigned long ulMin,                         
                          unsigned long ulMax,                         
                          int iMenuUpBtn,     
                          int iMenuDownBtn) {

    if(iMenuUpBtn == LOW) {
        ulValue += ulMin;
        if ( ulValue > ulMax)
            ulValue = ulMax;
    }
    else if(iMenuDownBtn == LOW) {
        ulValue -= ulMin;
        if (ulValue < ulMin)
            ulValue = ulMin;
    }

    return ulValue;
}

void OnWeldBtn() {
    g_bWeldButIsPressed = true;
}

void OnMenuBtn() {
    g_bMenuButIsPressed = true;
}


void ExpStrBySpases(String &rstr, unsigned int uilen) {
    while (rstr.length() < uilen) {
        rstr += " ";
    }
}

void initImpArr() {
    int l_iIndx = 0;
 
    g_ulNumOfImp = 2;
    
    Impulses[l_iIndx].ul_millsImpDur = 10;
    Impulses[l_iIndx++].ul_millsPause = 10;           
    Impulses[l_iIndx].ul_millsImpDur = 40;
    Impulses[l_iIndx++].ul_millsPause = 20;
    
    for( ;l_iIndx < IMP_NUM_MAX; l_iIndx++ ) {
        Impulses[l_iIndx].ul_millsImpDur = 41 + l_iIndx;
        Impulses[l_iIndx].ul_millsPause = 21 + l_iIndx;
    }
}

void OutputParamsOnLCD() {

    unsigned int l_uiIndx, l_uiItem;
    String l_strOut;

    lcd.setCursor(0, 1);
    if ( g_uiCurrItem == 0 ) {
        sprintf(g_chOutStr, "Pulses: %lu", g_ulNumOfImp);
    }   
    else {
        l_uiIndx = (g_uiCurrItem - 1)/2;
        l_uiItem = (g_uiCurrItem - 1) % 2;
        if ( !l_uiItem ) {
            sprintf(g_chOutStr, "%u Duration: %lu", l_uiIndx + 1, Impulses[l_uiIndx].ul_millsImpDur);
        }
        else {
            sprintf(g_chOutStr, "%u Pause: %lu", l_uiIndx + 1, Impulses[l_uiIndx].ul_millsPause);
        }
        Serial.println(g_chOutStr);
    }

    l_strOut = g_chOutStr;
    ExpStrBySpases(l_strOut, MENU_STR_LEN);
    lcd.print(l_strOut);
}  

void Menu1() {
    int l_iMenuItem = 0; 
    bool l_bMenuProc = true, l_bIsChanges = true;

    while( l_bMenuProc ) {
        DrowMenu1(l_iMenuItem, l_bIsChanges);           
        
        
        enc.tick();
        if( enc.isLeft() ) {
            Serial.println("Encoder left");
            l_bIsChanges = true;
            l_iMenuItem++;
            sprintf(g_chOutStr, "Menu1 Item: %i", l_iMenuItem);
            Serial.println(g_chOutStr);

            if ( l_iMenuItem >= MENU1_DIM ) {
                l_iMenuItem = 0;
                sprintf(g_chOutStr, "Menu1 Item: %i", l_iMenuItem);
                Serial.println(g_chOutStr);
            }    
        }
        else if( enc.isRight() ) {
            Serial.println("Encoder right");
            l_bIsChanges = true;
            sprintf(g_chOutStr, "Menu1 Item: %i", l_iMenuItem);
            Serial.println(g_chOutStr);

            l_iMenuItem--;
            sprintf(g_chOutStr, "Menu1 Item: %i", l_iMenuItem);
            Serial.println(g_chOutStr);

            if ( l_iMenuItem < 0 ) {
                l_iMenuItem = MENU1_DIM - 1;
                sprintf(g_chOutStr, "Menu1 Item: %i", l_iMenuItem);
                Serial.println(g_chOutStr);
            }

        }   
        else {
            l_bIsChanges = false;
        }
        if( enc.isClick() ) {
            Serial.println("Button is clicked");
            switch ( l_iMenuItem ) {
            case MENU1_BACK:
                l_bMenuProc = false;
                Serial.println("Return from Menu1");
                break;
            case MENU1_SETTINGS:
                 Menu2NumPulses();   
                break;
            case MENU1_SAVE:
                SaveParams();
                break;
            case MENU1_LOAD:
                LoadParams();
                break;
            default:
                break;
            }
        }
        delay(10);
    }
}

void DrowMenu1(int Item, bool bOutDebug) {
    
    String l_strOut;
    int l_iItem = Item % 2;

    if ( bOutDebug ) {
        sprintf(g_chOutStr, "DrowMtnu1 Item in func: %i", Item);
        Serial.println(g_chOutStr);    
    }

    if ( l_iItem == 0 ) {

        l_strOut = "->" + g_strMenu1[Item]; 
        ExpStrBySpases(l_strOut, MENU_STR_LEN);
        
        if ( bOutDebug ) {
            sprintf(g_chOutStr, "DrowMtnu1 1 Item: %i", Item);
            Serial.println(g_chOutStr);    
        }

        lcd.setCursor(0, 0);
        lcd.print(l_strOut);

        Item++;
        l_strOut = "  " + g_strMenu1[Item]; 
        ExpStrBySpases(l_strOut, MENU_STR_LEN);
        lcd.setCursor(0, 1);
        lcd.print(l_strOut);

        if ( bOutDebug ) {
            sprintf(g_chOutStr, "DrowMtnu1 2 Item: %i", Item);
            Serial.println(g_chOutStr);
        }
    }
    else {
        l_iItem = Item -1;
        l_strOut = "  " + g_strMenu1[l_iItem]; 
        ExpStrBySpases(l_strOut, MENU_STR_LEN);
        if ( bOutDebug ) {
            sprintf(g_chOutStr, "DrowMtnu1 1 Item: %i", l_iItem);
            Serial.println(g_chOutStr);    
        }

        lcd.setCursor(0, 0);
        lcd.print(l_strOut);

        l_iItem++;
        l_strOut = "->" + g_strMenu1[l_iItem]; 
        ExpStrBySpases(l_strOut, MENU_STR_LEN);
        lcd.setCursor(0, 1);
        lcd.print(l_strOut);

        if ( bOutDebug ) {
            sprintf(g_chOutStr, "DrowMtnu1 2 Item: %i", l_iItem);
            Serial.println(g_chOutStr);
        }

    }
}

void Menu2NumPulses() {
   int l_iMenuItem = 0; 
   bool l_bMenuProc = true, l_bIsChanges = true;

    Serial.println("Menu2NumPulses");
    
    while( l_bMenuProc ){
        DrowMenu2(l_iMenuItem, l_bIsChanges);

        enc.tick();
        if (enc.isLeft()) {
            Serial.println("Menu2NumPulses enc left");
            l_iMenuItem++;
            if ( l_iMenuItem >= MENU2_DIM ) {
                l_iMenuItem = 0;
            } 
        }
        else if(enc.isRight()) {
            Serial.println("Menu2NumPulses enc right");
            l_iMenuItem--;
            if ( l_iMenuItem < 0 ) {
                l_iMenuItem = MENU2_DIM - 1;
            } 

        }
        else if(enc.isClick()) {
            Serial.println("Menu2NumPulses button is clicked");
            switch ( l_iMenuItem ) {
            case MENU2_BACK:
                l_bMenuProc = false;
                Serial.println("Return from Menu2");
                break;
            case MENU2_NUM_PULSES:
                 SetNumPulses(false);   
                break;
            case MENU2_DURATIONS:
                SetPulsesParams();
                break;   
            default:
                break;
            }
        }
        delay(10);
    }
}

void DrowMenu2(int iMenuItem, bool bIsChanges) {
    String l_strOut;
    int l_iItem = iMenuItem % 2;
    unsigned int l_uiSpaces = 16;

    if ( l_iItem == 0 ) {
        l_strOut = "->" + g_strMenu2[iMenuItem]; 
        
        if ( iMenuItem == MENU2_NUM_PULSES ) {
            l_uiSpaces = MENU_CURSOR_OFFSET;
            SetNumPulses(true);
        }
        else {
            l_uiSpaces = 16;
        }
        ExpStrBySpases(l_strOut, l_uiSpaces);
        lcd.setCursor(0, 0);
        lcd.print(l_strOut);

        iMenuItem++;
        if ( iMenuItem >= MENU2_DIM ) {
            l_strOut = "  ";
        }
        else {            
            l_strOut = "  " + g_strMenu2[iMenuItem]; 
        }
        ExpStrBySpases(l_strOut, l_uiSpaces);
        lcd.setCursor(0, 1);
        lcd.print(l_strOut);
    }
    else {
        l_iItem = iMenuItem -1;        
        l_strOut = "  " + g_strMenu2[l_iItem]; 
        if ( l_iItem == MENU2_NUM_PULSES ) {
            l_uiSpaces = MENU_CURSOR_OFFSET;
            SetNumPulses(true);
        }
        else {
            l_uiSpaces = MENU_STR_LEN;
        }
        ExpStrBySpases(l_strOut, l_uiSpaces);
        lcd.setCursor(0, 0);
        lcd.print(l_strOut);

        l_strOut = "->" + g_strMenu2[iMenuItem]; 
        ExpStrBySpases(l_strOut, l_uiSpaces);        
        lcd.setCursor(0, 1);
        lcd.print(l_strOut);
 
    }
}

void SetNumPulses(bool bView) {
    String l_strPules(g_ulNumOfImp);
    bool l_bSetNumPulsesProc = true;

    if ( bView ) {
        ExpStrBySpases(l_strPules, 3);
        lcd.setCursor(MENU_CURSOR_OFFSET, 0);
        lcd.print(l_strPules);
    }
    else {
        lcd.setCursor(MENU_CURSOR_OFFSET, 0);
        lcd.blink();        
        
        while(l_bSetNumPulsesProc) {
            lcd.setCursor(MENU_CURSOR_OFFSET, 0);
            lcd.blink();        
            enc.tick();
            if (enc.isLeft()) {
                g_ulNumOfImp++;
                if ( g_ulNumOfImp > IMP_NUM_MAX ) {
                    g_ulNumOfImp = IMP_NUM_MAX;
                }
                l_strPules = g_ulNumOfImp;
                ExpStrBySpases(l_strPules, MENU_CURSOR_OFFSET);
                lcd.print(l_strPules);
            }
            else if(enc.isRight()) {
                g_ulNumOfImp--;
                if ( g_ulNumOfImp < 1 ) {
                    g_ulNumOfImp = 1;
                }
                l_strPules = g_ulNumOfImp;
                ExpStrBySpases(l_strPules, MENU_CURSOR_OFFSET);
                lcd.print(l_strPules);
            }
            else if(enc.isClick()) {
                l_bSetNumPulsesProc = false;        
            }
                
        }
    }
}    

void SetPulsesParams() {
    int l_iItem = 0, l_iIndx =0;
    int l_iNumOfImp =  (int)g_ulNumOfImp;
    bool l_bSetPulsesParamsProc = true;
    String l_strOut;

    Serial.println("SetPulsesParams()");

    while(l_bSetPulsesParamsProc) {
        DrawParams(l_iIndx, l_iItem, false);

        enc.tick();
        if (enc.isLeft()) {
            Serial.println("enc left");
            l_iIndx++;
            if( l_iIndx == 2 ) {
                l_iItem++;
                l_iIndx = 0;
                if ( l_iItem > l_iNumOfImp ) {
                    l_iItem = l_iNumOfImp;
                }
            }
            if ( l_iItem == l_iNumOfImp ) {
                l_iIndx = 0;
            }
            
            l_strOut = String("l_iIndx: ") + String(l_iIndx);
            Serial.println(l_strOut);
            l_strOut = String("l_iItem: ") + String(l_iItem);
            Serial.println(l_strOut);

        }        
        else if(enc.isRight()) {
            l_iIndx--;
            if ( l_iIndx < 0 ) {
                l_iItem--;
                l_iIndx = 1;
                if ( l_iItem < 0 ) {
                    l_iItem = 0;
                    l_iIndx = 0;
                }  
            }

        }
        else if(enc.isClick()) {
            if (l_iItem == l_iNumOfImp ) {
                l_bSetPulsesParamsProc = false;
            }
            else {
                DrawParams(l_iIndx, l_iItem, true);
            }
        }                

    }    
}

void DrawParams(int l_iIndx, int l_iItem, bool l_bChangeParams) {
    
    String l_strDur;
    String l_strPause;
    int l_iNumOfImp =  (int)g_ulNumOfImp;
    bool l_encProc = true;

    if ( l_iItem < l_iNumOfImp ) {
        if ( l_iIndx == 0 ) {
            l_strDur = "->" + String(l_iItem+1) +   String(" Dur-on: ") + Impulses[l_iItem].ul_millsImpDur;
            l_strPause = "  " + String(l_iItem+1) + String(" Pause : ") + Impulses[l_iItem].ul_millsPause; 
        }
        else {
            l_strDur = "  " + String(l_iItem+1) + String(" Dur-on: ") + Impulses[l_iItem].ul_millsImpDur;
            l_strPause = "->" + String(l_iItem+1) + String(" Pause : ") + Impulses[l_iItem].ul_millsPause; 
        }
        ExpStrBySpases(l_strDur, MENU_STR_LEN);
        ExpStrBySpases(l_strPause, MENU_STR_LEN);
        lcd.setCursor(0, 0);
        lcd.print(l_strDur);

        lcd.setCursor(0, 1);
        lcd.print(l_strPause);
    }
    else {
        l_strPause = "->Back";
        ExpStrBySpases(l_strPause, MENU_STR_LEN);
        lcd.setCursor(0, 0);
        lcd.print(l_strPause);

        l_strPause = " ";
        ExpStrBySpases(l_strPause, MENU_STR_LEN);
        lcd.setCursor(0, 1);
        lcd.print(l_strPause);
    }
    if( l_bChangeParams ) {
        while ( l_encProc ) {

            if ( l_iIndx == 0 ) {
                lcd.setCursor(12, 0);
            }    
            else {
                lcd.setCursor(12, 1);
            }    
            lcd.blink();

            enc.tick();
            if( enc.isLeft() ) {
                if ( l_iIndx == 0 ) {
                    Impulses[l_iItem].ul_millsImpDur++;
                    if ( Impulses[l_iItem].ul_millsImpDur > 999 ) {
                        Impulses[l_iItem].ul_millsImpDur = 999;
                    }
                    l_strDur = Impulses[l_iItem].ul_millsImpDur;
                    ExpStrBySpases(l_strDur, 3);
                    lcd.print(l_strDur);
                    lcd.setCursor(12, 0);
                    lcd.blink();

                }
                else {
                    Impulses[l_iItem].ul_millsPause++;
                    if ( Impulses[l_iItem].ul_millsPause > 999 ) {
                        Impulses[l_iItem].ul_millsPause = 999;        
                    }
                    l_strPause = Impulses[l_iItem].ul_millsPause;
                    ExpStrBySpases(l_strPause, 3);
                    lcd.print(l_strPause);
                    lcd.setCursor(12, 1);
                    lcd.blink();
                }
            }
            else if ( enc.isRight() ) {
                if ( l_iIndx == 0 ) {
                    Impulses[l_iItem].ul_millsImpDur--;
                    if ( Impulses[l_iItem].ul_millsImpDur == 0 ) {
                        Impulses[l_iItem].ul_millsImpDur = 1;        
                    }
                    l_strDur = Impulses[l_iItem].ul_millsImpDur;
                    ExpStrBySpases(l_strDur, 3);
                    lcd.print(l_strDur);
                    lcd.setCursor(12, 0);
                    lcd.blink();
                }
                else {
                    Impulses[l_iItem].ul_millsPause--;
                    if ( Impulses[l_iItem].ul_millsPause == 0 ) {
                        Impulses[l_iItem].ul_millsPause = 1;        
                    }
                    l_strPause = Impulses[l_iItem].ul_millsPause;
                    ExpStrBySpases(l_strPause, 3);
                    lcd.print(l_strPause);
                    lcd.setCursor(12, 1);
                    lcd.blink();
                }
            }
            else if( enc.click() ) {
                l_encProc = false;
            }
        }

    }
} 

void SaveParams() {
    int l_iEE_Adress = 0;

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("-=Save params=-");
    lcd.setCursor(0,1);
    lcd.blink();

    EEPROM.put(l_iEE_Adress, g_ulNumOfImp);
    l_iEE_Adress = sizeof(g_ulNumOfImp);
    EEPROM.put(l_iEE_Adress, Impulses);
    delay(2000);
}

void LoadParams() {
    int l_iEE_Adress = 0;

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("-=Load params=-");
    lcd.setCursor(0,1);
    lcd.blink();

    EEPROM.get(l_iEE_Adress, g_ulNumOfImp);
    l_iEE_Adress = sizeof(g_ulNumOfImp);
    EEPROM.get(l_iEE_Adress, Impulses);
    delay(2000);
}            

