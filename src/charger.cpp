#ifdef ARDUINO
# include <Arduino.h>
#else
# include <WProgram.h>
#endif
#include "config.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "energy.h"
#include "charger.h"
#include "pid.h"


//
//
// read analog pin and return avg and digital 
float avg_read(short p, double& digital){
    float ref=(VCC_REF/1023.0);
    digital=analogRead(p);
    for (short i=0;i<1;i++){
      digital=digital*.5+analogRead(p)*.5;
    }
    return digital*ref;
}


//
//
//
//Define the aggressive and conservative Tuning Parameters
// http://fr.wikipedia.org/wiki/M%C3%A9thode_de_Ziegler-Nichols
// Ku = 2.1, Tu = 50Hz, 0.02s, 
// Kp =  Ku/2.2 =.96  , Ki = (1.2 * Kp) / Pu = .023, Kd=0
//check docs/PID.log
double aKp=0.15, aKi=0.001, aKd=0.000;
//double aKp=2.2, aKi=0.08, aKd=0.001;
static PID pid;



int charger_init(CHARGER& charger){
  digitalWrite(P_SW, LOW);
  analogWrite(P_PWM, MIN_PWM);  
  
	charger.fb=charger.vfb=charger.avg_vfb=charger.dfb_v=charger.ifb=charger.dfb_i=0.0;
  
  //
  // setpoint value
  charger.sp=A2D(V_BATT); 
  //
  // input value (charger.dfb_v is the output)
  charger.pwm=charger.open_pwm=MIN_PWM;
  
  pid_init(pid, MIN_PWM,  MAX_PWM, aKp, aKi, aKd);  
  
}

void charger_reset(CHARGER& charger, int pwm){
  charger.pwm=(double)pwm;
  analogWrite(P_PWM,pwm); 
  delay(TIMER_TICK);
  digitalWrite(P_SW, LOW);
  pid_reset(pid);
}

void charger_check_mosfet(CHARGER& charger){
    //
    // security trigger on over voltage
    if (charger.vfb>0.05 && charger.vfb<=(V_IN) && charger.pwm>(OPEN_PWM)){
#ifdef CONFIG_WITH_PRINT      
      Serial.print("BUG, UNPLUG THE POWERLINE!!!! : ");
      Serial.print(charger.vfb*V_FACTOR,2);
      Serial.print(" I: ");Serial.print(charger.ifb*I_FACTOR,2);
      Serial.print(" PWM: ");
      Serial.println((int)charger.pwm);
#endif      

      charger_reset(charger,0);
      while(true);
    }
}

//
// checking when output is switched off
// detect internal issue like undervoltage, over current, over temperature 
// or unplugged input (for debug only)
int charger_openvoltage(CHARGER& charger){

  int openvoltage=true, count=0;
  
#ifdef CONFIG_WITH_PRINT      
  Serial.println("START CALIBRATE: ");
#endif      
 
  digitalWrite(P_SW, LOW);
  charger.temp=avr_internal_temp();
#ifdef CONFIG_WITH_PRINT      
    Serial.print(" TEMP: ");Serial.println(charger.temp,2);
#endif      

  //
  // detect over temperature
  if(charger.temp>TEMP_MAX){
#ifdef CONFIG_WITH_PRINT      
    Serial.print("OVER TEMP: ");Serial.println(charger.temp,2);
#endif      
    charger_reset(charger,MIN_PWM);
    delay(TIMER_WAIT*5);
    return false;      
  }


	while(openvoltage){
	  charger.pwm=OPEN_PWM;
    analogWrite(P_PWM, OPEN_PWM); 
    charger.vfb=avg_read(A_VFB,charger.dfb_v);
    charger.ifb=avg_read(A_IFB,charger.dfb_i);

    //
    // check mosfet
    charger_check_mosfet(charger);

    //
    // check off if vout <= vin 
    //    => input is unplugged?
    if ((charger.vfb)<=V_IN){
#ifdef CONFIG_WITH_PRINT      
      Serial.print("OFF : ");
      Serial.print(charger.vfb*V_FACTOR,2);
      Serial.print(" I: ");
      Serial.println(charger.ifb*I_FACTOR,2);
#endif      
      // re init pwm
      count=0;
      analogWrite(P_PWM, 0); 
      delay(TIMER_WAIT);
      continue;
    }
    
    //
    // check off if vout <= vin 
    //    => input is short circuit?
    if ((charger.ifb)>I_BATT_CHARGED){
#ifdef CONFIG_WITH_PRINT      
      Serial.print("CURRENT ISSUE I: ");
      Serial.print(charger.ifb*I_FACTOR,2);
      Serial.print(" V: ");
      Serial.println(charger.vfb*V_FACTOR,2);
#endif      
      // re init pwm
      analogWrite(P_PWM, 0); 
      delay(TIMER_WAIT);
      return false;
    }
    

    if ((charger.vfb)>V_IN*1.2 && count++>100){
      openvoltage=false;count=0;
    }
    
  }
  
  charger.open_pwm=OPEN_PWM;
  return true;
}


// Checking battery
// ================
// 1)The charger check is a battery is plugged, we assume that battery capacity 
//   and voltage are hardcoded ( SmartBattery feature is not yet implemented)
// 2)The charger then checks for over/under battery temperature and over voltage 
//   and flags an error if any of these are detected, 
//   -> sending a short error message to the PC.   
//
// ----
// http://batteryuniversity.com/learn/article/charging_lithium_ion_batteries
// Do not recharge lithium-ion if a cell has stayed at or below 1.5V for more 
// than a week. Copper shunts may have formed inside the cells that can lead 
// to a partial or total electrical short. If recharged, the cells might become 
// unstable, causing excessive heat or showing other anomalies. Li-ion packs 
// that have been under stress are more sensitive to mechanical abuse, such 
// as vibration, dropping and exposure to heat.

int charger_checking(CHARGER& charger){

  int checking=true;
  while(checking){
    //
    // switch on charger in open >VIN*1.2 
    analogWrite(P_PWM, OPEN_PWM);  
    digitalWrite(P_SW, HIGH);
    
    //
    //
    // checking for HIGH CURRENT (short circuit)
    charger.ifb=avg_read(A_IFB,charger.dfb_i);
    if (charger.ifb>O_I){
      //current is to high, switchoff charger
#ifdef CONFIG_WITH_PRINT      
      Serial.print("OVER I: ");Serial.println(charger.ifb*I_FACTOR,2);
#endif      

      charger_reset(charger,MIN_PWM);
      delay(TIMER_WAIT);
      continue;
    }

    //
    //
    // checking under/over voltage
    charger.vfb=avg_read(A_VFB,charger.dfb_v);
    if (charger.vfb>(V_BATT) || charger.vfb <U_V){
      // on volt to high switchoff charger
#ifdef CONFIG_WITH_PRINT      
      Serial.print("UNDER/OVER V: ");Serial.println(charger.vfb*V_FACTOR,2);
#endif      
      charger_reset(charger,MIN_PWM);
      delay(TIMER_WAIT);
      continue;
    }

    //
    //    
    // checking unpluged (open voltage is > input voltage) AND current is ~Null
    if (charger.vfb>(V_IN) && charger.ifb<I_BATT_CHARGED && false){
#ifdef CONFIG_WITH_PRINT      
      Serial.print("STANDBY ");
      Serial.print(" I: ");Serial.print(charger.ifb*I_FACTOR,2);
      Serial.print(" V: ");Serial.println(charger.vfb*V_FACTOR,2);
#endif      
      delay(TIMER_WAIT);
      continue;
    }
    charger.avg_vfb=charger.vfb;
    charger.pwm=OPEN_PWM;
    checking=false;
  }     
  return true;
}

//
// main loop, charging
// ===================
// During all loop (fast & constant charge) the charger check 
// the temperature and if battery is hot unplugged
//
// 2)If OK control proceeds to the Constant Current loop where the PWM signal 
//   on-time is adjusted up or down until the measured current matches the 
//   specified fast charge current. 
//   -> The charge LED flashes quickly during this mode.
//
// 3)When the Maximum Charge Voltage setting is reached then the 
//   mode switches to Constant Voltage and a new loop is entered that uses 
//   the same method as above to control voltage at the Maximum Charge 
//   Voltage point. 
//   -> The LED now flashes more slowly. 
//
//   This continues until 30 minutes after the charge current, which is 
//   decreasing with time in constant voltage mode, reaches the minimum 
//   (set at 50 mA per 1600 mAH of capacity). 
//   ->The charger then shuts off and the LED goes on steady.
// 
//
int charger_mainloop(CHARGER& charger){
  long charging=1, count=0, constant_voltage=false;
  double p_norm=P_NORM, i_norm=I_NORM;
  
  //
  // one loop is about (on arduino 16Mhz)
  // - 920us, 1087Hz (1x avg_read(3 iter) without Serial.print)
  // - 1.8ms, 555Hz (2x avg_read without Serial.print=
  // - 3.3ms, 306Hz (only with Serial.print)
  while(charging){
    digitalWrite(P_SW,HIGH);  

    // only peak temp every (1.8 - 5.0 ms *1000 => ~2-5 seconds )
    if((charging++)%400==0){
  	  //charger.temp=avr_internal_temp();
  	  charging=1;
      if (constant_voltage)Serial.print("CST ");
      Serial.print("INFO V: ");Serial.print(charger.vfb*V_FACTOR,2);
      Serial.print(" PWM: ");Serial.print((int)(charger.pwm));
      //Serial.print(" SP: ");Serial.print(A2D(V_BATT));
      Serial.print(" TEMP: ");Serial.print(charger.temp);
      Serial.print(" FB: ");Serial.print((int)(charger.fb));
      Serial.print(" ERROR: ");Serial.print((int)(pid.e));
      Serial.print("      P: ");Serial.println(charger.ifb*I_FACTOR*charger.vfb*V_FACTOR,2);

  	}
    
    
    //
    // read voltage feedback
    charger.vfb=avg_read(A_VFB,charger.dfb_v);
    charger.avg_vfb=charger.avg_vfb*.9+charger.vfb*.1;
    charger.ifb=avg_read(A_IFB,charger.dfb_i);

#if 1


    //
    // detect over temperature
    if(charger.temp>TEMP_MAX){
#ifdef CONFIG_WITH_PRINT      
      Serial.print("OVER TEMP: ");Serial.println(charger.temp,2);
#endif      
      charger_reset(charger,MIN_PWM);
      while(true);
      continue;      
    }


    //
    // check mosfet
    charger_check_mosfet(charger);


#ifdef CONFIG_WITH_PRINT      
    //
    // detect offline (FOR DEBUG ONLY)
    if ((charger.vfb)<=V_IN && (charger.ifb<I_BATT_CHARGED) && count++>300){
      Serial.print("OFF : ");
      Serial.print(charger.vfb*V_FACTOR,2);
      Serial.print(" PWM: ");
      Serial.println((int)charger.pwm);
      // re init pwm
      charger_reset(charger,MIN_PWM);
      delay(TIMER_WAIT);
      return false;
    }    
#endif      

    
    //
    // security trigger on over voltage
    if (charger.vfb>O_V){
#ifdef CONFIG_WITH_PRINT      
      Serial.print("OVER V: ");Serial.print(charger.vfb*V_FACTOR,2);
      Serial.print(" PWM: ");Serial.print((int)(charger.pwm));
      Serial.print(" I: ");Serial.println(charger.ifb*I_FACTOR,2);
#endif      

      charger_reset(charger,MIN_PWM);
      delay(TIMER_WAIT*3);
      continue;      
    }

    
    //
    // security trigger on over current
    if ((charger.ifb*charger.vfb)>(P_MAX*1.1)){
#ifdef CONFIG_WITH_PRINT      
      Serial.print("OVER P: ");Serial.print(charger.ifb*I_FACTOR*charger.vfb*V_FACTOR,2);
      Serial.print(" PWM: ");Serial.print((int)(charger.pwm));
      Serial.print(" V: ");Serial.println(charger.vfb*V_FACTOR,2);
#endif      
      charger_reset(charger,MIN_PWM);
      delay(TIMER_WAIT*3);
      continue;      
    }
    
    //
    // check if cherging is done
    if (abs(charger.vfb-V_BATT)<=0.1 && charger.ifb<=I_BATT_CHARGED && charger.ifb>0.01){
#ifdef CONFIG_WITH_PRINT      
      Serial.print("CHARGED: V=");
      Serial.print(charger.vfb*V_FACTOR,2);
      Serial.print(" I=");Serial.println(charger.ifb*I_FACTOR,2);
#endif      
      charger_reset(charger,MIN_PWM);
      while(true);
    }
    
#endif    
    
    
    //
    // limit on voltage for constant voltage
    if ((V_BATT-charger.avg_vfb)<=0.01 || constant_voltage){
      constant_voltage=true;
      charger.pwm=pid_update(pid,charger.sp,charger.dfb_v);
    }else{
      //
      // limit on power for fast charge
      charger.fb=A2D(charger.ifb*charger.vfb);//charger.dfb_v;
      charger.pwm=pid_update(pid,A2D(P_MAX),charger.fb);
    }
    analogWrite(P_PWM, (int)(charger.pwm) ); 
  }
  charger_reset(charger,MIN_PWM);
  return true;
}

