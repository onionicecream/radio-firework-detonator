#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "printf.h"

RF24 radio(9, 10); // CE, CSN

typedef struct
{
boolean bs1 = 0;
boolean bs2 = 0;
boolean bs3 = 0;
}
controlDef;

//boolean button_test = 0;

controlDef controlPak;

int led1 = 4;
int led2 = 6;
int led3 = 7;


uint8_t address[][6] = {"1Node", "2Node"};

void setup() {


  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);

  radio.begin();  
  radio.setPALevel(RF24_PA_MIN); 
  radio.openWritingPipe(address [1]);
  radio.openReadingPipe(1, address[0]);      
  radio.startListening();          
  
}

void loop() {

  if (radio.available())
  {
    radio.read(&controlPak, sizeof(controlPak));
    
//    radio.read(&button_test, sizeof(button_test));

//    if(button_test == 1)
//      {
//        digitalWrite(led1, HIGH);
//      }
//      else
//      {
//        digitalWrite(led1, LOW);
//      }

    if(controlPak.bs1 == 1)
      {
      digitalWrite(led1, HIGH);
      }
    else
      {
      digitalWrite(led1, LOW);
      }

      if(controlPak.bs2 == 1)
      {
      digitalWrite(led2, HIGH);
      }
   else
      {
      digitalWrite(led2, LOW);
      }

      if(controlPak.bs3 == 1)
      {
      digitalWrite(led3, HIGH);
      }
   else
      {
      digitalWrite(led3, LOW);
      }
  }
  delay(100);
}
