#define WHEEL_CIRC 19.48
#define COUNTS_PER_REV 4
#define ONE_TICK_DIST 4.87 // 19.48 divided by 4
#define ONE_TICK_HEADING 36 // 4.87 x 2 / 15.5, where 15.5 is axel to axel length. then convert the radians to degrees.
#define PIN18 (1<<PD3)
#define PIN19 (1<<PD2)

float left_tick_dist = 1;
float right_tick_dist = 1;
int right_tick_heading = 1;

typedef enum 
{
  STOP = 0,
  FORWARD = 1,
  BACKWARD = 2,
  LEFT = 3,
  RIGHT = 4
} RobotDirection;

void setupEncoders() 
{
  // setting up interrupts
  EICRA = 0b10100000; // falling edge for both pin 18 and 19
  EIMSK = 0b00001100; // enable INT2 and INT3 interrupts for PD2 and PD3

  // enable pullups
  DDRD = ~(1 << (PIN18 & PIN19));
  PORTD = 1 << (PIN18 & PIN19);
}

ISR(INT3_vect)
{
  rightISR();
}

ISR(INT2_vect)
{
  leftISR();
}

void leftISR() //left
{
  dist_travelled += left_tick_dist;
}

void rightISR()
{
  dist_travelled += right_tick_dist;
  heading += right_tick_heading;
}

float getDist()
{
  return dist_travelled;
}

float getHeading()
{
  heading = heading % 360;
  return heading;
}

void setForwardDist(float dist, float speed)
{
//  left_tick_dist = ONE_TICK_DIST/2;
//  right_tick_dist = ONE_TICK_DIST/2;
//  heading = 0;
  setLeftMotors(speed, true);
  setRightMotors(speed, true);
}

void setBackwardDist(float dist, float speed)
{
//  left_tick_dist = -ONE_TICK_DIST/2;
//  right_tick_dist = -ONE_TICK_DIST/2;
//  heading = 0;
  setLeftMotors(speed, false);
  setRightMotors(speed, false);
}

void setLeftDist(float dist, float speed)
{
//  left_tick_dist = 0;
//  right_tick_dist = 0;
//  right_tick_heading = -ONE_TICK_HEADING;
  setLeftMotors(speed, true);
  setRightMotors(speed, false);
}

void setRightDist(float dist, float speed)
{
//  left_tick_dist = 0;
//  right_tick_dist = 0;
//  heading = ONE_TICK_HEADING;
  setLeftMotors(speed, false);
  setRightMotors(speed, true);
}

void debugMotors() {
  setForwardDist(10, 50);
  while (dist_travelled <= 30);
  Serial.print("Distance: ");
  Serial.println(dist_travelled);
  Serial.print("Heading: ");
  Serial.println(heading);
  setLeftDist(90, 70);
  while (heading >= 500);
  Serial.print("Distance: ");
  Serial.println(dist_travelled);
  Serial.print("Heading: ");
  Serial.println(heading);
  setRightDist(90, 70);
  while (heading <= 0);
  Serial.print("Distance: ");
  Serial.println(dist_travelled);
  Serial.print("Heading: ");
  Serial.println(heading);
  setBackwardDist(10, 50);
  while (dist_travelled >= 0);
  Serial.print("Distance: ");
  Serial.println(dist_travelled);
  Serial.print("Heading: ");
  Serial.println(heading);
  stopMotors();
  delay(2000);
}
