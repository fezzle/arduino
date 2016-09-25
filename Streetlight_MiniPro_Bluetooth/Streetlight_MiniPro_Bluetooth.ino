#include <EEPROM.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <avr/io.h>

#define LIGHT_EEADDR 0
#define MESSAGE_EEADDR 1
#define MESSAGE_MAXLEN 40

#define RELAY_0_PIN 2
#define RELAY_1_PIN 3


enum {  GREEN=0, OFF, YELLOW, RED,  WORK, BREAK };

unsigned long pomodoro_time = 0;
char pomodoro_state = OFF;

char light;
unsigned long last_change;
char message[MESSAGE_MAXLEN + 1] = { 0 };

const char RED_STR[] PROGMEM = "Red";
const char GREEN_STR[] PROGMEM = "Green";
const char YELLOW_STR[] PROGMEM = "Yellow";
const char OFF_STR[] PROGMEM = "Off";

void Serial_PPrint(const char *str) {
  char nextchar = pgm_read_byte_near(str++);
  while (nextchar) {
    Serial.print(nextchar);
    nextchar = pgm_read_byte_near(str++);
  }
}

void set_pomodoro_work() {
  pomodoro_state = WORK;
  pomodoro_time = millis() + (long)25*60*1000;
  set_relays(RED);
}

void set_pomodoro_break() {
  pomodoro_state = BREAK;
  pomodoro_time = millis() + (long)5*60*1000;
  set_relays(GREEN);
}

void set_pomodoro_off() {
  pomodoro_state = OFF;
  pomodoro_time = 0;
  set_relays(OFF);
}

void set_relays(char val) {
  digitalWrite(RELAY_0_PIN, val & 0b001 ? true : false);
  digitalWrite(RELAY_1_PIN, val & 0b010 ? true : false);
  last_change = millis();
  EEPROM.write(LIGHT_EEADDR, light);
}

/* 
** Attempts to handle a command string from user.  
** Returns true if understood, false otherwise.
*/
boolean handle_command(char *commandstr) {
  char *tok = strtok(commandstr, " ");
	
  if (strcmp_P(tok, PSTR("ping")) == 0) {
    Serial_PPrint(PSTR("ping? pong!\n"));
    
  } else if (strcmp_P(tok, PSTR("light")) == 0) {
    tok = strtok(NULL, " ");

    Serial_PPrint(PSTR("light switched: "));
    if (strcmp_P(tok, PSTR("red")) == 0) {
      light = RED;
      Serial_PPrint(PSTR("red"));
    } else if (strcmp_P(tok, PSTR("yellow")) == 0) {
      light = YELLOW;
      Serial_PPrint(PSTR("yellow"));
    } else if (strcmp_P(tok, PSTR("green")) == 0) {
      light = GREEN;
      Serial_PPrint(PSTR("green"));
    } else {
      light = OFF;
      Serial_PPrint(PSTR("off"));
    }
    Serial.print('\n');
    set_pomodoro_off();
    set_relays(light);

  } else if (strcmp_P(tok, PSTR("stats")) == 0) {
    Serial_PPrint(PSTR("Light:"));
    switch (light) {
      case YELLOW:
        Serial_PPrint(YELLOW_STR);
        break;
      case RED:
        Serial_PPrint(RED_STR);
        break;
      case GREEN:
        Serial_PPrint(GREEN_STR);
        break;
      default:
        Serial_PPrint(OFF_STR);
        break;
    }
    Serial.print('\n');

    char buffer[20];
    if (pomodoro_state == WORK || pomodoro_state == BREAK) {
      Serial_PPrint(PSTR("Pomodoro state: "));
      if (pomodoro_state == WORK) {
        Serial_PPrint(PSTR("WORK"));
      } else {
        Serial_PPrint(PSTR("BREAK"));
      }
      Serial_PPrint(PSTR(" for "));
      strcpy_time(pomodoro_time - millis(), buffer);
      Serial.print(buffer);
      Serial_PPrint(PSTR(" seconds\n"));
      
    } else {
      Serial_PPrint(PSTR("Time in state: "));
      strcpy_time(millis() - last_change, buffer);
      Serial.println(buffer);
      Serial_PPrint(PSTR("Message: "));
      Serial.println(message);
    }
    
  } else if (strcmp_P(tok, PSTR("help")) == 0) {
    print_help();
  } else if (strcmp_P(tok, PSTR("pomodoro")) == 0) {
    tok = strtok(NULL, " ");

    if (strcmp_P(tok, PSTR("start")) == 0) {
      set_pomodoro_work();
    } else {
      set_pomodoro_off();
    }

  } else { 
    strlcpy(message, tok, sizeof(message));
    char toklen = strlen(message);
    if (toklen < sizeof(message) - 1) {
      // add the space back in after it was removed during tokenization
      message[toklen++] = ' ';
      message[toklen  ] = '\0';
    } 
    strlcat(message, strtok(NULL, '\0'), sizeof(message));
    Serial_PPrint(PSTR("Accepted message, enter \"help\" for help\n"));
    Serial.print("message:");
    Serial.println(message);
    for (int i=MESSAGE_EEADDR; i<sizeof(message); i++) {
      EEPROM.write(i, message[i-MESSAGE_EEADDR]);
      if (!message[i-MESSAGE_EEADDR]) {
        break;
      }
    }
  }
  
  return true;
}

void print_help() {
  Serial_PPrint(PSTR(" Commands: \n"));
  Serial_PPrint(PSTR("  ping\n"));
  Serial_PPrint(PSTR("  light (red|yellow|green|off)\n"));
  Serial_PPrint(PSTR("  stats\n"));
  Serial_PPrint(PSTR("  pomodoro (start|off)\n"));
  Serial_PPrint(PSTR("  <some message>\n"));
  Serial_PPrint(PSTR("\n\n"));
}

void setup() { 
  Serial.begin(57600);
  Serial.setTimeout(1000);
  pinMode(RELAY_0_PIN, OUTPUT); 
  pinMode(RELAY_1_PIN, OUTPUT); 
  last_change = millis();
  light = EEPROM.read(LIGHT_EEADDR);
  set_relays(light);  
  
  int i;
  for (i=MESSAGE_EEADDR; (i-MESSAGE_EEADDR) < sizeof(message)-1; i++) {
    message[i-MESSAGE_EEADDR] = EEPROM.read(i);
    if (!message[i-MESSAGE_EEADDR]) {
      break;
    }
  }
  message[i-MESSAGE_EEADDR] = '\0';
}


void check_serial() {
  static char buffer[41];
  static int pos = 0;
  
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || pos == sizeof(buffer)-1) {
      buffer[pos] = 0;
      if (!handle_command(buffer)) {
        print_help();
      }        
      pos = 0;
    } else {
      buffer[pos++] = c;
    }
  }
}

int next_digit(int *source, int mantissa) {
  int n = 0;
  while (*source > mantissa) {
   n++;
   *source -= mantissa;
  }
  return n;
}

void strcpy_time(unsigned long time, char *buffer) {
  char *ptr = buffer;
  
  unsigned long seconds = time / 1000;
  int hours = seconds / 3600L;
  seconds -= (long)hours * 3600L;
  int minutes = seconds / 60;
  seconds -= minutes * 60;
  if (hours > 99) {
    hours = 99;
  }
  int secondsi = (int)seconds;
  
  *ptr++ = '0' + next_digit(&hours, 10);
  *ptr++ = '0' + next_digit(&hours, 1);
  *ptr++ = ':';
  *ptr++ = '0' + next_digit(&minutes, 10);  
  *ptr++ = '0' + next_digit(&minutes, 1);  
  *ptr++ = ':';
  *ptr++ = '0' + next_digit(&secondsi, 10);  
  *ptr++ = '0' + next_digit(&secondsi, 1);
  *ptr++ = 0;
}

void loop() {
  check_serial();
  if ((pomodoro_state == BREAK || pomodoro_state == WORK) 
        && millis() >= pomodoro_time) {
    if (pomodoro_state == BREAK) {
      set_pomodoro_work();
    } else {
      set_pomodoro_break();
    }
    delay(1000);
  }
}
