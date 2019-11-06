/*
 * Copyright (c) 2019 Sodaq BV.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <drivers/pinmux.h>

static int board_pinmux_init(struct device *dev)
{
	struct device *porta = device_get_binding(DT_ATMEL_SAM0_PINMUX_PINMUX_A_LABEL);
	struct device *portb = device_get_binding(DT_ATMEL_SAM0_PINMUX_PINMUX_B_LABEL);

	ARG_UNUSED(dev);

#if DT_ATMEL_SAM0_UART_SERCOM_0_BASE_ADDRESS
	/* SERCOM0 UART on RX=PA9, TX=PA10 */
	pinmux_pin_set(porta, 9, PINMUX_FUNC_C);
	pinmux_pin_set(porta, 10, PINMUX_FUNC_C);
#endif

#if DT_ATMEL_SAM0_UART_SERCOM_5_BASE_ADDRESS
	/* SERCOM5 UART on RX=PB31, TX=PB30, RTS=PB22, CTS=PB23 */
	pinmux_pin_set(portb, 31, PINMUX_FUNC_D);
	pinmux_pin_set(portb, 30, PINMUX_FUNC_D);
	pinmux_pin_set(portb, 22, PINMUX_FUNC_D);
	pinmux_pin_set(portb, 23, PINMUX_FUNC_D);
#endif

#if DT_ATMEL_SAM0_UART_SERCOM_1_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_ATMEL_SAM0_UART_SERCOM_2_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_ATMEL_SAM0_UART_SERCOM_3_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_ATMEL_SAM0_UART_SERCOM_4_BASE_ADDRESS
#error Pin mapping is not configured
#endif

#if DT_ATMEL_SAM0_SPI_SERCOM_3_BASE_ADDRESS
	/* SERCOM3 SPI on MISO=PA22/pad 0, MOSI=PA20/pad 2, SCK=PA21/pad 3, SS=PA23/pad 1 */
	pinmux_pin_set(porta, 22, PINMUX_FUNC_C);
	pinmux_pin_set(portb, 20, PINMUX_FUNC_D);
	pinmux_pin_set(portb, 21, PINMUX_FUNC_D);
	pinmux_pin_set(portb, 23, PINMUX_FUNC_C);
#endif

#if DT_ATMEL_SAM0_SPI_SERCOM_0_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_ATMEL_SAM0_SPI_SERCOM_1_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_ATMEL_SAM0_SPI_SERCOM_2_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_ATMEL_SAM0_SPI_SERCOM_4_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_ATMEL_SAM0_SPI_SERCOM_5_BASE_ADDRESS
#error Pin mapping is not configured
#endif

#ifdef CONFIG_USB_DC_SAM0
	/* USB DP on PA25, USB DM on PA24 */
	pinmux_pin_set(porta, 25, PINMUX_FUNC_G);
	pinmux_pin_set(porta, 24, PINMUX_FUNC_G);
#endif

	return 0;
}

SYS_INIT(board_pinmux_init, PRE_KERNEL_1, CONFIG_PINMUX_INIT_PRIORITY);
