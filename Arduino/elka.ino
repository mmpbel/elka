/*
  Elka
  New Year tree controller.
 */

const int TICK = 1;

const int KEY_THRESHOLD = 100 / TICK;
const int KEY_DELAY = 5000 / TICK;
 
// Pin 13 has an LED connected on most Arduino boards.
const byte LED = 13;
byte ledSt = 0;
byte enableLeds = 0;

// keys
const byte NUM_KEYS = 6;

const byte KEY_A = 4;
const byte KEY_B = 5;
const byte KEY_C = 6;
const byte KEY_D = 7;
const byte NEXT_KEY = 8;
const byte RESET_KEY = 9;


const byte KEY[NUM_KEYS] = {KEY_A, KEY_B, KEY_C, KEY_D, NEXT_KEY, RESET_KEY};
int keyCounter[NUM_KEYS];

const byte LED_OUT1 = 11;
const byte LED_OUT2 = 12;

// the setup routine runs once when you press reset:
void setup ()
{                
  // initialize the digital pin as an output.
  pinMode(LED, OUTPUT);     
  pinMode(LED_OUT1, OUTPUT);     
  pinMode(LED_OUT2, OUTPUT);     

  for (byte i=0; i<NUM_KEYS; ++i)
  {
    pinMode(KEY[i], INPUT_PULLUP);
    keyCounter[i] = 0;
  }

  Serial.begin(9600);     // opens serial port, sets data rate to 9600 bps
}

byte  cntPWM = 0;
int   cntCircle = 0;

const int PWM_PERIOD = 40*2;
const int LED_PERIOD = PWM_PERIOD * 2;
const int LED_OFFSET = PWM_PERIOD/2;

// the loop routine runs over and over again forever:
void loop ()
{
//  delay(TICK);               // wait for ms

  if (++cntPWM >= PWM_PERIOD)
  {
    cntPWM = 0;

    if (++cntCircle >= LED_PERIOD)
    {
      cntCircle = -LED_PERIOD;
    }
  }

  if (enableLeds)
  {
    if (cntPWM < (abs(cntCircle) - LED_OFFSET))
    {
      digitalWrite(LED_OUT1, HIGH);
      digitalWrite(LED, HIGH);
      digitalWrite(LED_OUT2, LOW);
    }
    else
    {
      digitalWrite(LED_OUT1, LOW);
      digitalWrite(LED, LOW);
      digitalWrite(LED_OUT2, HIGH);
    }
  }
  
  for (byte i=0; i<NUM_KEYS; ++i)
  {
    if (HIGH == digitalRead(KEY[i]))
    {
      if (++keyCounter[i] > KEY_THRESHOLD)
      {
        keyCounter[i] = -KEY_DELAY;
        // turn on a tune
        Serial.print(i);
        Serial.write('\n');
  
        if (RESET_KEY == KEY[i])
        {
          enableLeds = 0;
          digitalWrite(LED_OUT1, LOW);
          digitalWrite(LED, LOW);
          digitalWrite(LED_OUT2, LOW);
        }
        else
        {
          enableLeds = 1;
        }
      }
    }
    else if (keyCounter[i] > 0)
    {
      --keyCounter[i];
    }
    else
    {
      ++keyCounter[i];
    }
  }
}


