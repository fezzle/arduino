#include <7segment.h>
#include <font5x7.h>
#include <font8x16.h>
#include <fontlargenumber.h>
#include <MicroView.h>
#include <space01.h>
#include <space02.h>
#include <space03.h>
#include <EEPROM.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <avr/io.h>

#define LIGHT_EEADDR 0
#define MESSAGE_EEADDR 1
#define RELAY_0_PIN 5
#define RELAY_1_PIN 6


enum {  GREEN=0, OFF, RED, YELLOW };

char light;
unsigned long last_change;
char message[20] = { 0 };

char RED_STR[] PROGMEM = "Red";
char GREEN_STR[] PROGMEM = "Grn";
char YELLOW_STR[] PROGMEM = "Ylw";
char OFF_STR[] PROGMEM = "Off";
//prog_char *LIGHT_TABLE[] PROGMEM = { GREEN_STR, OFF_STR, RED_STR, YELLOW_STR };

void Serial_PPrint(char *str) {
  char nextchar = pgm_read_byte_near(str++);
  while (nextchar) {
    Serial.print(nextchar);
    nextchar = pgm_read_byte_near(str++);
  }
}

void strcpy_PF(char *buff, char *pgmstr) {
  char nextchar = pgm_read_byte_near(pgmstr++);
  while (nextchar) {
    *buff++ = nextchar;
    nextchar = pgm_read_byte_near(pgmstr++);
  }
  *buff++ = 0;
}

void strcat_PF(char *buff, char *pgmstr) {
  while (*buff) {
    buff++;
  }
  strcpy_PF(buff, pgmstr);
}

void set_relays(char val) {
  digitalWrite(RELAY_0_PIN, val & 0b001 ? true : false);
  digitalWrite(RELAY_1_PIN, val & 0b010 ? true : false);
  last_change = millis();
}

/* 
** Attempts to handle a command string from user.  
** Returns true if understood, false otherwise.
*/
boolean handle_command(char *commandstr) {
  char *tok = strtok(commandstr, " ");
	
  if (strcmp_P(tok, PSTR("ping")) == 0) {
    Serial_PPrint(PSTR("| ping? pong!\n"));
    
  } else if (strcmp_P(tok, PSTR("light")) == 0) {
    tok = strtok(NULL, " ");

    Serial_PPrint(PSTR("| light switched "));
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
    
    EEPROM.write(LIGHT_EEADDR, light);

    set_relays(light);

  } else if (strcmp_P(tok, PSTR("stats")) == 0) {
    Serial_PPrint(PSTR("| No stats right now!\n"));    

  } else if (strcmp_P(tok, PSTR("help")) == 0) {
    print_help();      

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
  Serial_PPrint(PSTR("  <some message>\n"));
  Serial_PPrint(PSTR("\n\n"));
}

void setup() {
  uView.begin();              // start MicroView
  
  Serial.begin(115200);
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
  static char buffer[20];
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

void strcat_time(char *buffer) {
  char *ptr = buffer;
  while (*ptr) { 
    ptr++;
  }
  
  long seconds = (millis() - last_change) / 1000;
  int hours = seconds / 3600;
  seconds -= (long)hours * 3600;
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


void update_display() {  
  char buffer[15];
  uView.clear(PAGE);          // clear page

  strcpy_PF(buffer, PSTR("T:"));
  strcat_time(buffer);
  uView.setCursor(0, (0)*uView.getFontHeight());
  uView.print(buffer);

  strcpy_PF(buffer, PSTR("Light: "));
  char *lightstr;
  switch (light) {
    case RED: 
      lightstr = RED_STR;
      break;
    case GREEN:
      lightstr = GREEN_STR;
      break;
    case YELLOW:
      lightstr = YELLOW_STR;
      break;
    default:
      lightstr = OFF_STR;
  }
  strcat_PF(buffer, lightstr);
  uView.setCursor(0, (1)*uView.getFontHeight());     
  uView.print(buffer);
  
  uView.setCursor(0, (2)*uView.getFontHeight());
  uView.print(message);
  
  uView.display();
}
 
void loop() {
   check_serial();
   update_display();
}
