

/**
 * battery life calculator
 * http://oregonembedded.com/batterycalc.htm
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#define CONFIG_WITH_DEBUG
#define CONFIG_WITH_I2C
#define CONFIG_WITH_ECLIPSE
//#define CONFIG_WITH_LOGGER
//#define CONFIG_WITH_TIMER

// divide clock
#if F_CPU == 16000000L
#	define SET_PRESCALER 1
#elif F_CPU == 8000000L
#	define SET_PRESCALER 0
#endif

// Disabled EEPROM when debugging
// DEBUG CHANNEL ATTINYX5,
// if (F_CPU <= 8000000L) => port B bit 3 (PB3:4) ELSE port B bit 2 (PB2:5)
// DEBUG CHANNEL ATTINYX4,
// if (F_CPU <= 8000000L) => port B bit 0 (PB0:2) ELSE port A bit 0 (PA0:13)
#ifdef DEBUG_SERIAL
#  define CONFIG_WITH_PRINT
#else
#	define CONFIG_WITH_EEPROM
#endif


//
// battery managment
#define V_MIN 2.6
#define V_MAX 4.25
#define V_TOP 4.20

// power
#define P_GND  7
#define P_VCC  8
#define SET_PRESCALER 0
#define TEMPERATURE_CALIBRATE 294

#if defined( __AVR_ATtinyX4__ )


// ATMEL ATTINY84 / ARDUINO
//
//                           +-\/-+
//                     VCC  1|    |14  GND
//  (Serial1)  (D  0)  PB0  2|    |13  AREF (D 10)       (Serial2)
//             (D  1)  PB1  3|    |12  PA1  (D  9) 
//                     PB3  4|    |11  PA2  (D  8) 
//  PWM  INT0  (D  2)  PB2  5|    |10  PA3  (D  7) 
//  PWM        (D  3)  PA7  6|    |9   PA4  (D  6) 
//  PWM        (D  4)  PA6  7|    |8   PA5  (D  5)        PWM (Serial3)
//                           +----+

# define P_DEBUG  10


// Set pin 6's PWM frequency to 62500 Hz (62500/1 = 62500)
// Note that the base frequency for pins 5 and 6 is 62500 Hz  
// *   - Changes on pins 3, 5, 6, or 11 may cause the delay() and
// *     millis() functions to stop working. Other timing-related
#define         P_PWM 5

//
// V_REF as master, I_MAX is normalized 
#define         VCC_REF 4.98
#define         V_REF 2.0
#define         I_MAX 2.0
#define         O_V 5.2
#define         O_I 3.0
#define         PRINT_TICK 200
#define         PRINT 1

#define         A_VFB 1
#define         A_IFB 2


// 0x22 => TEMP sensor, 0x80 1.1V reference
# define A_TEMP 0xA2
//1.1V VREF MUX100001
# define MUX_VREF 0b00100001
//VCC as voltage reference _BT(REFS0)
# define ADC_VREF 0b00000000

#elif defined( __AVR_ATtinyX5__ )

// ATMEL ATTINY45 / ARDUINO
//
//                           +-\/-+
//  Ain0 !RST  (D  5)  PB5  1|    |8   VCC
//  Ain3       (D  3)  PB3  2|    |7   PB2  (D  2)  INT0  Ain1 SCK
//  Ain2       (D  4)  PB4  3|    |6   PB1  (D  1)        pwm1 MISO
//                     GND  4|    |5   PB0  (D  0)        pwm0 MOSI
//                           +----+

# define P_DEBUG  3
# define P_SOUT 1
# define P_SIN  2

// 0x0F => TEMP sensor, 0x80 1.1V reference
# define A_TEMP 0x8F
//1.1V VREF
# define MUX_VREF 0b00001100
//VCC as voltage reference
# define ADC_VREF 0b00000000

#endif


#endif
