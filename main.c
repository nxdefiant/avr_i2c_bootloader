#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include "main.h"
#include "isp.h"

#define TWI_RESET	TWCR = (1<<TWEA) | (1<<TWINT) | (1<<TWEN) | (1<<TWIE)
#define TWI_ACK		TWI_RESET
#define TWI_NAK		TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWIE)

static uint8_t bForceRunning = 0;
static volatile uint8_t bExecute = 0;

#define SIZE_MEMAREA (8+SPM_PAGESIZE)
volatile uint8_t memarea[SIZE_MEMAREA];

ISR(TWI_vect)
{
	static unsigned short count = 0;
	uint8_t tmp;

	switch (TWSR & 0xF8)
	{  
		case 0x60: // start receive
			TWI_ACK;
			count = 0;
			break;
		case 0x80: // receive
			tmp = TWDR;
			if (count == 0 && tmp == 0xff) {
				// execute command
				bExecute = 1;
				TWI_NAK;
			} else {
				memarea[count++] = tmp;
				if (count < SIZE_MEMAREA) {
					TWI_ACK;
				} else {
					count = SIZE_MEMAREA-1;
					TWI_NAK;
				}
			}
			break;
		case 0xA8: // start send
			count = 0;
		case 0xB8: // send
			TWDR = memarea[count++];
			if (count < SIZE_MEMAREA) {
				TWI_ACK;
			} else {
				count = SIZE_MEMAREA-1;
				TWI_NAK;
			}
			break;
		default:
			TWI_RESET;
	}
}


int main(void)
{
	unsigned char temp;
	void (*boot)(void) = 0x0000;

	cli();

	if (eeprom_read_byte((uint8_t*)511) == 123) {
		// bootloader force running
		eeprom_write_byte((uint8_t*)511, 0);
		bForceRunning = 1;
		MCUSR = 0;
	}

	// check for valid program
	if (pgm_read_byte(0x0000) != 0xff && bForceRunning == 0) {
		boot();
	}

	boot_addr = -1;

	// Move Interrupt Vector	
	temp = GICR;
	GICR = temp | (1<<IVCE);
	GICR = temp | (1<<IVSEL);
					
	// i2c setup
	TWAR = 0x52;
	TWI_RESET;
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
			temp = GICR;
			GICR = temp | (1<<IVCE);
			GICR = temp & ~(1<<IVSEL);
			boot();
		}
	}

	return 0;
}
