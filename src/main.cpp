/*
 * Check:  http://www.electronoobs.com/eng_robotica_tut5_2_1.php
 *
 *
A basic receiver test for the nRF24L01 module to receive 6 channels send a ppm
sum with all of them on digital pin D2. Install NRF24 library before you compile
Please, like, share and subscribe on my https://www.youtube.com/c/ELECTRONOOBS
 */
#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>
#include <nRF24L01.h>

////////////////////// PPM CONFIGURATION//////////////////////////
#define channel_number 6 // set the number of channels
#define sigPin 2         // set PPM signal output pin on the arduino
#define PPM_FrLen 27000
#define PPM_PulseLen 400 // set the pulse length
//////////////////////////////////////////////////////////////////

int ppm[channel_number];

const uint64_t pipeIn = 0xF0F1F2F3F4LL;

RF24 radio(10, 9);

// The sizeof this struct should not exceed 32 bytes
struct MyData {
  byte throttle;
  byte yaw;
  byte pitch;
  byte roll;
  byte AUX1;
  byte AUX2;
};

MyData data;

void setPPMValuesFromData() {
  ppm[0] = map(data.throttle, 0, 255, 1000, 2000);
  ppm[1] = map(data.yaw, 0, 255, 1000, 2000);
  ppm[2] = map(data.pitch, 0, 255, 1000, 2000);
  ppm[3] = map(data.roll, 0, 255, 1000, 2000);
  ppm[4] = map(data.AUX1, 0, 1, 1000, 2000);
  ppm[5] = map(data.AUX2, 0, 1, 1000, 2000);

  Serial.print(ppm[0]);
  Serial.print("---");
  Serial.print(ppm[1]);
  Serial.print("----");
  Serial.print(ppm[2]);
  Serial.print("---");
  Serial.print(ppm[3]);
  Serial.print("----");
  Serial.print(ppm[4]);
  Serial.print("----");
  Serial.println(ppm[5]);
}

void resetData() {
  // 'safe' values to use when no radio input is detected
  data.throttle = 0;
  data.yaw = 127;
  data.pitch = 127;
  data.roll = 127;
  data.AUX1 = 0;
  data.AUX2 = 0;

  setPPMValuesFromData();
}

/**************************************************/

void setupPPM() {
  pinMode(sigPin, OUTPUT);
  digitalWrite(sigPin, 0); // set the PPM signal pin to the default state (off)

  cli();
  TCCR1A = 0; // set entire TCCR1 register to 0
  TCCR1B = 0;

  OCR1A = 100; // compare match register (not very important, sets the timeout
               // for the first interrupt)
  TCCR1B |= (1 << WGM12);  // turn on CTC mode
  TCCR1B |= (1 << CS11);   // 8 prescaler: 0,5 microseconds at 16mhz
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
  sei();
}

void setup() {
  Serial.begin(9600);
  resetData();
  setupPPM();

  radio.begin();
  radio.setAutoAck(false);
  radio.setChannel(95);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_HIGH);

  radio.openReadingPipe(1, pipeIn);
  radio.startListening();
}

/**************************************************/

unsigned long lastRecvTime = 0;

void recvData() {
  while (radio.available()) {
    radio.read(&data, sizeof(MyData));
    lastRecvTime = millis();
  }
}

/**************************************************/

void loop() {
  recvData();

  unsigned long now = millis();
  if (now - lastRecvTime > 1000) {
    // signal lost?
    Serial.print("===========================");
    Serial.println(ppm[0]);
    resetData();
  }

  setPPMValuesFromData();
}

/**************************************************/

#define clockMultiplier 2

ISR(TIMER1_COMPA_vect) {
  static boolean state = true;

  TCNT1 = 0;

  if (state) {
    // end pulse
    PORTD =
        PORTD &
        ~B00000100; // turn pin 2 off. Could also use: digitalWrite(sigPin,0)
    OCR1A = PPM_PulseLen * clockMultiplier;
    state = false;
  } else {
    // start pulse
    static byte cur_chan_numb;
    static unsigned int calc_rest;

    PORTD = PORTD | B00000100; // turn pin 2 on. Could also use:
                               // digitalWrite(sigPin, 1)
    state = true;

    if (cur_chan_numb >= channel_number) {
      cur_chan_numb = 0;
      calc_rest += PPM_PulseLen;
      OCR1A = (PPM_FrLen - calc_rest) * clockMultiplier;
      calc_rest = 0;
    } else {
      OCR1A = (ppm[cur_chan_numb] - PPM_PulseLen) * clockMultiplier;
      calc_rest += ppm[cur_chan_numb];
      cur_chan_numb++;
    }
  }
}
