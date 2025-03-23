void rs232_scan(void);
void roomuart_scan(void);

void arch_init_serial(void)
{
    rs232_scan();
    roomuart_scan();
}
