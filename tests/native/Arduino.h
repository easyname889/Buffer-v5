#pragma once
// Host-only deterministic hardware seam. Never links to physical devices.
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <string>
#include <sstream>
#include <functional>
#include <vector>
#include <stdexcept>
#include <algorithm>
using std::isnan;
enum { PA2=2,PA3,PA4,PA5,PA6,PA7,PA8,PA15=15,PB1=17,PB2,PB3,PB4,PB5,PB6,PB7,PB10=26,PB11,PB12,PB13,PB14,PB15,PC13=45 };
enum { LOW=0,HIGH=1,INPUT=0,OUTPUT=1,INPUT_PULLUP=2,INPUT_PULLDOWN=3,CHANGE=4,RISING=5,HEX=16 };
inline uint32_t sim_ms=0;
inline int pins[64]={}, modes[64]={};
inline std::function<void()> callbacks[64];
inline std::function<void()> on_tick;
inline uint32_t sim_deadline=0;
struct EndSimulation : std::exception {};
struct PinEvent { uint32_t ms; int pin,value; };
inline std::vector<PinEvent> pin_events;
inline uint32_t millis(){return sim_ms;}
inline void delay(uint32_t n){while(n--){++sim_ms;if(on_tick)on_tick();if(sim_deadline && sim_ms>=sim_deadline)throw EndSimulation();}}
inline void pinMode(int p,int m){modes[p]=m;}
inline int digitalRead(int p){return pins[p];}
inline void digitalWrite(int p,int v){if(pins[p]!=v)pin_events.push_back({sim_ms,p,v});pins[p]=v;}
inline void digitalToggle(int p){digitalWrite(p,!pins[p]);}
inline void attachInterrupt(int p,void(*f)(),int){callbacks[p]=f;}
inline void input(int p,int v){bool change=pins[p]!=v;pins[p]=v;if(change&&callbacks[p])callbacks[p]();}
class String {
 std::string s;
public:
 String()=default; String(const char*p):s(p){} String(std::string p):s(p){}
 template<class T> String(T v){std::ostringstream o;o<<+v;s=o.str();}
 const char*c_str()const{return s.c_str();}
 int indexOf(const char*p)const{auto i=s.find(p);return i==std::string::npos?-1:int(i);}
 String substring(unsigned i)const{return i>s.size()?String(""):String(s.substr(i));}
 long toInt()const{return static_cast<int32_t>(std::strtol(s.c_str(),nullptr,10));}
 String&operator+=(char c){s+=c;return *this;}
 friend String operator+(const char*a,const String&b){return String(std::string(a)+b.s);}
 friend std::ostream&operator<<(std::ostream&o,const String&v){return o<<v.s;}
};
struct SerialMock {
 std::string rx,out;
 // Hook provides a deterministic one-millisecond loop clock at Serial.available().
 std::function<void()> available_hook;
 int available(){if(available_hook)available_hook();return !rx.empty();}
 char read(){char c=rx.front();rx.erase(0,1);return c;}
 void begin(int){} void dtr(bool){}
 template<class T>void print(T v){std::ostringstream o;o<<v;out+=o.str();}
 template<class T>void println(T v){print(v);out+='\n';}
 template<class T>void println(T v,int){println(v);}
} inline Serial;
struct TimerRegs {uint32_t CNT=0,CR1=0;};
inline TimerRegs tim2,tim6;
#define TIM2 (&tim2)
#define TIM6 (&tim6)
struct HardwareTimer {HardwareTimer(TimerRegs*){} void pause(){} void setPrescaleFactor(int){} void setOverflow(int){} void attachInterrupt(void(*)()){} void resume(){}};
struct GPIORegs {uint32_t IDR=0;};
inline GPIORegs ga,gb;
#define GPIOA (&ga)
#define GPIOB (&gb)
struct TimerInit {int Prescaler,CounterMode,Period,ClockDivision,AutoReloadPreload;};
struct TIM_HandleTypeDef {TimerRegs*Instance;TimerInit Init;};
struct GPIO_InitTypeDef {int Pin,Mode,Pull,Alternate;};
struct TIM_SlaveConfigTypeDef {int SlaveMode,InputTrigger,TriggerPolarity,TriggerFilter;};
struct TIM_MasterConfigTypeDef {int MasterOutputTrigger,MasterSlaveMode;};
enum {GPIO_PIN_11=2048,GPIO_PIN_5=32,TIM_CR1_DIR=16,GPIO_MODE_AF_PP=0,GPIO_NOPULL=0,GPIO_AF2_TIM2=0,TIM_COUNTERMODE_UP=0,TIM_CLOCKDIVISION_DIV1=0,TIM_AUTORELOAD_PRELOAD_DISABLE=0,HAL_OK=0,TIM_SLAVEMODE_EXTERNAL1=0,TIM_TS_TI1FP1=0,TIM_TRIGGERPOLARITY_RISING=0,TIM_TRGO_RESET=0,TIM_MASTERSLAVEMODE_DISABLE=0,TIM6_DAC_IRQn=0,EXTI4_15_IRQn=0};
inline void __HAL_RCC_GPIOB_CLK_ENABLE(){} inline void __HAL_RCC_GPIOA_CLK_ENABLE(){} inline void __HAL_RCC_TIM2_CLK_ENABLE(){}
inline void NVIC_SetPriority(int,int){}
inline void HAL_GPIO_Init(GPIORegs*,GPIO_InitTypeDef*){} inline void HAL_GPIO_DeInit(GPIORegs*,int){}
inline int HAL_TIM_Base_Init(TIM_HandleTypeDef*){return HAL_OK;}
inline int HAL_TIM_SlaveConfigSynchro(TIM_HandleTypeDef*,TIM_SlaveConfigTypeDef*){return HAL_OK;}
inline int HAL_TIMEx_MasterConfigSynchronization(TIM_HandleTypeDef*,TIM_MasterConfigTypeDef*){return HAL_OK;}
inline void HAL_TIM_Base_Start(TIM_HandleTypeDef*){} inline void HAL_TIM_Base_Stop(TIM_HandleTypeDef*){}
inline void Error_Handler(){throw std::runtime_error("HAL error");}
