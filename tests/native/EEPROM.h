#pragma once
#include "Arduino.h"
struct EEPROMMock {
 uint8_t data[128];int writes=0;
 EEPROMMock(){memset(data,255,sizeof(data));}
 template<class T>void get(int a,T&v){memcpy(&v,data+a,sizeof(v));}
 template<class T>void put(int a,const T&v){memcpy(data+a,&v,sizeof(v));++writes;}
} inline EEPROM;
