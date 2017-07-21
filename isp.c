#include <string.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include "main.h"
#include "isp.h"

#if SPM_PAGESIZE != 64 && SPM_PAGESIZE != 128
#error "fail"
#endif


static void boot_program_page(uint32_t page, uint8_t *buf)
{
	uint16_t i;
	uint8_t sreg;

	// Disable interrupts.
	sreg = SREG;
	cli();

	eeprom_busy_wait();

	for (i=0; i<SPM_PAGESIZE; i+=2)
	{
		// Set up little-endian word.
		uint16_t w = *buf++;
		w += (*buf++) << 8;

		boot_page_fill(page + i, w);
	}

	boot_page_write(page);     // Store buffer in flash page.
	boot_spm_busy_wait();       // Wait until the memory is written.

	// Reenable RWW-section again. We need this if we want to jump back
	// to the application after bootloading.
	boot_rww_enable();

	// Re-enable interrupts (if they were ever enabled).
	SREG = sreg;
}


void handle_command(uint8_t *data) {
	uint16_t i;
	ispcmd_t *ispcmd = (ispcmd_t *)data;
	
	switch(ispcmd->cmd) {
		case 0x1: // read
			memcpy_P(data, (PGM_P)(uint16_t)(ispcmd->addr), ispcmd->len);
			break;
		case 0x2: // erase
			if (ispcmd->addr >= 0 && ispcmd->addr < BOOT_ADDR_START) {
				cli();
				boot_page_erase(ispcmd->addr);
				boot_spm_busy_wait();
				sei();
			}
			break;
		case 0x3: // write
			if (ispcmd->addr >= 0 && ispcmd->addr < BOOT_ADDR_START) {
				boot_program_page(ispcmd->addr, ispcmd->data);
			}
			break;
		case 0x5: // erase all program sections
			cli();
			for(i=0; i<BOOT_ADDR_START; i+=SPM_PAGESIZE) {
				boot_spm_busy_wait();
				boot_page_erase(i);
			}
			sei();
			break;
		case 0x6: // jump
			boot_addr = ispcmd->addr;
			break;
		case 0x7: // Get Pagesize
			data[0] = SPM_PAGESIZE;
			break;
		case 0x99: // Info
			strncpy((char*)data, "Bootloader", 11);
			break;
	}
}
