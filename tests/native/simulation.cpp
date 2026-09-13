// Runs unchanged production source with deterministic GPIO/clock/UART/EEPROM mocks.
// Each scenario runs in a fresh process: function-local statics are reset by OS.
#include <iostream>
#include <map>
#include "../../lib/buffer/buffer.cpp"
void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
void tick(){++sim_ms;if(sim_ms%100==0){++g_run_cnt;timer_it_callback();}}
void cycle(){read_sensor_state();motor_control();}
void advance(uint32_t ms){while(ms--){tick();cycle();}}
void zone(int z){pins[HALL1]=z==3;pins[HALL2]=z==2;pins[HALL3]=z==0;advance(16);}
void prepare(bool token=true){
 pins[KEY1]=pins[KEY2]=pins[FRONT_SIGNAL_PIN]=pins[BACK_SIGNAL_PIN]=HIGH;
 pins[MDM_DPIN]=HIGH; pins[CHAIN_Y_SENSOR_PIN]=HIGH;
 buffer_sensor_init();buffer_motor_init();has_token=token;
 advance(16);
}
void command(std::string s){Serial.rx=s+"\n";while(Serial.available())USB_Serial_Analys();}
void tap(int key,uint32_t hold=50){input(key,LOW);advance(hold);input(key,HIGH);}
void run_loop(uint32_t ms){
 sim_deadline=sim_ms+ms;on_tick=[](){if(sim_ms%100==0)timer_it_callback();};
 Serial.available_hook=[](){delay(1);};
 try{buffer_loop();}catch(const EndSimulation&){}
 Serial.available_hook=nullptr;on_tick=nullptr;sim_deadline=0;
}
void supply(){pins[CHAIN_Y_SENSOR_PIN]=LOW;advance(16);}
void basic(){prepare();supply();zone(0);check(motor_state==Forward,"Low must feed");zone(3);check(motor_state==Back,"Safety must reverse");zone(2);check(motor_state==Stop,"Safety to High must latch stop");advance(6000);check(motor_state==Stop,"Idle must remain stopped");zone(1);advance(300);check(motor_state==Forward,"Window dwell must resume");}
void low_override(){prepare();supply();zone(3);zone(0);check(motor_state==Forward&&!feed_latched,"Low must override safety latch");}
void high_trim(){prepare();zone(2);advance(1000);check(motor_state==Stop,"Sustained High must stop");}
void debounce(){prepare(false);pins[FRONT_SIGNAL_PIN]=LOW;advance(14);check(!has_token,"Noise shorter than 15ms must not claim token");pins[FRONT_SIGNAL_PIN]=HIGH;advance(20);check(!has_token,"Noise must not latch");}
void hall_priority(){prepare();for(int mask=0;mask<8;++mask){pins[HALL1]=!!(mask&4);pins[HALL2]=!!(mask&2);pins[HALL3]=!!(mask&1);advance(16);auto expected=(mask&4)?AutoZoneSafety:(mask&2)?AutoZoneHigh:(mask&1)?AutoZoneLow:AutoZoneWindow;check(get_auto_zone()==expected,"Wrong hall overlap priority");}}
void timeout_stop(){prepare();advance(60200);check(is_error&&motor_state==Stop,"Continuous forward must trip and stop");check(front_time==0,"Stop must reset forward duration");}
void timeout_boundary(){prepare();front_time=59900;timer_it_callback();check(!is_error,"60s inclusive should not trip current > comparator");timer_it_callback();check(is_error,"60100ms must trip");cycle();check(motor_state==Stop,"Error must stop on next cycle");}
void timer_reset(){prepare();advance(59000);zone(3);zone(0);advance(2000);check(!is_error,"Reverse should reset continuous-forward timer");}
void rollover(){prepare();sim_ms=UINT32_MAX-100;start_chain_pulse(sim_ms);sim_ms+=249;update_chain_output(sim_ms);check(pins[EXTENSION_PIN2]==LOW,"Pulse must remain low at 249ms over wrap");++sim_ms;update_chain_output(sim_ms);check(pins[EXTENSION_PIN2]==HIGH,"Pulse must end at 250ms over wrap");}
void pulse(){prepare();pass_token_to_next(sim_ms);check(!has_token&&pins[EXTENSION_PIN2]==LOW,"Pass must drop token and drive low");advance(249);check(pins[EXTENSION_PIN2]==LOW,"Pulse ended early");advance(1);check(pins[EXTENSION_PIN2]==HIGH,"Pulse must end at 250ms");}
void chain_edge(){prepare(false);pins[FRONT_SIGNAL_PIN]=LOW;advance(20);check(has_token,"Active low edge must claim token");set_token_state(false);advance(500);check(!has_token,"Held level must not reclaim token");pins[FRONT_SIGNAL_PIN]=HIGH;advance(20);pins[FRONT_SIGNAL_PIN]=LOW;advance(20);check(has_token,"Second edge must claim token");}
void y_gate(){prepare(false);pins[CHAIN_Y_SENSOR_PIN]=LOW;advance(20);receive_chain_token(sim_ms);advance(1000);check(motor_state==Stop,"Occupied Y must gate newly acquired token");pins[CHAIN_Y_SENSOR_PIN]=HIGH;advance(16);check(motor_state==Forward,"Y clear should release gate");pins[CHAIN_Y_SENSOR_PIN]=LOW;advance(16);check(motor_state==Forward,"Own filament at Y must not re-gate");}
void empty_receiver(){prepare(false);pins[ENDSTOP_3]=HIGH;advance(20);receive_chain_token(sim_ms);advance(61000);check(chain_pulse_active||last_empty_forward_time>0,"Empty receiver discarded handoff during first 60s; no deferred retry");}
void cooldown_repeat(){prepare(false);pins[ENDSTOP_3]=HIGH;advance(61000);receive_chain_token(sim_ms);check(chain_pulse_active,"First late empty pass must work");advance(300);auto previous=last_empty_forward_time;receive_chain_token(sim_ms);advance(61000);check(last_empty_forward_time!=previous,"Empty receiver drops repeated token within cooldown permanently");}
void restart_occupied_y(){pins[KEY1]=pins[KEY2]=pins[FRONT_SIGNAL_PIN]=pins[BACK_SIGNAL_PIN]=HIGH;pins[MDM_DPIN]=HIGH;pins[CHAIN_Y_SENSOR_PIN]=LOW;uint8_t token=1;EEPROM.put(EEPROM_ADDR_CHAIN_TOKEN,token);buffer_init();advance(120000);check(has_token&&motor_state==Forward,"Stored token restarts behind occupied Y and never regains feed latch");}
void lost_token_reload(){prepare(false);pins[ENDSTOP_3]=HIGH;advance(20);receive_chain_token(sim_ms);pins[ENDSTOP_3]=LOW;advance(120000);check(has_token,"Reload cannot recover token discarded by an empty unit");}
void no_token_stop(){prepare(false);zone(0);check(motor_state==Stop,"Nonholder Low must stop");zone(3);check(motor_state==Back,"Nonholder Safety exception must reverse");}
void runout(){prepare();pins[ENDSTOP_3]=HIGH;advance(16);check(!has_token&&motor_state==Stop&&chain_pulse_active,"Runout must stop and hand off");check(pins[DUANLIAO]==LOW&&pins[START_LED]==LOW,"Runout outputs must be low");}
void duplicate_claim(){prepare(false);tap(KEY2);advance(501);check(!has_token,"KEY2 can mint a second token while another board owns the first");}
void bounce_key(){prepare();tap(KEY2,1);advance(1);tap(KEY2,1);advance(501);check(!is_error,"One bouncing press interpreted as double-click toggles fault");}
void double_toggle(){prepare();tap(KEY2);advance(100);tap(KEY2);advance(501);check(is_error,"Current documented double-click must toggle fault");tap(KEY2);advance(100);tap(KEY2);advance(501);check(!is_error,"Second double-click must clear fault");}
void long_jog_safety(){prepare();input(KEY2,LOW);bool drove_past_safety=false;on_tick=[&](){if(sim_ms%100==0){++g_run_cnt;timer_it_callback();}if(sim_ms==1000)pins[HALL1]=HIGH;if(sim_ms==1500)drove_past_safety=driver.velocity>0&&driver.direction==FORWARD;if(sim_ms==2000)input(KEY2,HIGH);};advance(501);on_tick=nullptr;check(!drove_past_safety,"Forward jog ignores Safety while main loop is blocked");}
void jog_chain_pulse(){prepare();start_chain_pulse(sim_ms);pins[BACK_SIGNAL_PIN]=LOW;on_tick=[](){if(sim_ms%100==0){++g_run_cnt;timer_it_callback();}if(sim_ms==2016)pins[BACK_SIGNAL_PIN]=HIGH;};motor_control();on_tick=nullptr;check(pins[EXTENSION_PIN2]==HIGH,"250ms chain output remains LOW for entire 2s reverse hold");}
void jog_timeout(){prepare(false);input(KEY2,LOW);bool ran_after_timeout=false;on_tick=[&](){if(sim_ms%100==0){++g_run_cnt;timer_it_callback();}if(sim_ms==65000)ran_after_timeout=driver.velocity>0&&!is_error;if(sim_ms==66000)input(KEY2,HIGH);};advance(501);on_tick=nullptr;check(!ran_after_timeout,"Manual forward from Stop bypasses 60s timer entirely");}
void stuck_reverse(){prepare();pins[BACK_SIGNAL_PIN]=LOW;sim_deadline=sim_ms+70000;bool blocked=false;on_tick=[](){if(sim_ms%100==0)timer_it_callback();};try{motor_control();}catch(const EndSimulation&){blocked=true;}on_tick=nullptr;check(!(blocked&&driver.velocity>0&&IWDG->KR==0xAAAA),"Stuck reverse input runs indefinitely while watchdog is fed");}
void uart_failure(){prepare();driver.ack=false;apply_motor_command(Back,VACTRUAL_VALUE);int writes=driver.writes;for(int i=0;i<20;++i)apply_motor_command(Back,VACTRUAL_VALUE);check(is_error||driver.writes>writes,"Failed UART command cached as successful; future retries suppressed");}
void stop_enable(){prepare(false);check(pins[EN_PIN]==HIGH,"Initial Stop cache prevents disabling driver enabled by motor init");}
void front_standalone(){prepare(false);check(motor_state==Forward,"Front must feed without token");pins[ENDSTOP_3]=HIGH;advance(20);check(motor_state==Forward&&!filament_missing(),"Front must ignore its filament switch");zone(0);check(motor_state==Stop,"Empty Y plus Low must stop front");pins[CHAIN_Y_SENSOR_PIN]=LOW;advance(16);check(motor_state==Forward,"Returning filament must resume front");}
void front_lamp(){prepare(false);check(motor_state==Forward,"Front must feed");check(pins[START_LED]==HIGH,"Front is feeding but START_LED is off because unused token=0");}
void timeout_lamp(){prepare();run_loop(60500);check(is_error&&motor_state==Stop,"Timeout setup failed");check(pins[START_LED]==LOW,"Timeout stop still shows START_LED on");}
void error_blink(){prepare();run_loop(1600);std::vector<uint32_t>times;for(auto&e:pin_events)if(e.pin==ERR_LED)times.push_back(e.ms);check(times.size()>=3,"ERR_LED heartbeat absent");for(size_t i=1;i<times.size();++i)check(times[i]-times[i-1]==500,"No-MDM heartbeat period mismatch");}
void mdm_blink(){prepare();connet_mdm_flag=true;run_loop(1000);std::vector<uint32_t>times;for(auto&e:pin_events)if(e.pin==ERR_LED)times.push_back(e.ms);check(times.size()>=5,"MDM double heartbeat absent");check(times[1]-times[0]==100&&times[2]-times[1]==100&&times[3]-times[2]==100,"MDM heartbeat phases mismatch");}
void blockage_output(){prepare();connet_mdm_flag=true;steps=100;blockage_detect.allow_error=1;Blockage_Detect();TIM2->CNT=1000;Blockage_Detect();sim_ms+=101;TIM2->CNT=2000;Blockage_Detect();check(blockage_detect.blockage_flag&&pins[DULIAO]==LOW,"Fixture must produce real blockage alert");motor_control();check(pins[DULIAO]==LOW,"motor_control overwrites jam LOW with filament-present HIGH on shared PB15");}
void blockage_blink(){prepare();connet_mdm_flag=true;blockage_detect.blockage_flag=true;blockage_inform_times=sim_ms;run_loop(450);std::vector<uint32_t>times;for(auto&e:pin_events)if(e.pin==ERR_LED)times.push_back(e.ms);for(size_t i=1;i<times.size();++i)check(times[i]-times[i-1]>=50,"MDM heartbeat falls through between jam toggles and generates 1ms LED glitch");}
void serial_invalid(){prepare();auto old=timeout;command("timeout");check(timeout==old,"Invalid timeout command writes zero despite error");}
void serial_token(){prepare();command("token banana");check(has_token,"Nonnumeric token silently becomes zero, dropping ownership");}
void serial_scale(){prepare();auto old=allow_error_scale;command("scale -1");check(allow_error_scale==old,"Rejected negative scale still persisted");}
void serial_speed(){prepare();auto old=SPEED;command("speed 1001");check(SPEED==old,"Runtime accepts speed above boot-valid maximum 1000");}
void zero_steps(){prepare();uint32_t zero=0;EEPROM.put(EEPROM_ADDR_STEPS,zero);Pulse_Receive_Init();check(steps>0,"EEPROM zero steps accepted; MDM division by zero reachable");}
void front_noise_restart(){prepare(false);zone(0);check(motor_state==Stop,"Runout setup");pins[HALL3]=LOW;advance(16);check(motor_state==Stop,"Front resumes dry when exhausted arm leaves narrow Low hall zone");}
void mdm_inputs(){prepare();connet_mdm_flag=true;pins[ENDSTOP_3]=HIGH;pins[MDM_DPIN]=HIGH;advance(16);check(!filament_missing(),"MDM present must override missing local switch");pins[MDM_DPIN]=LOW;cycle();
#ifndef FRONT_BUFFER_STANDALONE
check(filament_missing(),"Both runout inputs must indicate missing");
#else
check(!filament_missing(),"Front ignores MDM runout too");
#endif
}
void pulse_direction(){prepare();GPIOB->IDR=GPIO_PIN_11;TIM2->CR1=TIM_CR1_DIR;Dir_IT_Callback();check(!(TIM2->CR1&TIM_CR1_DIR),"DIR high must count up");GPIOB->IDR=0;Dir_IT_Callback();check(TIM2->CR1&TIM_CR1_DIR,"DIR low must count down");int32_t old=blockage_detect.mdm_pulse_cnt;Recv_MDM_Pulse_IT_Callback();check(blockage_detect.mdm_pulse_cnt==old+1,"MDM pulse must increment encoder count");}
void pulse_wrap(){prepare();blockage_detect.pulse_cnt=32766;Blockage_Detect();TIM2->CNT=32770;Blockage_Detect();check(blockage_detect.pulse_cnt_sub==4,"Small delta across signed 16bit boundary");blockage_detect.pulse_cnt=-2;TIM2->CNT=2;Blockage_Detect();check(blockage_detect.pulse_cnt_sub==4,"Small delta across hardware wrap");}
// Multi-board transport uses independent OS processes for independent MCU globals.
void board_server(){
 prepare(false);
 int dt,fil,y,halls,chain,token;
 while(std::cin>>dt>>fil>>y>>halls>>chain>>token){
  pins[ENDSTOP_3]=fil;pins[CHAIN_Y_SENSOR_PIN]=y;
  pins[HALL1]=!!(halls&4);pins[HALL2]=!!(halls&2);pins[HALL3]=!!(halls&1);
  pins[FRONT_SIGNAL_PIN]=chain;
  if(token>=0)set_token_state(token!=0);
  advance(dt);
  std::cout<<sim_ms<<" "<<has_token<<" "<<motor_state<<" "<<pins[EXTENSION_PIN2]
           <<" "<<pins[START_LED]<<" "<<is_error<<" "<<pins[DUANLIAO]<<" "<<pins[EN_PIN]<<std::endl;
 }
}
void random_invariants(){
 prepare();uint32_t seed=0xB0FFE123;
 for(int i=0;i<10000;++i){
  seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
  pins[HALL1]=!!(seed&1);pins[HALL2]=!!(seed&2);pins[HALL3]=!!(seed&4);
  pins[ENDSTOP_3]=!!(seed&8);pins[CHAIN_Y_SENSOR_PIN]=!!(seed&16);
  set_token_state(!!(seed&32));is_error=!!(seed&64);advance(20);
  check(speed_trim_percent>=20&&speed_trim_percent<=150,"Trim outside bounds");
  if(is_error)check(motor_state==Stop,"Error must prevent automatic motion");
#ifndef FRONT_BUFFER_STANDALONE
  if(filament_missing())check(motor_state==Stop&&!has_token,"Runout must release token and stop");
  if(!has_token&&get_auto_zone()!=AutoZoneSafety)check(motor_state==Stop,"Nonholder fed automatically");
#else
  if(y_path_clear&&get_auto_zone()==AutoZoneLow)check(motor_state==Stop,"Front empty gate bypassed");
#endif
 }
}
int main(int argc,char**argv){
 if(argc==2&&std::string(argv[1])=="board"){board_server();return 0;}
 std::map<std::string,void(*)()> cases={
 {"mdm_inputs",mdm_inputs},{"pulse_direction",pulse_direction},{"pulse_wrap",pulse_wrap},{"random_invariants",random_invariants},{"basic",basic},{"low_override",low_override},{"high_trim",high_trim},{"debounce",debounce},{"hall_priority",hall_priority},{"timeout_stop",timeout_stop},{"timeout_boundary",timeout_boundary},{"timer_reset",timer_reset},{"rollover",rollover},{"pulse",pulse},{"chain_edge",chain_edge},{"y_gate",y_gate},{"empty_receiver",empty_receiver},{"cooldown_repeat",cooldown_repeat},{"restart_occupied_y",restart_occupied_y},{"lost_token_reload",lost_token_reload},{"no_token_stop",no_token_stop},{"runout",runout},{"duplicate_claim",duplicate_claim},{"bounce_key",bounce_key},{"double_toggle",double_toggle},{"long_jog_safety",long_jog_safety},{"jog_chain_pulse",jog_chain_pulse},{"jog_timeout",jog_timeout},{"stuck_reverse",stuck_reverse},{"uart_failure",uart_failure},{"stop_enable",stop_enable},{"front_standalone",front_standalone},{"front_lamp",front_lamp},{"timeout_lamp",timeout_lamp},{"error_blink",error_blink},{"mdm_blink",mdm_blink},{"blockage_output",blockage_output},{"blockage_blink",blockage_blink},{"serial_invalid",serial_invalid},{"serial_token",serial_token},{"serial_scale",serial_scale},{"serial_speed",serial_speed},{"zero_steps",zero_steps},{"front_noise_restart",front_noise_restart}};
 if(argc!=2||!cases.count(argv[1]))return 2;
 try{cases.at(argv[1])();std::cout<<"PASS "<<argv[1]<<"\n";return 0;}
 catch(const std::exception&e){std::cout<<"FAIL "<<argv[1]<<": "<<e.what()<<" [ms="<<sim_ms<<" motor="<<motor_state<<" token="<<has_token<<" error="<<is_error<<" velocity="<<driver.velocity<<"]\n";return 1;}
}
