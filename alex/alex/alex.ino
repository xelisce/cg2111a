#include <Arduino.h>
#include <serialize.h>
#include <stdarg.h>
#include "packet.h"
#include "constants.h"

#define PIN18 (1<<PD3)
#define PIN19 (1<<PD2)
volatile float dist_travelled = 0;
volatile int heading = 0;
volatile uint16_t Red = 0, Green = 0, Blue = 0;


void setup() {
  //  setupEncoders();
  EICRA = 0b10100000; // falling edge for both pin 18 and 19
  EIMSK = 0b00001100; // enable INT2 and INT3 interrupts for PD2 and PD3
  // enable pullups
  DDRD = 0b11110011;
  PIND = 0b00001100;
  //setupSerial();
  Serial.begin(9600);
  GPIO_init();  // Initialize pins
}

void loop() {
  Serial.print("Distance: ");
  Serial.println(dist_travelled);
  Serial.print("Heading: ");
  Serial.println(heading);
  GetColors();
    // Object detection based on color
    if (Red < Green) {
        Serial.println("Object is Red");
    } else if (Green < Red) {
        Serial.println("Object is Green");
    } else {
        Serial.println("Object is Unknown");
    }

    _delay_ms(2000);
//  setRightDist(20, 70);
//  if (heading >= 5) {
//    stopMotors();
//  }
}
