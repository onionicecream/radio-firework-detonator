#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "printf.h"

RF24 radio(9, 10); // CE, CSN
  
int button1 = 5;
int button2 = 6;
int button3 = 7;

typedef struct
{
boolean bs1 = 0;
boolean bs2 = 0;
boolean bs3 = 0;
}
controlDef;

controlDef controlPak;

//boolean button_test = 0;

//////////////////////////////

uint8_t address[][6] = {"1Node", "2Node"};

void setup() {
  
pinMode(button1, INPUT);
pinMode(button2, INPUT);
pinMode(button3, INPUT);

radio.begin();
radio.setPALevel(RF24_PA_MIN);
radio.openWritingPipe(address[0]);
radio.openReadingPipe(1, address[1]);
radio.stopListening();

}

void loop() {

controlPak.bs1 = digitalRead(button1);
controlPak.bs2 = digitalRead(button2);
controlPak.bs3 = digitalRead(button3);

//button_test = digitalRead(button1);

radio.write(&controlPak, sizeof(controlPak));
//radio.write(&button_test, sizeof(button_test));

delay(200);

}
