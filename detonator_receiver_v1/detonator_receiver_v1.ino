#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "printf.h"

RF24 radio(9, 10);  // CE, CSN

typedef struct
{
  boolean alpha = 0;
  boolean beta = 0;
  boolean gamma = 0;
  boolean test = 0;
} controlDef;

typedef struct
{
  bool alpha = 0;
  bool beta = 0;
  bool gamma = 0;
} continuity_t;

controlDef ignitors;
continuity_t continuity;

char ping_reference[] = "test123";
char ping_received[8] = "";

#define r_alpha A2
#define r_beta A1
#define r_gamma A5
#define led_alpha 4
#define led_beta 3
#define led_gamma 2
#define alpha_ldr A0
#define beta_ldr A4
#define gamma_ldr A7
#define SW 8

int ldr_treshold = 600;

uint8_t address[][6] = { "1Node", "2Node" };

#define s 0

void setup() {
  pinMode(r_alpha, OUTPUT);
  pinMode(r_beta, OUTPUT);
  pinMode(r_gamma, OUTPUT);

  pinMode(led_alpha, OUTPUT);
  pinMode(led_beta, OUTPUT);
  pinMode(led_gamma, OUTPUT);

  pinMode(alpha_ldr, INPUT);
  pinMode(beta_ldr, INPUT);
  pinMode(gamma_ldr, INPUT);

  pinMode(SW, INPUT_PULLUP);

  if (s) {
    Serial.begin(9600);
    Serial.println("serial connected!");
  }

  radio.begin();
  radio.setPALevel(RF24_PA_MIN);
  radio.openWritingPipe(address[1]);
  radio.openReadingPipe(1, address[0]);

  radio.startListening();
  while (!radio.available())
    ;

  radio.read(&ping_received, sizeof(ping_received));
  radio.stopListening();

  if (strcmp(ping_received, ping_reference) == 0) {
    flash_leds();
  }
  if (s) {
    Serial.print("received:");
    Serial.println(ping_received);
    Serial.print("reference:");
    Serial.println(ping_reference);
    Serial.print("strcmp=");
    Serial.println(strcmp(ping_received, ping_reference));
  }
  delay(100);

  for (int i = 0; i < 4 ; i++)  {
    radio.write(&ping_received, sizeof(ping_received));
    delay(200);
  }
}

void loop() {
  radio.startListening();
  if (radio.available()) {
    radio.read(&ignitors, sizeof(ignitors));
  }

  digitalWrite(led_alpha, ignitors.alpha);
  digitalWrite(r_alpha, ignitors.alpha);

  digitalWrite(led_beta, ignitors.beta);
  digitalWrite(r_beta, ignitors.beta);

  digitalWrite(led_gamma, ignitors.gamma);
  digitalWrite(r_gamma, ignitors.gamma);

  delay(100);

  continuity.alpha = (analogRead(alpha_ldr) > ldr_treshold);
  continuity.beta = (analogRead(beta_ldr) > ldr_treshold);
  continuity.gamma = (analogRead(gamma_ldr) > ldr_treshold);

  if (!digitalRead(SW)) {
    while (!digitalRead(SW)) {
      digitalWrite(led_alpha, continuity.alpha);
      digitalWrite(led_beta, continuity.beta);
      digitalWrite(led_gamma, continuity.gamma);
    }
    delay(100);
    digitalWrite(led_alpha, LOW);
    digitalWrite(led_beta, LOW);
    digitalWrite(led_gamma, LOW);
  }

  if(s) {
    Serial.print("continuity.alpha=");
    Serial.print(continuity.alpha);
    Serial.print("; continuity.beta=");
    Serial.print(continuity.beta);
    Serial.print("; continuity.gamma=");
    Serial.println(continuity.gamma);
  }

  delay(10);
  radio.stopListening();

  radio.write(&continuity, sizeof(continuity));
  delay(10);
}

void flash_leds() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(led_alpha, HIGH);
    digitalWrite(led_beta, HIGH);
    digitalWrite(led_gamma, HIGH);

    delay(400);

    digitalWrite(led_alpha, LOW);
    digitalWrite(led_beta, LOW);
    digitalWrite(led_gamma, LOW);

    delay(400);
  }
}