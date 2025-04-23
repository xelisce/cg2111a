//#include <Arduino.h>
//#include <serialize.h>
//#include <stdarg.h>
//#include "packet.h"
//#include "constants.h"
//
#define PIN18 (1<<PD3)
#define PIN19 (1<<PD2)
volatile float dist_travelled = 0;
volatile int heading = 0;
//volatile uint16_t Red = 0, Green = 0, Blue = 0;
//
//
//void setup() {
//  //  setupEncoders();
//  EICRA = 0b10100000; // falling edge for both pin 18 and 19
//  EIMSK = 0b00001100; // enable INT2 and INT3 interrupts for PD2 and PD3
//  // enable pullups
//  DDRD = 0b11110011;
//  PIND = 0b00001100;
//  //setupSerial();
//  Serial.begin(9600);
//  GPIO_init();  // Initialize pins
//}
//
//void loop() {
//  Serial.print("Distance: ");
//  Serial.println(dist_travelled);
//  Serial.print("Heading: ");
//  Serial.println(heading);
//  GetColors();
//    // Object detection based on color
//    if (Red < Green) {
//        Serial.println("Object is Red");
//    } else if (Green < Red) {
//        Serial.println("Object is Green");
//    } else {
//        Serial.println("Object is Unknown");
//    }
//
//    _delay_ms(2000);
////  setRightDist(20, 70);
////  if (heading >= 5) {
////    stopMotors();
////  }
//}

#include <serialize.h>

#include "packet.h"
#include "constants.h"

/*
 * Alex's configuration constants
 */

// Number of ticks per revolution from the 
// wheel encoder.
//
//#define COUNTS_PER_REV      1
//
//// Wheel circumference in cm.
//// We will use this to calculate forward/backward distance traveled 
//// by taking revs * WHEEL_CIRC
//
//#define WHEEL_CIRC          1
//
///*
// *    Alex's State Variables
// */
//
//// Store the ticks from Alex's left and
//// right encoders.
//volatile unsigned long leftTicks; 
//volatile unsigned long rightTicks;
//
//// Store the revolutions on Alex's left
//// and right wheels
//volatile unsigned long leftRevs;
//volatile unsigned long rightRevs;
//
//// Forward and backward distance traveled
//volatile unsigned long forwardDist;
//volatile unsigned long reverseDist;


/*
 * 
 * Alex Communication Routines.
 * 
 */
 


/*
 * Setup and start codes for external interrupts and 
 * pullup resistors.
 * 
 */
// Enable pull up resistors on pins 18 and 19
void enablePullups()
{
  // Use bare-metal to enable the pull-up resistors on pins
  // 19 and 18. These are pins PD2 and PD3 respectively.
  // We set bits 2 and 3 in DDRD to 0 to make them inputs. 
  
}

// Functions to be called by INT2 and INT3 ISRs.
//void leftISR()
//{
//  if (dir ==FORWARD){
//    
//  }
//}
//
//void rightISR()
//{
//  rightTicks++;
//  Serial.print("RIGHT: ");
//  Serial.println(rightTicks);
//}

// Set up the external interrupt pins INT2 and INT3
// for falling edge triggered. Use bare-metal.
void setupEINT()
{
  // Use bare-metal to configure pins 18 and 19 to be
  // falling edge triggered. Remember to enable
  // the INT2 and INT3 interrupts.
  // Hint: Check pages 110 and 111 in the ATmega2560 Datasheet.

}





void setup() {
  // put your setup code here, to run once:

  cli();
//  setupEINT();
  setupSerial();
  startSerial();
//  enablePullups();
  initializeState();
  sei();
}



void loop() {
// Uncomment the code below for Step 2 of Activity 3 in Week 8 Studio 2

 //forward(0, 100);

// Uncomment the code below for Week 9 Studio 2

 // put your main code here, to run repeatedly:
  TPacket recvPacket; // This holds commands from the Pi

  TResult result = readPacket(&recvPacket);
  
  if(result == PACKET_OK)
    handlePacket(&recvPacket);
  else
    if(result == PACKET_BAD)
    {
      sendBadPacket();
    }
    else
      if(result == PACKET_CHECKSUM_BAD)
      {
        sendBadChecksum();
      } 

}
