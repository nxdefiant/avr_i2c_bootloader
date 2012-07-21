#ifndef MAIN_H
#define MAIN_H

#define MAX(a, b) ( (a)>(b) ? (a) : (b) )
#define MIN(a, b) ( (a)<(b) ? (a) : (b) )
#define BETWEEN(a, b, c) ((a <= b && b <= c) ? 1 : 0)

int32_t boot_addr;
void wait_rfm_done_send(void);

#endif

