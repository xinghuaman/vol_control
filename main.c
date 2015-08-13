#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#ifndef __AVR_ATmega8__
#define __AVR_ATmega8__ 
#endif

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#define CS PB0
#define UD PB2
#define REL_1 PB6
#define JACK_INT PD2
#define CUR_STAGE_MUTE PD4
#define IR_REMOTE PD3
#define BTN2 PD5
#define BTN1 PD6
#define BTN0 PD7

#define CS_LOW() PORTB &= ~_BV(CS)
#define CS_HIGH() PORTB |= _BV(CS)
#define UD_LOW() PORTB &= ~_BV(UD)
#define UD_HIGH() PORTB |= _BV(UD)
#define REL_1_1() PORTB |= _BV(REL_1)
#define REL_1_0() PORTB &= ~_BV(REL_1)

struct IR_struct{
	uint8_t state;
	uint8_t time;
	uint8_t time_limit;
	uint8_t bit;
	uint8_t ignore;
	uint8_t data[14];
}

struct IR_struct IR = {
	.state = 0,
	.time = 0,
	/*
	 * 1 bit takes 1778us. Prescaler gives 8us per tick.
	 * One bit (1778us) => 222.25 ticks
	 * Half of bit (889us) => 111.125 ticks
	 * Let the limit be between those values: 1.5*889us = 1333.5us. 
	 * 1333.5us/8us = 166.6875 => 167
	 */
	.time_limit = 167,
	.bit = 0,
	.ignore = 0
};
volatile int8_t volume = 0;
volatile uint8_t steps = 0;
volatile int8_t direction = 0;


inline void PWM_clear_on_compare_match()
{
	TCCR1A |= _BV(COM1B1);
	TCCR1A &= ~_BV(COM1B0);
}

inline void PWM_set_on_compare_match()
{
	TCCR1A |= _BV(COM1B1) | _BV(COM1B0);
}

/*
 * OMG, Atmegas do not have "turning off" feature like ARM cortexes have!
 */
void PWM_stupid_turn_off()
{
	TIMSK &= ~_BV(OCIE1B);
	TCCR1A &= ~(_BV(COM1B1) | _BV(COM1B0));
}

void PWM_stupid_turn_on(int8_t direction)
{
	if (direction > 0)
		PWM_clear_on_compare_match();
	else 
		PWM_set_on_compare_match();
	TIMSK |= _BV(OCIE1B);
}

void VOL_increase(uint8_t steps_vol)
{
	direction = 1;
	steps = steps_vol;
	UD_HIGH();
	_delay_ms(1);
	CS_LOW();
	_delay_ms(1);
	PWM_stupid_turn_on(direction);
}

void VOL_decrease(uint8_t steps_vol)
{
	direction = -1;
	steps = steps_vol;
	UD_LOW();
	_delay_ms(1);
	CS_LOW();
	_delay_ms(1);
	PWM_stupid_turn_on(direction);
}

inline void VOL_stop_changing()
{
	PWM_stupid_turn_off();
	CS_HIGH();
}
void PWM_init()
{
	/*MODE 5, look to the datasheet, Waveform generation mode, p. 98 */
	TCCR1A = _BV(WGM10);
	TCCR1B = _BV(WGM12);
	/*End of setting mode*/
	TCCR1B |= _BV(CS11); //clkio/8 from prescaler	
	OCR1B = 128;
	PWM_stupid_turn_off();	
	sei();
}

void pins_init()
{
	DDRB = 0xff;
	PORTB = 0;
	DDRC = 0xff;
	PORTC = 0;
	DDRD = 0xff;
	PORTD = 0;

	DDRD &= ~(_BV(BTN0) | _BV(BTN1) | _BV(BTN2));
	PORTD |= _BV(BTN0) | _BV(BTN1) | _BV(BTN2);
	PORTD |= _BV(CUR_STAGE_MUTE);
	CS_HIGH();
}

void my_stupidity()
{
	UD_HIGH();
	_delay_ms(1);
	CS_LOW();
	_delay_ms(1);
	for(uint8_t it = 0; it < 0x40/2; it++){
		UD_LOW();
		_delay_ms(1);
		UD_HIGH();
		_delay_ms(1);
	}
	CS_HIGH();
}


void my_stupidity_2()
{
	UD_LOW();
	_delay_ms(1);
	CS_LOW();
	_delay_ms(1);
	for(uint8_t it = 0; it < 0x40/2; it++){
		UD_HIGH();
		_delay_ms(1);
		UD_LOW();
		_delay_ms(1);
	}
	CS_HIGH();
}

inline void timer0_reset_val()
{
	TCNT0 = 0;
}

void timer0_on()
{
	/*clkio/64 (from prescaler)*/
	TCCR0 |= _BV(CS01) | _BV(CS00);
	/*Interrupt will present end of IR transmission*/
	TIMSK |= _BV(TOIE0);
	timer0_reset_val();
}

void timer0_off()
{
	/*no clock source*/
	TCCR0 &= ~(_BV(CS02) | _BV(CS01) | _BV(CS00));
	/*Interrupt off*/
	TIMSK &= ~_BV(TOIE0);
}

inline void timer0_get_val(uint8_t *value)
{
	*value = TCNT0;	
}

void IR_init()
{
	DDRD &= ~_BV(IR_REMOTE);
	PORTD &= ~_BV(IR_REMOTE);
	MCUCR |= _BV(ISC11);
	timer0_init();
	GICR |= _BV(INT1);
}

inline void INT_turn_edge()
{
	MCUCR ^= _BV(ISC10);
}

inline void INT_on_falling_edge()
{
	MCUCR &= ~_BV(ISC10);
}

inline void INT_on_rising_edge()
{
	MCUCR |= _BV(ISC10);
}

//TODO: DO IT AS MACRO
inline uint8_t INT_as_rising_edge(){
	if(MCUCR & _BV(ISC10))
		return 1;
	return 0;
}

int main(void)
{
	pins_init();
	PWM_init();
	_delay_ms(500);
	VOL_increase(0x3F);
	while(1){
	}
	return 0;
}

ISR(TIMER1_COMPB_vect)
{
	volume += direction;
	steps--;
	if (!steps)
		VOL_stop_changing();
}

inline uint8_t IR_test_if_shorter_gap(){
	uint8_t rt;
	timer0_get_val(&IR.time);
	if(IR.time <= IR.time_limit)
		rt = 1;
	else
		rt = 0;
	timer0_reset_val();
	return rt;
}

inline void case4_algorithm(){
	if ( ignore ){
		ignore = 0;
		INT_turn_edge();
		break;
	}
	if ( IR.data[IR.bit - 1] == 0 ){
		if ( !INT_as_rising_edge() ){
			if ( IR_test_if_shorter_gap() ){
				IR.data[IR.bit] = 0;
				ignore = 1;
			} else {
				IR.data[IR.bit] = 1;
			}
		}
	} else {
		if ( INT_as_rising_edge() ){
			if( IR_test_if_shorter_gap() ){
				IR.data[IR.bit] = 1;
				ignore = 1;
			} else {
				OR.data[IR.bit] = 0;
			}
		}
	}
	INT_turn_edge();
	IR.bit++;
	if(IR.bit == IR.bit_limit){
		IR.state = 0;
		IR.bit = 0;
		INT_on_falling_edge();	
	}
}

ISR(INT1_vect)
{
	switch(IR.state){
	case 0:
		timer0_on();
		INT_on_rising_edge();
		IR.state = 1;
		IR.data[IR.bit] = 1;
		IR.bit++;
		break;
	case 1:
		if(IR_test_if_shorter_gap()){
			INT_on_falling_edge();
			IR.state = 2;
			IR.data[IR.bit] = 1;
			IR.bit++;
		} else {
			goto err;
		}
		break;
	case 2:
		if(IR_test_if_shorter_gap()){
			INT_on_rising_edge();
			IR.state = 3;
		} else {
			goto err;
		}
		break;
	case 3:
		IR.state = 4;
		INT_turn_edge();
		if ( IR_test_if_shorter_gap() )
			IR.data[IR.bit] = 1;
			ignore = 1;
		else
			IR.data[IR.bit] = 0;
		break;
	case 4: 
		case4_algorithm();
	}
			
err:
	IR.state = 0;
	IR.bit = 0;
	IR.time = 0;
	IR.toggle = 0;
	INT_on_falling_edge();
}

ISR(TIMER0_OVF_vect)
{
	timer0_off();
	IR.state = 0;
}
