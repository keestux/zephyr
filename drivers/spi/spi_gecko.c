/*
 * Copyright (c) 2019 Christian Taedcke <hacking@taedcke.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_LEVEL CONFIG_SPI_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(spi_gecko);
#include "spi_context.h"

#include <sys/sys_io.h>
#include <device.h>
#include <drivers/spi.h>
#include <soc.h>

#include "em_cmu.h"
#include "em_usart.h"

#include <stdbool.h>

#ifndef CONFIG_SOC_GECKO_HAS_INDIVIDUAL_PIN_LOCATION
#error "This EFM32 USART SPI driver is only implemented for devices that \
 support individual pin locations"
#endif

#define USART_PREFIX cmuClock_USART
#define CLOCK_ID_PRFX2(prefix, suffix) prefix##suffix
#define CLOCK_ID_PRFX(prefix, suffix) CLOCK_ID_PRFX2(prefix, suffix)
#define CLOCK_USART(id) CLOCK_ID_PRFX(USART_PREFIX, id)

#define SPI_MAX_CS_SIZE 3
#define SPI_WORD_SIZE 8

#define SPI_DATA(dev) ((struct spi_gecko_data *) ((dev)->driver_data))

/* Structure Declarations */

struct spi_gecko_data {
	struct spi_context ctx;
};

struct spi_gecko_config {
    	USART_TypeDef *base;
	CMU_Clock_TypeDef clock;
	struct soc_gpio_pin pin_rx;
	struct soc_gpio_pin pin_tx;
        struct soc_gpio_pin pin_clk;
        struct soc_gpio_pin pin_cs;
	u8_t loc_rx;
	u8_t loc_tx;
        u8_t loc_clk;
        u8_t loc_cs;
};


/* Helper Functions */
static int spi_config(struct device *dev, const struct spi_config *config, u16_t *control)
{
	const struct spi_gecko_config *gecko_config = dev->config->config_info;
	u8_t cs = 0x00;

	if (config->slave != 0) {
		if (config->slave >= SPI_MAX_CS_SIZE) {
			LOG_ERR("More slaves than supported");
			return -ENOTSUP;
		}
		cs = (u8_t)(config->slave);
	}

	if (SPI_WORD_SIZE_GET(config->operation) != 8) {
		LOG_ERR("Word size must be %d", SPI_WORD_SIZE);
		return -ENOTSUP;
	}

	if (config->operation & SPI_CS_ACTIVE_HIGH) {
		LOG_ERR("CS active high not supported");
		return -ENOTSUP;
	}

	if (config->operation & SPI_LOCK_ON) {
		LOG_ERR("Lock On not supported");
		return -ENOTSUP;
	}

	if ((config->operation & SPI_LINES_MASK) != SPI_LINES_SINGLE) {
		LOG_ERR("Only supports single mode");
		return -ENOTSUP;
	}

	if (config->operation & SPI_TRANSFER_LSB) {
		LOG_ERR("LSB first not supported");
		return -ENOTSUP;
	}

	if (config->operation & (SPI_MODE_CPOL | SPI_MODE_CPHA)) {
		LOG_ERR("Only supports CPOL=CPHA=0");
		return -ENOTSUP;
	}

	if (config->operation & SPI_OP_MODE_SLAVE) {
		LOG_ERR("Slave mode not supported");
		return -ENOTSUP;
	}

	/* Set Loopback */
	if (config->operation & SPI_MODE_LOOP) {
	    gecko_config->base->CTRL |= USART_CTRL_LOOPBK;
	} else {
	    /* TODO: is this necessary and/or correct? */
	    gecko_config->base->CTRL &= ~USART_CTRL_LOOPBK;
	}
	
	/* Set word size */
	gecko_config->base->FRAME = (uint32_t)SPI_WORD_SIZE_GET(config->operation)
                 | USART_FRAME_STOPBITS_DEFAULT
                 | USART_FRAME_PARITY_DEFAULT;

	/* Enable automatic chip select */
	gecko_config->base->CTRL |= USART_CTRL_AUTOCS;
	
	return 0;
}

static void spi_gecko_send(struct device *dev, u8_t frame, u16_t control)
{
        const struct spi_gecko_config *config = dev->config->config_info;
	
	/* Write frame to register */
        USART_Tx(config->base, frame);

	/* Start the transfer */
	/* TODO what to do here? */

	/* Wait until the transfer ends */
	while (!(config->base->STATUS & USART_STATUS_TXC))
	        ;
}

static u8_t spi_gecko_recv(USART_TypeDef *usart)
{
        /* Return data inside rx register */
	return (u8_t)usart->RXDATA;
}

static void spi_gecko_xfer(struct device *dev,
		const struct spi_config *config, u16_t control)
{
	struct spi_context *ctx = &SPI_DATA(dev)->ctx;
	u32_t send_len = spi_context_longest_current_buf(ctx);
	u8_t read_data;

	for (u32_t i = 0; i < send_len; i++) {
		/* Send a frame */
		if (i < ctx->tx_len) {
			spi_gecko_send(dev, (u8_t) (ctx->tx_buf)[i],
					control);
		} else {
			/* Send dummy bytes */
			spi_gecko_send(dev, 0, control);
		}
		/* Receive a frame */
		read_data = spi_gecko_recv(((struct spi_gecko_config *)dev->config->config_info)->base);
		if (i < ctx->rx_len) {
			ctx->rx_buf[i] = read_data;
		}
	}
	spi_context_complete(ctx, 0);
}

static void spi_gecko_init_pins(struct device *dev)
{
	const struct spi_gecko_config *config = dev->config->config_info;

	soc_gpio_configure(&config->pin_rx);
	soc_gpio_configure(&config->pin_tx);
	soc_gpio_configure(&config->pin_clk);
	soc_gpio_configure(&config->pin_cs);

	/* disable all pins while configuring */
	config->base->ROUTEPEN = 0;
	
	config->base->ROUTELOC0 =
		(config->loc_tx << _USART_ROUTELOC0_TXLOC_SHIFT) |
		(config->loc_rx << _USART_ROUTELOC0_RXLOC_SHIFT) |
	        (config->loc_clk << _USART_ROUTELOC0_CLKLOC_SHIFT) |
		(config->loc_cs << _USART_ROUTELOC0_CSLOC_SHIFT);
	config->base->ROUTELOC1 = _USART_ROUTELOC1_RESETVALUE;
	
	config->base->ROUTEPEN = USART_ROUTEPEN_RXPEN | USART_ROUTEPEN_TXPEN |
	    USART_ROUTEPEN_CLKPEN | USART_ROUTEPEN_CSPEN;
}


/* API Functions */

static int spi_gecko_init(struct device *dev)
{
	const struct spi_gecko_config *config = dev->config->config_info;
	USART_InitSync_TypeDef usartInit = USART_INITSYNC_DEFAULT;

	/* The peripheral and gpio clock are already enabled from soc and gpio
	 * driver
	 */

        usartInit.enable = usartDisable;
	usartInit.baudrate = 1000000;
	usartInit.databits = usartDatabits8;
	usartInit.master = 1;
	usartInit.msbf = 1;
	usartInit.clockMode = usartClockMode0;
#if defined( USART_INPUT_RXPRS ) && defined( USART_TRIGCTRL_AUTOTXTEN )
	usartInit.prsRxEnable = 0;
	usartInit.prsRxCh = 0;
	usartInit.autoTx = 0;
#endif
        /* TODO: Handle CS */
        usartInit.autoCsEnable = false;

	/* Enable USART clock */
	CMU_ClockEnable(config->clock, true);

	/* Init USART */
	USART_InitSync(config->base, &usartInit);

	/* Initialize USART pins */
	spi_gecko_init_pins(dev);

	/* Enable the peripheral */
	config->base->CMD = (uint32_t) usartEnable;

	return 0;
}

static int spi_gecko_transceive(struct device *dev,
			  const struct spi_config *config,
			  const struct spi_buf_set *tx_bufs,
			  const struct spi_buf_set *rx_bufs)
{
	u16_t control = 0;

	spi_config(dev, config, &control);
	spi_context_buffers_setup(&SPI_DATA(dev)->ctx, tx_bufs, rx_bufs, 1);
	spi_gecko_xfer(dev, config, control);
	return 0;
}

#ifdef CONFIG_SPI_ASYNC
static int spi_gecko_transceive_async(struct device *dev,
			  const struct spi_config *config,
			  const struct spi_buf_set *tx_bufs,
			  const struct spi_buf_set *rx_bufs,
			  struct k_poll_signal *async)
{
	return -ENOTSUP;
}
#endif /* CONFIG_SPI_ASYNC */

static int spi_gecko_release(struct device *dev,
		const struct spi_config *config)
{
        const struct spi_gecko_config *gecko_config = dev->config->config_info;

	if (!(gecko_config->base->STATUS & USART_STATUS_TXIDLE)) {
		return -EBUSY;
	}
	return 0;
}

/* Device Instantiation */
static struct spi_driver_api spi_gecko_api = {
	.transceive = spi_gecko_transceive,
#ifdef CONFIG_SPI_ASYNC
	.transceive_async = spi_gecko_transceive_async,
#endif /* CONFIG_SPI_ASYNC */
	.release = spi_gecko_release,
};

#define SPI_INIT3(n, usart)					    \
	static struct spi_gecko_data spi_gecko_data_##n = { \
		SPI_CONTEXT_INIT_LOCK(spi_gecko_data_##n, ctx), \
		SPI_CONTEXT_INIT_SYNC(spi_gecko_data_##n, ctx), \
	}; \
	static struct spi_gecko_config spi_gecko_cfg_##n = { \
	    .base = (USART_TypeDef *) \
                 DT_INST_##n##_SILABS_GECKO_USART_SPI_BASE_ADDRESS, \
	    .clock = CLOCK_USART(usart), \
	    .pin_rx = { DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_RX_1, \
			DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_RX_2, \
			gpioModeInput, 1},				\
	    .pin_tx = { DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_TX_1, \
			DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_TX_2, \
			gpioModePushPull, 1},				\
            .pin_clk = { DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_CLK_1, \
			DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_CLK_2, \
			gpioModePushPull, 1},				\
            .pin_cs = { DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_CS_1, \
			DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_CS_2, \
			gpioModePushPull, 1},				\
	    .loc_rx = DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_RX_0, \
	    .loc_tx = DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_TX_0, \
            .loc_clk = DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_CLK_0, \
	    .loc_cs = DT_INST_##n##_SILABS_GECKO_USART_SPI_LOCATION_CS_0, \
	}; \
	DEVICE_AND_API_INIT(spi_##n, \
			DT_INST_##n##_SILABS_GECKO_USART_SPI_LABEL, \
			spi_gecko_init, \
			&spi_gecko_data_##n, \
			&spi_gecko_cfg_##n, \
			POST_KERNEL, \
			CONFIG_SPI_INIT_PRIORITY, \
			&spi_gecko_api)

#define SPI_INIT2(n, usart) SPI_INIT3(n, usart) 
#define SPI_INIT(n) SPI_INIT2(n, DT_INST_##n##_SILABS_GECKO_USART_SPI_PERIPHERAL_ID)

#ifdef DT_INST_0_SILABS_GECKO_USART_SPI_LABEL

SPI_INIT(0);

#endif /* DT_INST_0_GECKO_SPI_LABEL */
