#include <String.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h> // Graphics for screen
#include <Adafruit_SSD1306.h> // Screen


#include "config.h"          
// This houses two defines - #define SSID "<my ssid>" and #define PASSWORD "<my password>"

// WIFI & JMRI Stuff
const char* ssid = SSID;
const char* password = PASSWORD;
const char* host = "trainserver"; // ip address or hostname of the server
const int port = 12090;
String throttleName = "Traininator"; // Visible on JMRI
String hostname = "Traininator"; // Hostname visible on your router
int trainAddress = 3;
WiFiClient trainServer;

// OLED Stuff
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// EEPROM size
#define EEPROM_SIZE 40


// Other global variables
char oldThrottleLevel = 0;
unsigned long doubleClickTime = 0;
int timeForDoubleClick = 2000; // number of milliseconds between clicks to stop the train
int timeForTimeOut = 20000; // number of milliseconds before the programming function times out
int programMode = 0; // which mode of programming we are in
unsigned long programmingTimer = 0; // Timer to "time out" the double click message
char trainDirection = 0; // if we are in neutral or not


// If you are like me and your throttle is backwards leave this in.  Otherwise comment it out
//#define WHOOPS_I_REVERSED_MY_THROTTLE 1



// Button stuff for debouncing, etc.  Make sure these are all the same size of arrays
// my button layout:  small = 0, fw=1, re=2, button=8, sw1=7, sw2=5, sw3=6, sw4=4, sw5=3
const unsigned char buttonPins[] = {15, 13, 4, 19, 5, 18, 17, 16, 12};
unsigned char buttonValues[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned char buttonHistory[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
// functions - Forward = 99, backward = 98, programming button = 97
// I'm picking my switches to control 0,1,2,3,7,8 since 8 is mute and 7 is dim on my controller
unsigned char trainFunction[] = {97, 99, 98, 0, 1, 3, 7, 8, 2}; // What functions do you want the buttons to turn on/off?
unsigned char horn = 8; // Use the horn button for programming mode. Which of the buttonPins is the horn?
char throttlePin = A0; //What the throttle is hooked up to.  A0 = GPIO36


void setup() {
    Serial.begin(9600);


    // Display stuff
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        while (true) {
            Serial.println(F("SSD1306 allocation failed"));
        }
    }
    // EEPROM
    EEPROM.begin(EEPROM_SIZE);


    readInEEPROM();

    resetTheDisplay();

    // Pins
    for (int x = 0; x < sizeof(buttonPins); x++) {
        pinMode(buttonPins[x] , INPUT_PULLUP);
    }

    //Wifi
    display.setCursor(0, 9);
    display.println("Connecting to WIFI:");
    display.display();
    // Actually start the wifi - flash a message
    WiFi.setHostname(hostname.c_str());
    WiFi.begin(ssid, password);
    int flash = 0;
    while (WiFi.status() != WL_CONNECTED) {
        if (flash == 0) {
            flash = 1;
            display.setTextColor(WHITE);
            display.setCursor(0, 9);
            display.println("Connecting to WIFI:");
            display.display();
        } else {
            flash = 0;
            display.setTextColor(BLACK);
            display.setCursor(0, 9);
            display.println("Connecting to WIFI:");
            display.display();
        }
        delay(500);
    }
    // clear message and show connecting message
    resetTheDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 9);
    display.println("Connecting to server:");
    display.display();
    // connect to the train server
    if (!trainServer.connect(host, port)) {
        display.println("FAIL!");
        display.display();
        delay(5000);
        ESP.restart();
    }
    // All connected.  Reset display
    resetTheDisplay();

    // Name our throttle and connect to a train
    trainServer.println("N" + throttleName);
    delay(500);
    // Take the reply and dump it to the user
    while (trainServer.available()) {
        char c = trainServer.read();
        Serial.write(c);
    }

    chooseLoco();

}






void loop() {
    char newThrottleLevel = getThrottleAmount();
    if (newThrottleLevel != oldThrottleLevel) {
        oldThrottleLevel = newThrottleLevel;
        setSpeed(newThrottleLevel);
    }

    readButtons();

    display.display();

    // we set a timeout period for the programming timer to get rid of the stop message
    if (programmingTimer > 0) {
        unsigned long nowTime = millis();
        if (nowTime > programmingTimer) {
            programmingTimer = 0;
            resetTheDisplay();
            if (programMode == 0) {
                programMode++;
                programmingFunction();
            } else {
                programMode = 0;
            }
        }
    }



    // Take the reply and dump it to the user
    while (trainServer.available()) {
        char c = trainServer.read();
        Serial.write(c);
    }
}

// Resets the display
void resetTheDisplay(void) {
    Serial.println("ResetTheDisplay");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("Traininator #");
    char printAdd[10];
    sprintf(printAdd, "%04d", trainAddress);
    display.println(printAdd);
    for (int x = 0; x < sizeof(trainFunction); x++ ) {
        if (trainFunction[x] < 96) {
            if (buttonValues[x] == 1) {
                display.setTextColor(WHITE);
            } else {
                display.setTextColor(BLACK);
            }
            int xCoordinate = (x * 20) - 55;
            display.setCursor(xCoordinate, 9);
            display.print(trainFunction[x]);
        }
    }
    display.setCursor(116, 0);
    if (trainDirection == 1) {
        display.setTextColor(WHITE);
        display.print("FW");
    } else if (trainDirection == 2) {
        display.setTextColor(WHITE);
        display.print("RE");
    }
    display.setTextColor(WHITE);
    display.display();
}


// Reads the EEPROM and if applicable writes it to the global variables
void readInEEPROM(void) {
    Serial.println("readInEEPROM");
    int tempTrainAdd = 0;
    EEPROM.get(0, tempTrainAdd);
    Serial.println(tempTrainAdd);
    // If there is nothing to read
    if (tempTrainAdd == 0 || tempTrainAdd == -1) {
        Serial.println("Returning, no change");
    } else {
        trainAddress = tempTrainAdd;
    }
    unsigned char tempTrainFunction[] = {97, 99, 98, 0, 1, 3, 7, 8, 2};
    EEPROM.get(10, tempTrainFunction);
    if (tempTrainFunction[0] != 97) {
        // EEPROM not programed yet.  Use default
        ;
    } else {
        for (int x = 0; x < sizeof(trainFunction); x++) {
            trainFunction[x] = tempTrainFunction[x];
        }
    }
}

// Writes the EEPROM values that were changed
void writeOutEEPROM(void) {
    Serial.println("writeOutEEPROM");

    EEPROM.put(0, trainAddress);
    EEPROM.put(10, trainFunction);
    EEPROM.commit();
}



// Reads the throttle the throttlePin
char getThrottleAmount(void) {
    // get average input readings
    long sensorValue = 0;
    int numReadings = 10;
    for (int x = 0; x < numReadings; x++) {
        sensorValue += analogRead(A0);
    }
    sensorValue = sensorValue / numReadings;  // Now a number between 480 and 17216
    //Serial.print(sensorValue);

    long throttleAmount = sensorValue * 128 / 4095 ;  // now a number between 0 and 128
    //Serial.print(" - "); Serial.print(throttleAmount);
    if (throttleAmount > 128)
        throttleAmount = 128;
    if (throttleAmount < 0)
        throttleAmount = 0;
#ifdef WHOOPS_I_REVERSED_MY_THROTTLE
    throttleAmount = 128 - throttleAmount;
#endif

    //Serial.print(" - "); Serial.println(throttleAmount);
    for (char x = 30; x <= 32; x++) {
        display.drawLine(0, x, 128, x, BLACK);
    }
    if ( throttleAmount > 0) {
        for (char x = 0; x <= throttleAmount; x++) {
            display.drawLine(x, 30, x, 32, WHITE);
        }
    }
    return throttleAmount;
}

// reads the button and debounce the output.
// This also determines what happens based on the buttons and switches
// functions - Forward = 99, backward = 98, programming button = 97
void readButtons(void) {

    for (int x = 0; x < sizeof(buttonPins); x++) {
        buttonHistory[x] = buttonHistory[x] << 1;
        char readPin = digitalRead(buttonPins[x]);
        buttonHistory[x] |= readPin;
        int thisFunction = trainFunction[x];

        if (buttonValues[x] == 0 && (buttonHistory[x] | 0b11111000) == 0b11111000) { // if it is held down 3 cycles in a row
            buttonValues[x] = 1;
            if (programMode > 0) {
                if (x == horn) {
                    programSomething();
                } else if (thisFunction == 97) {
                    programmingFunction();
                }
            } else if (thisFunction == 97) {
                programmingFunction();
            } else if (thisFunction == 99) {
                setDirectionForward();
            } else if (thisFunction == 98) {
                setDirectionBackwards();
            } else {
                turnOnFunction(thisFunction);
            }

        } else if (buttonValues[x] == 1 && (buttonHistory[x] | 0b11111000) == 0b11111111 ) { //if it is released
            buttonValues[x] = 0;
            if (programMode > 0) {
                ;
            } else if (thisFunction == 97) {  // don't care when programming button is released
                ;
            } else if (thisFunction == 98 || thisFunction == 99) {
                setDirectionNeutral();
            } else {
                turnOffFunction(thisFunction);
            }
        }
    }
}

/*
    Sending commands to the train server
*/

// Actually send a command
void sendCommand(String command) {
    trainServer.println(command);
    // Take the reply and dump it to the user
    while (trainServer.available()) {
        char c = trainServer.read();
        Serial.write(c);
    }
}


// Choose the DCC address we are controlling
void chooseLoco(void) {
    Serial.print("chooseLoco :");
    Serial.println(trainAddress);
    String realAddress = returnAddress();
    String command = "MT+" + realAddress + "<;>" + realAddress;
    sendCommand(command);
}

// Format the address for the JMRI server (short ID# or long ID#)
String returnAddress(void) {
    char returnme[10];
    if (trainAddress < 128) {
        sprintf(returnme, "S%02d", trainAddress);
    } else {
        sprintf(returnme, "L%04d", trainAddress);
    }
    return returnme;
}

// Turn on track power
void turnOnPower(void) {
    Serial.println("turnOnPower");
    sendCommand("PPA1");
    buttonValues[horn] = 0;  // Release the button so the horn won't go off
    delay(500); // give you time to release the horn
}

// Turn off track power
void turnOffPower(void) {
    Serial.println("turnOffPower");
    sendCommand("PPA0");
    buttonValues[horn] = 0;  // Release the button so the horn won't go off
    delay(500); // give you time to release the horn
}

// Turn on a function on the train engine
// For example function 2 is the horn usually - generally once turns it on and once turns it off
void turnOnFunction(int functionNumber) {
    Serial.print("turnOnFunction :");
    Serial.println(functionNumber);
    String realAddress = returnAddress();
    char command[30];
    sprintf(command, "MTA%s<;>F1%02d", realAddress, functionNumber);
    resetTheDisplay();
    sendCommand(command);
}

// Turn a function off
void turnOffFunction(int functionNumber) {
    Serial.println("turnOffFunction :");
    Serial.println(functionNumber);
    String realAddress = returnAddress();
    char command[30];
    //sprintf(command, "MTA%s<;>F0%02d", realAddress, functionNumber);
    sprintf(command, "MTA%s<;>F1%02d", realAddress, functionNumber);
    resetTheDisplay();
    sendCommand(command);
}

// Sets the train direction forward
void setDirectionForward(void) {
    Serial.println("setDirectionForward");
    trainDirection = 1;
    String realAddress = returnAddress();
    char command[30];
    sprintf(command, "MTA%s<;>R1", realAddress);
    sendCommand(command);
    resetTheDisplay();
}

// Sets the train direction backwards
void setDirectionBackwards(void) {
    Serial.println("setDirectionBackwards");
    trainDirection = 2;
    String realAddress = returnAddress();
    char command[30];
    sprintf(command, "MTA%s<;>R0", realAddress);
    sendCommand(command);
    resetTheDisplay();
}

// Train in Neutral??
void setDirectionNeutral(void) {
    Serial.println("setDirectionNeutral");
    trainDirection = 0;
    setSpeed(0);
    resetTheDisplay();
}


// Sets the speed of the train.
// Note send a -1 to issue a stop command
void setSpeed(int speed) {
    Serial.print("setSpeed :");
    Serial.println(int(speed));
    if (speed > 126) {
        speed = 126;
    }
    if (trainDirection == 0 && speed != -1) { //Override it if we are in neutral
        speed = 0;
    }
    String realAddress = returnAddress();
    char command[30];
    sprintf(command, "MTA%s<;>V%d", realAddress, speed);
    sendCommand(command);
}

// "emergency" stop - also shuts off engine on some decoders
void stopNow() {
    Serial.println("stopNow");
    trainDirection = 0;  // put it in Neutral so it stays stopped
    setSpeed(-1);
}

// the little button is used to program things or for an emergency stop
void programmingFunction(void) {
    Serial.println("programmingFunction");
    unsigned long clickTime = millis();
    if (programMode == 0 && clickTime < (doubleClickTime + timeForDoubleClick)) {
        doubleClickTime = 0;
        programmingTimer = 0;
        stopNow();
        resetTheDisplay();
    } else {
        if (programMode > 0) { // need somehow to know this is the second button click after the doubleclick timeout
            programMode ++;
        }
        doubleClickTime = clickTime;
        switch (programMode) {
            case 0:
                display.setCursor(0, 17);
                display.print("Click again to stop!");
                display.display();
                programmingTimer = clickTime + timeForDoubleClick;
                break;
            case 2:
                display.setCursor(0, 17);
                display.print("Turn on power?");
                display.display();
                programmingTimer = clickTime + timeForTimeOut;
                break;
            case 3:
                resetTheDisplay();
                display.setCursor(0, 17);
                display.print("Turn off power?");
                display.display();
                programmingTimer = clickTime + timeForTimeOut;
                break;
            case 4:
                resetTheDisplay();
                display.setCursor(0, 17);
                display.print("Choose loco?");
                display.display();
                programmingTimer = clickTime + timeForTimeOut;
                break;
            case 5:
                resetTheDisplay();
                display.setCursor(0, 17);
                display.print("Program Switch 1?");
                display.display();
                programmingTimer = clickTime + timeForTimeOut;
                break;
            case 6:
                resetTheDisplay();
                display.setCursor(0, 17);
                display.print("Program Switch 2?");
                display.display();
                programmingTimer = clickTime + timeForTimeOut;
                break;
            case 7:
                resetTheDisplay();
                display.setCursor(0, 17);
                display.print("Program Switch 3?");
                display.display();
                programmingTimer = clickTime + timeForTimeOut;
                break;
            case 8:
                resetTheDisplay();
                display.setCursor(0, 17);
                display.print("Program Switch 4?");
                display.display();
                programmingTimer = clickTime + timeForTimeOut;
                break;
            case 9:
                resetTheDisplay();
                display.setCursor(0, 17);
                display.print("Program Switch 5?");
                display.display();
                programmingTimer = clickTime + timeForTimeOut;
                break;
            case 10:
                resetTheDisplay();
                display.setCursor(0, 17);
                display.print("Program Button?");
                display.display();
                programmingTimer = clickTime + timeForTimeOut;
                break;
            default:
                programmingTimer = 0;
                programMode = 0;
                resetTheDisplay();
                break;
        }
    }
}

void programSomething(void) {
    Serial.println("programSomething");
    if (programMode == 2) {
        turnOnPower();
        programmingTimer = 0;
        programMode = 0;
        resetTheDisplay();
    } else if (programMode == 3) {
        turnOffPower();
        programmingTimer = 0;
        programMode = 0;
        resetTheDisplay();
        // Choose to program the buttons
    } else if (programMode >= 5) {
        // stop everything
        stopNow();
        trainDirection = 0;
        changeFunction(programMode);
    } else {
        // Here we are stopping everything to choose the train address.
        stopNow(); // just making sure we don't have an accident
        trainDirection = 0;
        int digit1 = trainAddress / 1000;
        int digit2 = trainAddress % 1000;
        digit2 = digit2 / 100;
        int digit3 = trainAddress % 100;
        digit3 = digit3 / 10;
        int digit4 = trainAddress % 10;
        Serial.println("Set Address mode");
        display.clearDisplay();
        delay(1000); // let the inputs clear
        display.setCursor(0, 0);
        display.println("Change address");
        display.setCursor(0, 9);
        display.println("digit 1 of 4:");
        digit1 = pickTheDigit(digit1);
        display.setTextColor(BLACK);
        display.setCursor(0, 9);
        display.println("digit 1 of 4:");
        display.setCursor(20, 19);
        display.println("ok");
        display.setTextColor(WHITE);
        display.setCursor(0, 9);
        display.println("digit 2 of 4:");
        digit2 = pickTheDigit(digit2);
        display.setTextColor(BLACK);
        display.setCursor(0, 9);
        display.println("digit 2 of 4:");
        display.setCursor(20, 19);
        display.println("ok");
        display.setTextColor(WHITE);
        display.setCursor(0, 9);
        display.println("digit 3 of 4:");
        digit3 = pickTheDigit(digit3);
        display.setTextColor(BLACK);
        display.setCursor(0, 9);
        display.println("digit 3 of 4:");
        display.setCursor(20, 19);
        display.println("ok");
        display.setTextColor(WHITE);
        display.setCursor(0, 9);
        display.println("digit 4 of 4:");
        digit4 = pickTheDigit(digit4);
        trainAddress = (digit1 * 1000) + (digit2 * 100) + (digit3 * 10) + (digit4);
        chooseLoco();
        writeOutEEPROM();
        resetTheDisplay();
        programmingTimer = 0;
        programMode = 0;
        buttonValues[horn] = 0; // Turn it off so the horn doesn't start blaring out
    }
}

// GO through and pick which function affects whichever switch we picked
// for a reminder here is the default:
// I'm picking my switches to control 0,1,2,3,7,8 since 8 is mute and 7 is dim on my controller
// unsigned char trainFunction[] = {97, 99, 98, 0, 1, 3, 7, 8, 2}; // What functions do you want the buttons to turn on/off?
void changeFunction(int switchNumber) {
    Serial.print("Change function");
    // Based on my 5 switches we need to subtract 2 from the switch number to be at the proper trainFunction[switch]
    switchNumber -= 2;
    int digit1 = trainFunction[switchNumber] / 10;
    int digit2 = trainFunction[switchNumber] % 10;
    display.clearDisplay();
    delay(1000); // let the inputs clear
    display.setCursor(0, 0);
    display.println("Change function");
    display.setCursor(0, 9);
    display.println("digit 1 of 2:");
    digit1 = pickTheDigit(digit1);
    display.setTextColor(BLACK);
    display.setCursor(0, 9);
    display.println("digit 1 of 2:");
    display.setCursor(20, 19);
    display.println("ok");
    display.setTextColor(WHITE);
    display.setCursor(0, 9);
    display.println("digit 2 of 2:");
    digit2 = pickTheDigit(digit2);
    trainFunction[switchNumber] = (digit1 * 10 + digit2);
    writeOutEEPROM();
    resetTheDisplay();
    programmingTimer = 0;
    programMode = 0;
    buttonValues[horn] = 0; // Turn it off so the horn doesn't start blaring out
}


int pickTheDigit(int digit) {
    while (true) {
        display.setCursor(0, 20);
        display.print(digit);
        display.display();
        for (int x = 0; x < sizeof(buttonPins); x++) {
            // we only care about the forward switch and the horn button;
            if (x == horn) { // this rotation is the horn button
                if (digitalRead(buttonPins[x]) == LOW) {
                    display.setCursor(0, 20);
                    display.setTextColor(BLACK);
                    display.print(digit);
                    display.setTextColor(WHITE);
                    display.setCursor(20, 19);
                    display.println("ok");
                    display.display();
                    delay(3000);
                    return digit;
                }
            }
            if (trainFunction[x] == 99) { // this rotation is the forward switch
                if (digitalRead(buttonPins[x]) == LOW) {
                    display.setCursor(0, 20);
                    display.setTextColor(BLACK);
                    display.print(digit);
                    display.setCursor(0, 20);
                    display.setTextColor(WHITE);
                    digit++;
                    if (digit == 10) {
                        digit = 0;
                    }
                    display.print(digit);
                    display.display();
                }
            }
        }
        delay(500);
    }
}
