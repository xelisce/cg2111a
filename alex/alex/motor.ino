#include <AFMotor.h>

#define FRONT_LEFT   2 // M4 on the driver shield
#define FRONT_RIGHT  1 // M1 on the driver shield
#define BACK_LEFT    3 // M3 on the driver shield
#define BACK_RIGHT   4 // M2 on the driver shield

AF_DCMotor motorFL(FRONT_LEFT);
AF_DCMotor motorFR(FRONT_RIGHT);
AF_DCMotor motorBL(BACK_LEFT);
AF_DCMotor motorBR(BACK_RIGHT);

void setLeftMotors(float speed, bool direction)
{
  int speed_scaled = (speed/100.0) * 255;
  motorFL.setSpeed(speed_scaled);
  motorBL.setSpeed(speed_scaled);
  if (direction) {
    motorFL.run(FORWARD);
    motorBL.run(FORWARD);
  }
  else {
    motorFL.run(BACKWARD);
    motorBL.run(BACKWARD);
  }
}

void setRightMotors(float speed, bool direction)
{
  int speed_scaled = (speed/100.0) * 255;
  motorFR.setSpeed(speed_scaled);
  motorBR.setSpeed(speed_scaled);
  if (direction) {
    motorFR.run(FORWARD);
    motorBR.run(FORWARD);
  }
  else {
    motorFR.run(BACKWARD);
    motorBR.run(BACKWARD);
  }
}

void stopMotors()
{
  motorFL.setSpeed(0);
  motorBL.setSpeed(0);
  motorFR.setSpeed(0);
  motorBR.setSpeed(0);
  motorFL.run(STOP);
  motorBL.run(STOP);
  motorFR.run(STOP);
  motorBR.run(STOP);
}
