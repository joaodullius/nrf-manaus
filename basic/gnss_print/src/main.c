#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#define UART_NODE DT_NODELABEL(arduino_serial)

int main(void) {

    unsigned char recv_char;

    const struct device *uart = DEVICE_DT_GET(UART_NODE);
    if (uart == NULL || !device_is_ready(uart)) {
        printk("Could not open UART");
        return 0;
    }

    printk("Recebendo dados do MAX-M10...\n");

    while(1) {
		while(uart_poll_in(uart, &recv_char) < 0){
			k_yield();
		}
		printk("%c", recv_char);
		k_yield();
	}

}
