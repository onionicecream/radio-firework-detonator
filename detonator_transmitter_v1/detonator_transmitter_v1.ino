#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "printf.h"
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define s 0  //toggle serial debugging !!! serial communication utilises TX(1) and RX(0), thus keypad is not working when serial is on !!!
#define d 0  //startup delays

const byte ROWS = 5;
const byte COLS = 3;

char hexaKeys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' },
  { 'A', 'B', 'G' }
};

byte rowPins[ROWS] = { 3, 4, 5, 6, 7 };
byte colPins[COLS] = { 0, 1, 2 };

Keypad kpd = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

RF24 radio(9, 10);  // CE, CSN

typedef struct
{
  bool alpha = 0;
  bool beta = 0;
  bool gamma = 0;
  bool test = 0;
} controlDef;

//i don't know why it didn't work to have both ignitors and continuity be the same controlDef type
typedef struct
{
  bool alpha = 0;
  bool beta = 0;
  bool gamma = 0;
} continuity_t;

controlDef ignitors;
continuity_t continuity;

char ping_sent[] = "test123";
char ping_received[8] = "";

LiquidCrystal_I2C lcd(0x3F, 20, 4);  // address for my module is 0x3F

byte alpha[] = {
  B00000,
  B00000,
  B01001,
  B10101,
  B10010,
  B10010,
  B01101,
  B00000
};
byte beta[] = {
  B00000,
  B00000,
  B01110,
  B10001,
  B11110,
  B10001,
  B11110,
  B10000
};
byte gamma[] = {
  B00000,
  B00001,
  B11010,
  B10100,
  B00100,
  B00100,
  B00100,
  B00000
};
byte lock[] = {
  B01110,
  B10001,
  B10001,
  B11111,
  B11111,
  B11011,
  B11011,
  B11111
};
byte tick[] = {
  B00000,
  B00000,
  B00001,
  B00011,
  B10110,
  B11100,
  B01000,
  B00000
};

#define ok_alpha A0
#define ok_beta A2   //my mistake!!! A6 and A7 cannot be used as outputs!, rectified by switching around
#define ok_gamma A3  //
#define switch_alpha A1
#define switch_beta A6
#define switch_gamma A7

#define arm 8

//convert analog input in digital input for pins A6 and A7
#define digital_read_switch_beta (analogRead(switch_beta) > 400)
#define digital_read_switch_gamma (analogRead(switch_gamma) > 400)

uint8_t address[][6] = { "1Node", "2Node" };

char pincode[7] = "";  //six number pin code
char reference[] = "749382";

bool access = true;
bool showpassword = false;
bool armed = false;

char timer_selector = 'x';

/**
MOMENTARY push=on
TOGGLE on/off
UNTIL_DISCONT on until continuity is interrupted
TIMER ignition with a delay
*/
enum mode { MOMENTARY,
            TOGGLE,
            UNTIL_DISCONT,
            TIMER };

enum mode ignition_mode;

enum which { ALPHA,
             BETA,
             GAMMA };

typedef struct {
  char s_sec[5] = "";
  int sec = 0;
  bool start = false;
  unsigned long start_msec = 0;
} timer;

timer timer_alpha, timer_beta, timer_gamma;

void setup() {
  pinMode(ok_alpha, OUTPUT);
  pinMode(ok_beta, OUTPUT);
  pinMode(ok_gamma, OUTPUT);

  pinMode(switch_alpha, INPUT);
  pinMode(switch_beta, INPUT);
  pinMode(switch_gamma, INPUT);

  pinMode(arm, OUTPUT);
  digitalWrite(arm, LOW);

  lcd.init();  // initialize the lcd

  lcd.createChar(0, alpha);
  lcd.createChar(1, beta);
  lcd.createChar(2, gamma);
  lcd.createChar(3, lock);
  lcd.createChar(4, tick);

  lcd.backlight();  // turn on backlight
  lcd.setCursor(2, 0);
  lcd.print("THE ULTIMATE");
  lcd.setCursor(3, 1);
  lcd.print("DETONATOR");

  if (d) delay(5000);

  lcd.clear();

  if (s) {
    Serial.begin(9600);
    Serial.println("Serial connected");
  }
  do {
    delay(100);
    radio.begin();
    radio.setPALevel(RF24_PA_MIN);
    radio.openWritingPipe(address[0]);
    radio.openReadingPipe(1, address[1]);
    radio.stopListening();

    if (radio.isChipConnected()) {
      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print("COMM MODULE");
      lcd.setCursor(3, 1);
      lcd.print("CONNECTED!");
      break;
    } else {
      lcd.setCursor(5, 0);
      lcd.print("ERROR");
      lcd.setCursor(2, 1);
      lcd.print("COMM MODULE!");
    }
  } while (true);

  if (d) delay(2500);

  lcd.clear();

  while (true) {
    display_connection_check();
    display_confirm_ping();
    while (kpd.getKey() != '*')
      ;
    lcd.clear();
    radio.write(&ping_sent, sizeof(ping_sent));
    radio.startListening();
    while (!radio.available())
      if (kpd.getKey() == '*') break;  //wait until message is available
    radio.read(&ping_received, sizeof(ping_received));
    delay(100);
    radio.stopListening();

    if (s) {
      Serial.print("received:");
      Serial.println(ping_received);
      Serial.print("reference:");
      Serial.println(ping_sent);
      Serial.print("strcmp=");
      Serial.println(strcmp(ping_received, ping_sent));
    }
    if (strcmp(ping_received, ping_sent) == 0) {
      display_connection_ok();
      break;
    } else {
      display_connection_error();
    }
  }
}

void loop() {
  char key = kpd.getKey();

  if (!access) {
    display_password(showpassword);

    if (key != 0) {
      if (s)
        if (key != 0) Serial.println(key);

      if (key == 'A' || key == 'B' || key == 'G') {  // Check if 'key' is one of 'A', 'B', or 'G'
        display_error_message();
      } else if (key >= '0' && key <= '9') {
        if (s) {
          Serial.print("Pincode: ");
          Serial.println(pincode);
        }
        if (strlen(pincode) < 6) {
          char str[2] = { key, 0 };
          strcat(pincode, str);
          delay(100);
        }
        if (s) {
          Serial.print("new pin: ");
          Serial.println(pincode);
        }
      } else if (key == '#') {
        if (s) Serial.println("delete last char");
        if (pincode[0] != '\0') pincode[strlen(pincode) - 1] = '\0';
      } else if (key == '*') {
        display_passwrod_menu();
        while (true) {
          key = kpd.getKey();
          if (key == '1') {
            showpassword = !showpassword;
            break;
          } else if (key != 0) break;
        }
        lcd.clear();
      }
    }

    if (strlen(pincode) == 6) {
      if (s) {
        Serial.print("pincode:   ");
        Serial.println(pincode);
      }
      if (s) {
        Serial.print("reference: ");
        Serial.println(reference);
      }
      if (s) {
        Serial.print("strcmp: ");
        Serial.println(strcmp(pincode, reference), DEC);
      }
      if (strcmp(pincode, reference) == 0) {
        display_password_correct();
        access = true;
      } else {
        display_password_wrong();
        pincode[0] = '\0';
      }
    }
  }

  if (access) {
    if (!armed) {
      display_unarmed();
      while (true) {
        key = kpd.getKey();
        if (key == '*') {
          display_arming();
          delay(1000);
          lcd.clear();
          armed = true;
          digitalWrite(arm, HIGH);

          break;
        }
      }
    } else {
      // display_continuity(); //used for debugging

      if (key == '*') {
        display_armed_menu();
        while (true) {
          key = kpd.getKey();
          switch (key) {
            case '1':
              ignition_mode = MOMENTARY;
              display_armed_menu();
              break;
            case '2':
              ignition_mode = TOGGLE;
              display_armed_menu();
              break;
            // case '3':
            //   ignition_mode = UNTIL_DISCONT;
            //   display_armed_menu();
            //   break;
            case '3':
              ignition_mode = TIMER;
              display_armed_menu();
              break;
            default: break;
          }
          if (key == '*') {
            lcd.clear();
            break;
          }
        }
      }

      switch (ignition_mode) {
        case MOMENTARY:
          display_armed();
          if (digitalRead(switch_alpha) == LOW) {
            if (kpd.isPressed('A')) ignitors.alpha = 1;
            if (kpd.getState() == RELEASED) ignitors.alpha = 0;
          }
          if (digital_read_switch_beta == LOW) {
            if (kpd.isPressed('B')) ignitors.beta = 1;
            if (kpd.getState() == RELEASED) ignitors.beta = 0;
          }
          if (digital_read_switch_gamma == LOW) {
            if (kpd.isPressed('G')) ignitors.gamma = 1;
            if (kpd.getState() == RELEASED) ignitors.gamma = 0;
          }
          break;  // end case MOMENTARY
        case TOGGLE:
          display_armed();
          if (digitalRead(switch_alpha) == LOW)
            if (key == 'A') ignitors.alpha = !ignitors.alpha;
          if (digital_read_switch_beta == LOW)
            if (key == 'B') ignitors.beta = !ignitors.beta;
          if (digital_read_switch_gamma == LOW)
            if (key == 'G') ignitors.gamma = !ignitors.gamma;
          break;  // end case TOGGLE
        // case UNTIL_DISCONT:
        //   if (digitalRead(switch_alpha) == LOW)
        //     if (kpd.isPressed('A')) ignitors.alpha = 1;
        //   if (continuity.alpha == 0) ignitors.alpha = 0;

        //   if (digital_read_switch_beta == LOW)
        //     if (kpd.isPressed('B')) ignitors.beta = 1;
        //   if (continuity.beta == 0) ignitors.beta = 0;

        //   if (digital_read_switch_gamma == LOW)
        //     if (kpd.isPressed('G')) ignitors.gamma = 1;
        //   if (continuity.gamma == 0) ignitors.gamma = 0;
        //   break;
        case TIMER:
          switch (timer_selector) {
            case '1':
              display_timer_selector(ALPHA,key);
              break;
            case '2':
              display_timer_selector(BETA,key);
              break;
            case '3':
              display_timer_selector(GAMMA,key);
              break;
            default:
              timer_selector = key;
              lcd.noBlink();
              display_timer();
              break;
          }
          if (key == '#') timer_selector = 'x';
          if (digitalRead(switch_alpha) == LOW && key == 'A' && timer_alpha.sec > 0) timer_alpha.start = !timer_alpha.start;
          if (digitalRead(switch_beta) == LOW && key == 'B' && timer_beta.sec > 0) timer_beta.start = !timer_beta.start;
          if (digitalRead(switch_gamma) == LOW && key == 'G' && timer_gamma.sec > 0) timer_gamma.start = !timer_gamma.start;
          decrease_timer();
          if (digitalRead(switch_alpha) == LOW && timer_alpha.start && timer_alpha.sec == 0) {
            ignitors.alpha = 1;
            timer_alpha.start = 0;
          }
          if (digitalRead(switch_alpha) == LOW && timer_beta.start && timer_beta.sec == 0) {
            ignitors.beta = 1;
            timer_beta.start = 0;
          }
          if (digitalRead(switch_alpha) == LOW && timer_gamma.start && timer_gamma.sec == 0) {
            ignitors.gamma = 1;
            timer_gamma.start = 0;
          }
          break;  //end case TIMER
      }

      radio.startListening();
      for (int i = 0; i < 3; i++) {
        if (radio.available()) {
          radio.read(&continuity, sizeof(continuity));
        }
      }
      delay(100);
      radio.stopListening();
      radio.write(&ignitors, sizeof(ignitors));
      delay(100);

      digitalWrite(ok_alpha, continuity.alpha);
      digitalWrite(ok_beta, continuity.beta);
      digitalWrite(ok_gamma, continuity.gamma);
    }
  }
}

void display_connection_check() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Checking");
  lcd.setCursor(0, 1);
  lcd.print("connection...");
  delay(1000);
}

void display_confirm_ping() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("To ping receiver");
  lcd.setCursor(4, 1);
  lcd.print("press '*'");
}

void display_connection_ok() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connection");
  lcd.setCursor(0, 1);
  lcd.print("verified");
  delay(1000);
  lcd.clear();
}

void display_connection_error() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connection");
  lcd.setCursor(0, 1);
  lcd.print("failed");
  delay(1000);
  lcd.clear();
}

void display_password(bool showpassword_val) {
  lcd.setCursor(1, 0);
  lcd.print("Enter pincode:");

  int i = 0;
  for (i; i < strlen(pincode); i++) {
    lcd.setCursor(5 + i, 1);
    if (showpassword_val) lcd.print(pincode[i]);
    else lcd.print('*');
  }
  for (int j = 0; j < (6 - strlen(pincode)); j++) {
    lcd.setCursor(5 + i + j, 1);
    lcd.print("_");
  }
}

void display_error_message() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("UNAUTHORIZED");
  lcd.setCursor(5, 1);
  lcd.print("ACTION");
  delay(2000);
  lcd.clear();
}

void display_passwrod_menu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1=tgl show pin");
  lcd.setCursor(0, 1);
  lcd.print("#=delete last");
}

void display_password_correct() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CORRECT  PINCODE");
  lcd.setCursor(0, 1);
  lcd.print(" ACCESS GRANTED");
  delay(2000);
  lcd.clear();
}

void display_password_wrong() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" WRONG PINCODE");
  lcd.setCursor(0, 1);
  lcd.print(" ACCESS DENIED");
  delay(2000);
  lcd.clear();
}

void display_unarmed() {
  lcd.setCursor(0, 0);
  lcd.print("To arm detonator");
  lcd.setCursor(4, 1);
  lcd.print("press '*'");
}

void display_arming() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Arming device...");
  delay(1000);
}

void display_armed() {
  lcd.setCursor(0, 0);
  lcd.print(">");

  lcd.setCursor(4, 0);
  lcd.write(0);  //alpha
  lcd.setCursor(8, 0);
  lcd.write(1);  //beta
  lcd.setCursor(12, 0);
  lcd.write(2);  //gamma

  lcd.setCursor(0, 1);
  lcd.print(">");

  lcd.setCursor(4, 1);
  if (digitalRead(switch_alpha) == LOW) lcd.write(4);  //lock
  else lcd.write(3);                                   //tick

  lcd.setCursor(8, 1);
  if (digital_read_switch_beta == LOW) lcd.write(4);  //lock
  else lcd.write(3);                                  //tick

  lcd.setCursor(12, 1);
  if (digital_read_switch_gamma == LOW) lcd.write(4);  //lock
  else lcd.write(3);                                   //tick
}

void display_armed_menu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Curr:");

  lcd.setCursor(5, 0);
  switch (ignition_mode) {
    case MOMENTARY:
      lcd.print("MOMENTARY");
      break;
    case TOGGLE:
      lcd.print("TOGGLE");
      break;
    // case UNTIL_DISCONT:
    //   lcd.print("TIL DISCONT");
    //   break;
    case TIMER:
      lcd.print("TIMER");
      break;
  }

  lcd.setCursor(0, 1);
  lcd.print("1MMT 2TGL 3TIMER");
}

void display_continuity() {
  lcd.setCursor(5, 0);
  if (continuity.alpha) lcd.print("1");
  else lcd.print("0");

  lcd.setCursor(9, 0);
  if (continuity.beta) lcd.print("1");
  else lcd.print("0");

  lcd.setCursor(13, 0);
  if (continuity.gamma) lcd.print("1");
  else lcd.print("0");
}

void display_timer() {
  //first row
  lcd.setCursor(0, 0);
  lcd.print(">");

  lcd.setCursor(4, 0);
  lcd.write(0);  //alpha
  lcd.setCursor(9, 0);
  lcd.write(1);  //beta
  lcd.setCursor(14, 0);
  lcd.write(2);  //gamma

  lcd.setCursor(5, 0);
  if (digitalRead(switch_alpha) == LOW) lcd.write(4);  //lock
  else lcd.write(3);                                   //tick

  lcd.setCursor(10, 0);
  if (digital_read_switch_beta == LOW) lcd.write(4);  //lock
  else lcd.write(3);                                  //tick

  lcd.setCursor(15, 0);
  if (digital_read_switch_gamma == LOW) lcd.write(4);  //lock
  else lcd.write(3);                                   //tick

  //second row
  lcd.setCursor(0, 1);
  lcd.print(" ");

  if ((timer_alpha.sec) == 0) {
    lcd.setCursor(4, 1);
    lcd.print(">1");
  } else {
    display_timer_individual(ALPHA);
  }

  if ((timer_beta.sec) == 0) {
    lcd.setCursor(9, 1);
    lcd.print(">2");
  } else {
    display_timer_individual(BETA);
  }

  if ((timer_gamma.sec) == 0) {
    lcd.setCursor(14, 1);
    lcd.print(">3");
  } else {
    display_timer_individual(GAMMA);
  }
}

void display_timer_individual(which which_one) {
  lcd.setCursor(0, 1);
  lcd.print("s");

  switch (which_one) {
    case ALPHA:
      lcd.setCursor(3, 1);
      lcd.printstr(timer_alpha.s_sec);
      break;
    case BETA:
      lcd.setCursor(8, 1);
      lcd.printstr(timer_alpha.s_sec);
      break;
    case GAMMA:
      lcd.setCursor(13, 1);
      lcd.printstr(timer_gamma.s_sec);
      break;
  }
}

void display_timer_selector(which which_one, char key) {
  display_timer_individual(which_one);
  lcd.blink();
  lcd.print("                ");//clear bottom row;

  int len = 0;
  //char key = kpd.getKey();
  switch (which_one) {
    case ALPHA:
      len = strlen(timer_alpha.s_sec);
      if (len < 4) {
        lcd.setCursor(3 + len, 1);
        if (key <= '9' && key >= '0') timer_alpha.s_sec[len] = key;
        //else if (key == '#') timer_alpha.s_sec[len] = '\0';
      }
      break;
    case BETA:
      len = strlen(timer_beta.s_sec);
      if (len < 4) {
        lcd.setCursor(8 + len, 1);
        if (key <= '9' && key >= '0') timer_beta.s_sec[len] = key;
        //else if (key == '#') timer_beta.s_sec[len] = '\0';
      }
      break;
    case GAMMA:
      len = strlen(timer_gamma.s_sec);
      if (len < 4) {
        lcd.setCursor(13 + len, 1);
        if (key <= '9' && key >= '0') timer_gamma.s_sec[len] = key;
        //else if (key == '#') timer_gamma.s_sec[len] = '\0';
      }
      break;
  }
}

void decrease_timer() {
  int digit = 0;
  for (int i = 0; i > 4; i++) {
    if (timer_alpha.start) {  //integer to string
      digit = timer_alpha.sec / pow(10, 3 - i);
      digit = digit % 10;
      timer_alpha.s_sec[i] = digit;
    } else {  //string to integer
      digit = (timer_alpha.s_sec[i] <= '9' && timer_alpha.s_sec[i] >= '0') ? (timer_alpha.s_sec[i] - '0') : 0;
      timer_alpha.sec += pow(10, 3 - i) * digit;
    }
  }
  for (int i = 0; i > 4; i++) {
    if (timer_beta.start) {  //integer to string
      digit = timer_beta.sec / pow(10, 3 - i);
      digit = digit % 10;
      timer_beta.s_sec[i] = digit;
    } else {  //string to integer
      digit = (timer_beta.s_sec[i] <= '9' && timer_beta.s_sec[i] >= '0') ? (timer_beta.s_sec[i] - '0') : 0;
      timer_beta.sec += pow(10, 3 - i) * digit;
    }
  }
  for (int i = 0; i > 4; i++) {
    if (timer_gamma.start) {  //integer to string
      digit = timer_gamma.sec / pow(10, 3 - i);
      digit = digit % 10;
      timer_alpha.s_sec[i] = digit;
    } else {  //string to integer
      digit = (timer_gamma.s_sec[i] <= '9' && timer_gamma.s_sec[i] >= '0') ? (timer_gamma.s_sec[i] - '0') : 0;
      timer_alpha.sec += pow(10, 3 - i) * digit;
    }
  }

  if (!timer_alpha.start) {
    timer_alpha.start_msec = millis();
  } else {
    if (millis() - timer_alpha.start_msec > 1000) {
      if (timer_alpha.sec > 0) timer_alpha.sec--;
      timer_alpha.start_msec = millis();
    }
  }

  if (!timer_beta.start) {
    timer_beta.start_msec = millis();
  } else {
    if (millis() - timer_beta.start_msec > 1000) {
      if (timer_beta.sec > 0) timer_beta.sec--;
      timer_beta.start_msec = millis();
    }
  }

  if (!timer_gamma.start) {
    timer_gamma.start_msec = millis();
  } else {
    if (millis() - timer_gamma.start_msec > 1000) {
      if (timer_gamma.sec > 0) timer_gamma.sec--;
      timer_gamma.start_msec = millis();
    }
  }
}