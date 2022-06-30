#ifndef _CLK_PROTO_H_
#define _CLK_PROTO_H_

void fixed_rate_scan(void);

#if CONFIG_CLK_BCM2835
void bcm2835_clk_scan(void);
#endif

#endif
