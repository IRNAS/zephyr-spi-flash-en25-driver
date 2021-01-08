/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <drivers/flash.h>
#include <drivers/gpio.h>
#include <logging/log_ctrl.h>


//LOG_MODULE_REGISTER(main);



#if !DT_NODE_EXISTS(DT_NODELABEL(flash42))
#error "whoops"
#endif

#define FLASH_DEVICE DT_LABEL(DT_NODELABEL(flash42))


/* Set to 1 to test the chip erase functionality. Please be aware that this
 * operation takes quite a while (it depends on the chip size, but can easily
 * take tens of seconds).
 * Note - erasing of the test region or whole chip is performed only when
 *        CONFIG_SPI_FLASH_AT45_USE_READ_MODIFY_WRITE is not enabled.
 */
#define ERASE_WHOLE_CHIP    0

#define TEST_REGION_OFFSET  0xFE00
#define TEST_REGION_SIZE    0x400

static uint8_t write_buf[TEST_REGION_SIZE];
static uint8_t read_buf[TEST_REGION_SIZE];

static int spi_flash_at45_init(const struct device *dev);

void main(void)
{
	printk("Hello world, using: %s\n", CONFIG_BOARD);

	const struct device *flash_dev;
	int i;
	int err;
	uint8_t data;
	struct flash_pages_info pages_info;
	size_t page_count, chip_size;



	const struct device * dev;
	dev = device_get_binding("GPIO_0");
	if (dev == NULL) {
		printk("Device %s not found!\n", "GPIO_0");
		return;
	}

    /* Keep lr reset, flash reset and flash wp high*/
	gpio_pin_configure(dev, 16, GPIO_OUTPUT);
	gpio_pin_set(dev, 16, 1);
	gpio_pin_configure(dev, 0, GPIO_OUTPUT);
	gpio_pin_set(dev, 0, 1);
	gpio_pin_configure(dev, 31, GPIO_OUTPUT);
	gpio_pin_set(dev, 31, 1);


    while(1)
    {
        printk("Hello world, using: %s\n", CONFIG_BOARD);
        k_sleep(K_MSEC(1000));
        printk("Hello world, using: %s\n", CONFIG_BOARD);
        k_sleep(K_MSEC(1000));
    }


}


//static int spi_flash_at45_init(const struct device *dev)
//{
//	struct spi_flash_at45_data *dev_data = get_dev_data(dev);
//	const struct spi_flash_at45_config *dev_config = get_dev_config(dev);
//	int err;
//
//	LOG_ERR("STARTING MY INIT");
//	printk("STARTING MY INIT");
//
//	dev_data->spi = device_get_binding(dev_config->spi_bus);
//	if (!dev_data->spi) {
//		LOG_ERR("Cannot find %s", dev_config->spi_bus);
//		return -ENODEV;
//	}
//
//	if (dev_config->cs_gpio) {
//		dev_data->spi_cs.gpio_dev =
//			device_get_binding(dev_config->cs_gpio);
//		if (!dev_data->spi_cs.gpio_dev) {
//			LOG_ERR("Cannot find %s", dev_config->cs_gpio);
//			return -ENODEV;
//		}
//
//		dev_data->spi_cs.gpio_pin = dev_config->cs_pin;
//		dev_data->spi_cs.gpio_dt_flags = dev_config->cs_dt_flags;
//		dev_data->spi_cs.delay = 0;
//	}
//
//	acquire(dev);
//
//	/* Just in case the chip was in the Deep (or Ultra-Deep) Power-Down
//	 * mode, issue the command to bring it back to normal operation.
//	 * Exiting from the Ultra-Deep mode requires only that the CS line
//	 * is asserted for a certain time, so issuing the Resume from Deep
//	 * Power-Down command will work in both cases.
//	 */
//	power_down_op(dev, CMD_EXIT_DPD, dev_config->t_exit_dpd);
//
//	err = check_jedec_id(dev);
//	if (err == 0) {
//		err = configure_page_size(dev);
//	}
//
//	release(dev);
//
//	return err;
//}
