// This #include statement was automatically added by the Particle IDE.
#include "S3B.h"

// This #include statement was automatically added by the Particle IDE.
#include <NCD1Relay.h>

STARTUP(WiFi.selectAntenna(ANT_EXTERNAL));

SYSTEM_MODE(MANUAL);

//Global variables
byte masterAddress[8] = {0x00, 0x13, 0xA2, 0x00, 0x41, 0x02, 0x20, 0x43};
unsigned long serialTimeout = 500;
int previousInputState = 256;
byte receiveBuffer[256];
unsigned long tOut = 100;

unsigned long interval = 500;
unsigned long lastSend = 0;

//Global objects
NCD1Relay controller;
S3B module;

unsigned long rssiIntervalOff = 400;
unsigned long lastOff = 0;
bool rssiDisplayStatus = false;

//Function Declorations
void getInfoFromMemory();
void parseReceivedData();
void setMasterAddress();

unsigned long comTimeout = 20000;
unsigned long lastReceive;
bool lostComs = false;

void setup() {
    Serial1.begin(115200);
    RGB.control(true);
    // getInfoFromMemory();
    controller.setAddress(0,0,0);
    byte data[1] = {85};
    module.transmit(masterAddress, data, 1);

}

void loop() {
	//Check Timeout
	if(millis() > lastReceive + comTimeout){
		controller.turnOffRelay();
		lostComs = true;
		RGB.color(255, 0, 0);
	}


    //Check for transmission from Master
    if(Serial1.available() != 0){
        parseReceivedData();
        RGB.color(255, 255, 255);

    }

    //Check Serial port to see if software is connected for setting a new Master Address.
    if(Serial.available() != 0){
        setMasterAddress();
    }
    if(millis() > lastOff+rssiIntervalOff){
        lastOff = millis();
        if(!lostComs){
        	RGB.color(0, 0, 0);
        }

    }
}

void getInfoFromMemory(){
    for(int i = 0; i < 8; i++){
        masterAddress[i] = EEPROM.read(i);
    }
}

void setMasterAddress(){

    int addressIncrementer = 0;
    unsigned long start = millis();
    byte newA[8];
    while(addressIncrementer < 8 && millis() < start+serialTimeout){
        if(Serial.available() != 0){
            newA[addressIncrementer] = Serial.read();
            addressIncrementer++;
        }

    }
    if(addressIncrementer == 8){
        for(int i = 0; i < 8; i++){
        EEPROM.write(i, newA[i]);
        masterAddress[i] = newA[i];
    }
    Serial.printf("Address set to %02x%02x%02x%02x %02x%02x%02x%02x", masterAddress[0], masterAddress[1], masterAddress[2],
    masterAddress[3], masterAddress[4], masterAddress[5], masterAddress[6], masterAddress[7]);
    }else{
        Serial.println("Serial Timeout setting address");
    }
}

void parseReceivedData(){
    // Serial.println("Got a command");
    byte startDelimiter = Serial1.read();
    if(startDelimiter == 0x7E){
        unsigned long startTime = millis();
        while(Serial1.available() < 2 && millis() <= tOut+startTime);
        if(Serial1.available() < 2){
            Serial.println("Timeout");
            return;
        }
        byte lenMSB = Serial1.read();
        byte lenLSB = Serial1.read();
        int newDataLength = (lenMSB*256)+lenLSB;

        int count = 0;
        while(count != newDataLength+1 && millis() <= tOut+startTime){
            if(Serial1.available() != 0){
                receiveBuffer[count] = Serial1.read();
                count++;
            }
        }
        if(count < newDataLength+1){
            Serial.println("Timeout2");
            Serial.printf("Received Bytes: %i, expected %i \n", count, newDataLength+1);
            return;
        }
        // Serial.printf("Received %i bytes \n", count);
        //We have all our data.
        byte newData[newDataLength+4];
        newData[0] = startDelimiter;
        newData[1] = lenMSB;
        newData[2] = lenLSB;
        for(int i = 3; i < newDataLength+4; i++){
            newData[i] = receiveBuffer[i-3];
        }
        // Serial.print("Received: ");
        // for(int i = 0; i < sizeof(newData); i++){
        //     Serial.printf("%x, ", newData[i]);
        // }
        // Serial.println();
        //validate data
        if(!module.validateReceivedData(newData, newDataLength+4)){
            Serial.println("Invalid Data");
            return;
        }
        //get length of new data
        int receivedDataLength = module.getReceiveDataLength(newData);
        char receivedData[receivedDataLength];
        int validDataCount = module.parseReceive(newData, receivedData, newDataLength+4);
        if(validDataCount == receivedDataLength){

        }
        lastReceive = millis();
        lostComs = false;
        switch(receivedData[0]){
            case 0:
            // Serial.printf("Setting status to %i \n", receivedData[1]);
            if(receivedData[1] == 0){
            	controller.turnOffRelay();
            }else{
            	if(receivedData[1] == 1){
            		controller.turnOnRelay();
            	}
            }
            //Check input status and send to master
            byte data[1] = {85};
            int tries = 0;
            retry:
            if(module.transmit(masterAddress, data, 1)){
                Serial.println("Transmission to master success");
            }else{
                Serial.println("Transmission to master failed");
                tries++;
                if(tries < 5){
                    goto retry;
                }
            }


            break;
        }


    }else{
        Serial.printf("First byte not valid, it was: 0x%x \n", startDelimiter);
    }
}

