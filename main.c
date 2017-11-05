#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/twi.h>
#include "main.h"
#include "isp.h"

#define TWI_ACK   TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN) | (1<<TWIE)
#define TWI_NAK   TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWIE)
#define TWI_RESET TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWSTO) | (1<<TWEN) | (1<<TWIE);

static volatile uint8_t bExecute;

#define SIZE_MEMAREA (8+SPM_PAGESIZE)
volatile uint8_t memarea[SIZE_MEMAREA];
static volatile unsigned short count;
extern void boot (void) __asm__("0") __attribute__((__noreturn__));

#if defined (__AVR_ATmega328P__)
void get_mcusr(void) __attribute__((naked)) __attribute__((section(".init3")));

// On new Devices the watchdog stays active after reset
// so it must be disabled early
void get_mcusr(void)
{
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
}
#endif


ISR(TWI_vect)
{
	uint8_t tmp;

	switch(TW_STATUS)
	{
		case TW_SR_SLA_ACK: // start write
			count = 0;
			TWI_ACK;
			break;
		case TW_SR_DATA_ACK: // write
			tmp = TWDR;
			if (count == 0 && tmp == 0xff) {
				// execute command
				bExecute = 1;
			} else {
				if (count >= SIZE_MEMAREA) {
					TWI_NAK;
					break;
				}
				memarea[count++] = tmp;
			}
			TWI_ACK;
			break;
		case TW_ST_SLA_ACK: // start read
			count = 0;
		case TW_ST_DATA_ACK: // read
			if (count >= SIZE_MEMAREA) {
				TWI_NAK;
				break;
			}
			TWDR = memarea[count++];
			TWI_ACK;
			break;
		case TW_SR_STOP:
			TWI_ACK;
			break;
		default:
			TWI_RESET;
	}
}


int main(void)
{
	unsigned char temp;
	uint8_t bForceRunning = 0;

	cli();

	if (eeprom_read_byte((uint8_t*)0) == 123) {
		// bootloader force running
		eeprom_write_byte((uint8_t*)0, 0);
		bForceRunning = 1;
	}

	// check for valid program
	if (pgm_read_byte(0x0000) != 0xff && bForceRunning == 0) {
		boot();
	}

	boot_addr = -1;
	memarea[0] = 0;
	bExecute = 0;
	count = 0;

	// Move Interrupt Vector	
#ifdef GICR
	temp = GICR;
	GICR = temp | (1<<IVCE);
	GICR = temp | (1<<IVSEL);
#else
	temp = MCUCR;
	MCUCR = temp | (1<<IVCE);
	MCUCR = temp | (1<<IVSEL);
#endif // GICR
	// i2c setup
	TWAR = 0x50;
	TWI_ACK;
	sei();
	
	set_sleep_mode(SLEEP_MODE_IDLE);

	for (;;) {
		sleep_mode();

		if (bExecute) {
			handle_command((uint8_t *)memarea);
			bExecute = 0;
		}
		
		if (boot_addr != -1) {
			cli();
			// Move Interrupt Vector	
#ifdef GICR
			temp = GICR;
			GICR = temp | (1<<IVCE);
			GICR = temp & ~(1<<IVSEL);
#else
			temp = MCUCR;
			MCUCR = temp | (1<<IVCE);
			MCUCR = temp & ~(1<<IVSEL);

#endif // GICR
			boot();

		}
	}

	return 0;
}
