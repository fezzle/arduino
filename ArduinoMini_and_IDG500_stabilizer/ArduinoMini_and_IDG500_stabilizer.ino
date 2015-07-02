/*
** A Gyro based stabilizer for an Arduino 328/8mhz based on the 
** Invensense IDG-500 gyro.
 */
 
// gyro analog X,Y Axis input pin
#define GYRO_X A2
#define GYRO_Y A3

// analog input pins for X,Y axis servo set-point
#define CALIBRATE_X A0
#define CALIBRATE_Y A1

// X and Y Axis Servo PWM signal ping 
#define CHANNEL_X 3
#define CHANNEL_Y 2

// PWM signal zero point high duration
#define PWM_PREAMBLE_US 800

// drifts back to set-point at roughly this degrees/sec
#define DRIFT_BACK_TO_CENTER_RATE 10L

struct SERVO {
  int adc_bias;
  int bias_acc;
  char direction;
  int expo;
  long rotation;
  unsigned long last_sample;
  unsigned int pulsewidth_us;
};

struct SERVO servo_x = { 0, 0, -1, 512, 0, 0, 0 };
struct SERVO servo_y = { 0, 0, 1, 512, 0, 0, 0 };

void setup() {
  // initialize serial communication at 9600 8n1
  Serial.begin(9600);

  pinMode(CHANNEL_X, OUTPUT);
  pinMode(CHANNEL_Y, OUTPUT);
  
  delay(1000);

  // set biases for gyro pins
  servo_x.adc_bias = analogRead(GYRO_X);
  servo_x.last_sample = millis();
  servo_y.adc_bias = analogRead(GYRO_Y);
  servo_y.last_sample = millis();
}

/*
** Reads the supplied gyro axis pin, calibration pin and updates the servo
** pwm time with a correction factor to return the servo to the set point.
*/
void read_gyro(int adcpin, int calibratepin, struct SERVO *servo, 
               int limit0, int limit1) {
  unsigned long now = millis();
  int adc = analogRead(adcpin) - servo->adc_bias;
  int calibration = analogRead(calibratepin);

  if (adc > 0 && servo->bias_acc++ > 128) {
    servo->adc_bias++;
    servo->bias_acc -= 128;
  } 
  else if (adc < 0 && servo->bias_acc-- < -128) {
    servo->adc_bias--;
    servo->bias_acc += 128; 
  }  
  adc = adc * servo->direction;

  int tdelta = now - servo->last_sample;
  if (tdelta <= 0) {
    servo->last_sample = now;
    return;
  }

  // drift the notional rotation back to center at 10deg per sec
  if (servo->rotation > 0L) {
    if (servo->rotation > 1000L) {
      servo->rotation -= (DRIFT_BACK_TO_CENTER_RATE * servo->expo / tdelta);
    } 
    else {
      servo->rotation = 0;
    }
  } 
  else if (servo->rotation < 0L) {
    if (servo->rotation < -1000L) {
      servo->rotation += (DRIFT_BACK_TO_CENTER_RATE * servo->expo / tdelta);
    } 
    else {
      servo->rotation = 0;
    }
  }

  // update notional rotation
  servo->rotation += ((long)adc * servo->expo) / tdelta;
  servo->last_sample = now;

  int new_pulsewidth = PWM_PREAMBLE_US + calibration + (servo->rotation / 100);
  if (new_pulsewidth > servo->pulsewidth_us) {
    servo->pulsewidth_us += (new_pulsewidth - servo->pulsewidth_us) / 2;
  } else {
    servo->pulsewidth_us -= (servo->pulsewidth_us - new_pulsewidth) / 2;
  }
  
  if (servo->pulsewidth_us < limit0) {
    servo->pulsewidth_us = limit0;
  } else if (servo->pulsewidth_us > limit1) {
    servo->pulsewidth_us = limit1;
  }
}


void delay_us(int us) {
  /*
  ** Delays the requested microseconds.  This is slightly better than arduino's version
  */
  while (us--) {
    __asm__("nop\n\t");
  }
}

void update_servos(struct SERVO *servo1, int servo1_pin, struct SERVO *servo2, int servo2_pin) {
  static unsigned long last_run = micros();
  unsigned long now = micros();

  if (now < last_run + 5000) {
    return;
  } else {
    last_run = now;
  }

  digitalWrite(servo1_pin, true);
  digitalWrite(servo2_pin, true);
  
  // arduino interrupts need to be paused while timing for pulse to avoid jitter
  noInterrupts();   
  if (servo1->pulsewidth_us > servo2->pulsewidth_us) {
    delay_us(servo2->pulsewidth_us);
    digitalWrite(servo2_pin, false);
    delay_us(servo1->pulsewidth_us - servo2->pulsewidth_us);
    digitalWrite(servo1_pin, false);
    interrupts();
    delay_us(3000 - servo1->pulsewidth_us);
  } else {
    delay_us(servo1->pulsewidth_us);
    digitalWrite(servo1_pin, false);
    delay_us(servo2->pulsewidth_us - servo1->pulsewidth_us);
    digitalWrite(servo2_pin, false);
    interrupts();
    delay_us(3000 - servo2->pulsewidth_us);
  }
}

void cycle_range() {
  // A debug method which cycles servo over entire range
  static int pulse = 0;
  static char up = 0;
  if (up) {
    pulse++;
  } 
  else {
    pulse--;
  }
  if (pulse < 10000) {
    pulse = 10000;
    up = 1;
  } 
  else if (pulse > 18000) {
    pulse = 18000;
    up = 0;
  }
  servo_x.pulsewidth_us = servo_y.pulsewidth_us = pulse/10;
}


void loop() {
  read_gyro(GYRO_X, CALIBRATE_X, &servo_x, 1000, 1800);   
  read_gyro(GYRO_Y, CALIBRATE_Y, &servo_y, 1000, 1800);
  update_servos(&servo_x, CHANNEL_X, &servo_y, CHANNEL_Y);

  if (1) {
    Serial.print(servo_x.rotation / 1000, DEC);  
    Serial.print('/');
    Serial.print(servo_x.last_sample, DEC);
    Serial.print("  ");
    Serial.print(servo_y.rotation / 1000, DEC);
    Serial.print('\n'); 
  }
}


