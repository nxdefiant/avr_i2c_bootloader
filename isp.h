#ifndef ISP_H
#define ISP_H

#define BOOT_ADDR_START 0x1c00

typedef struct _ispcmd {
	uint8_t cmd;
	uint32_t addr;
	uint8_t len;
	uint8_t data[SPM_PAGESIZE];
} ispcmd_t;

void handle_command(uint8_t *data);

#endif
