#pragma once
#include "Arduino.h"
struct TMC2209Stepper {
 int32_t velocity=0; bool direction=false,ack=true,CRCerror=false; uint8_t count=0; int writes=0;
 TMC2209Stepper(int,int,float,int){}
 void begin(){} void beginSerial(int){} void I_scale_analog(bool){} void toff(int){}
 void rms_current(int){} void microsteps(int){} void en_spreadCycle(bool){} void pwm_autoscale(bool){}
 void VACTUAL(int32_t v){++writes;if(ack){velocity=v;++count;}}
 void shaft(bool v){if(ack){direction=v;++count;}}
 uint8_t IFCNT(){return count;}
 uint32_t GCONF(){return 0;} void GCONF(int){} uint32_t CHOPCONF(){return 0;}
 uint32_t PWMCONF(){return 0;}void PWMCONF(int){}
};
