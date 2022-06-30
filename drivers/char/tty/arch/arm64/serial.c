void pl011_scan(void);
void bcm2835aux_scan(void);

void arch_init_serial(void)
{
    bcm2835aux_scan();
    pl011_scan();
}
