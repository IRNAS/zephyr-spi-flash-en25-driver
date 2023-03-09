/*
 * COPYRIGHT NOTICE: (c) 2022 Irnas.  All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

#ifdef CONFIG_NRFX_SPIM_EXT_MUTEX
#include <spi_external_mutex.h>
#endif

LOG_MODULE_REGISTER(spi_flash_en25, CONFIG_FLASH_LOG_LEVEL);

#define DT_DRV_COMPAT irnas_en25

/* EN25 commands used by this driver: */
/* - Main Memory Byte/Page Program through Buffer 1 without Built-In Erase */
/* - Ultra-Deep Power-Down */
#define CMD_ENTER_UDPD 0x79
/* - Buffer and Page Size Configuration, "Power of 2" binary page size - should not be needed */
// #define CMD_BINARY_PAGE_SIZE {0x3D, 0x2A, 0x80, 0xA6 }

/* ------------------------------*/
/* New commmads*/

/* - Reset Enable Command */
#define CMD_RESET_ENABLE     0x66
/* - Reset Command */
#define CMD_RESET	     0x99
/* - Write Enable Command */
#define CMD_WRITE_ENABLE     0x06
/* - Manufacturer and Device ID Read */
#define CMD_READ_ID	     0x9F
/* - Status Register Read Command */
#define CMD_READ_STATUS	     0x05
/* - Status Register Write Command */
#define CMD_WRITE_STATUS     0x01
/* - Chip Erase Command */
#define CMD_READ	     0x03
/* - Page Program (Continuous Write) Command */
#define CMD_PAGE_PROGRAM     0x02
/* - Chip erase Command */
#define CMD_CHIP_ERASE	     0xC7 /* It could also be 0x60 */
/* - Sector Erase (4KB) Command */
#define CMD_SECTOR_ERASE     0x20
/* - Full Block Erase (64KB) Command */
#define CMD_FULL_BLOCK_ERASE 0xD8
/* - Half Block Erase (32KB) Command */
#define CMD_HALF_BLOCK_ERASE 0x52
/* - Enter Deep Power-Down Command */
#define CMD_ENTER_DPD	     0xB9
/* - Exit from Deep Power-Down Command */
#define CMD_EXIT_DPD	     0xAB

#define INST_HAS_WP_OR(inst)  DT_INST_NODE_HAS_PROP(inst, wp_gpios) ||
#define ANY_INST_HAS_WP_GPIOS DT_INST_FOREACH_STATUS_OKAY(INST_HAS_WP_OR) 0

#define INST_HAS_HOLD_OR(inst)	DT_INST_NODE_HAS_PROP(inst, hold_gpios) ||
#define ANY_INST_HAS_HOLD_GPIOS DT_INST_FOREACH_STATUS_OKAY(INST_HAS_HOLD_OR) 0

#define INST_HAS_EXT_MUTEX_OR(inst)  DT_INST_NODE_HAS_PROP(inst, ext_mutex_gpios) ||
#define ANY_INST_HAS_EXT_MUTEX_GPIOS DT_INST_FOREACH_STATUS_OKAY(INST_HAS_EXT_MUTEX_OR) 0

#define STATUS_REG_WRITE_IN_PROGRESS 0x01

#define STATUS_REG_LSB_PAGE_SIZE_BIT 0x01

#define DEF_BUF_SET(_name, _buf_array)                                                             \
	const struct spi_buf_set _name = {                                                         \
		.buffers = _buf_array,                                                             \
		.count = ARRAY_SIZE(_buf_array),                                                   \
	}

struct spi_flash_en25_data {
	struct k_sem lock;
#if IS_ENABLED(CONFIG_PM_DEVICE)
	uint32_t pm_state;
#endif
};

enum ext_mutex_role {
	EXT_MUTEX_ROLE_MASTER,
	EXT_MUTEX_ROLE_SLAVE,
};

struct spi_flash_en25_config {
	struct spi_dt_spec bus;
#if ANY_INST_HAS_WP_GPIOS
	const struct gpio_dt_spec *wp;
#endif
#if ANY_INST_HAS_HOLD_GPIOS
	const struct gpio_dt_spec *hold;
#endif
#if ANY_INST_HAS_EXT_MUTEX_GPIOS
	const struct gpio_dt_spec *ext_mutex;
	const struct gpio_dt_spec *spi_clk;
	enum ext_mutex_role ext_mutex_role;
#endif
#if IS_ENABLED(CONFIG_FLASH_PAGE_LAYOUT)
	struct flash_pages_layout pages_layout;
#endif
	uint32_t chip_size;

	uint32_t write_sector_size;
	uint32_t erase_full_block_size;
	uint32_t erase_half_block_size;
	uint32_t erase_sector_size;

	uint16_t t_enter_dpd; /* in microseconds */
	uint16_t t_exit_dpd;  /* in microseconds */
	bool use_udpd;
	uint8_t jedec_id[3];
};

static const struct flash_parameters flash_en25_parameters = {
	/* the smallest number of bytes that can be written at a time*/
	.write_block_size = 1,
	/* The value in a byte after an erase operation */
	.erase_value = 0xff,
};

static struct spi_flash_en25_data *get_dev_data(const struct device *dev) { return dev->data; }

static const struct spi_flash_en25_config *get_dev_config(const struct device *dev)
{
	return dev->config;
}

static void acquire(const struct device *dev) { k_sem_take(&get_dev_data(dev)->lock, K_FOREVER); }

static void release(const struct device *dev) { k_sem_give(&get_dev_data(dev)->lock); }

#if ANY_INST_HAS_EXT_MUTEX_GPIOS

static int clk_pin_check(const struct gpio_dt_spec *sck_pin)
{
	int val_sck = 0;
	for (int i = 0; i < 10000; i++) {
		val_sck = gpio_pin_get_dt(sck_pin);
		/* only one high means clock is active - spi is in use by master */
		if (val_sck == 1) {
			return -EAGAIN;
		}
		/* Wait 1 us without sleeping this thread */
		k_busy_wait(1);
	}

	return 0;
}

static int ext_mutex_clk_check(const struct device *dev)
{
	int err;
	const struct spi_flash_en25_config *dev_config = get_dev_config(dev);

	/* Configure SCK pin to input */
	err = gpio_pin_configure_dt(dev_config->spi_clk, GPIO_INPUT);
	if (err) {
		LOG_ERR("gpio_pin_configure_dt, err: %d", err);
		return -EIO;
	}

	/* Read clock a bunch of times and check if it is high at any point */
	int interations = CONFIG_SPI_FLASH_EN25_EXTERNAL_MUTEX_TIMEOUT / 20;
	for (int i = 0; i < interations; i++) {
		/* clk_pin_check takes cca 10 ms */
		err = clk_pin_check(dev_config->spi_clk);
		if (err == 0) {
			break;
		}
		k_sleep(K_MSEC(10));
	}

	if (err) {
		LOG_ERR("SCK pin is high even after waiting, err: %d", err);
		return -EAGAIN;
	}
	return 0;
}

static int ext_mutex_pin_wait(const struct device *dev)
{
	const struct spi_flash_en25_config *dev_config = get_dev_config(dev);

	/* Check pin once per ms if it is inactive */
	for (int i = 0; i < CONFIG_SPI_FLASH_EN25_EXTERNAL_MUTEX_TIMEOUT; i++) {
		int val = gpio_pin_get_dt(dev_config->ext_mutex);
		if (!val) {
			return 0;
		}
		/* Wait 1 ms without sleeping this thread */
		k_busy_wait(1000);
	}

	/* if waiting timed out, we can not lock */
	return -EAGAIN;
}

static int acquire_ext_mutex(const struct device *dev)
{
	int err = 0;
	const struct spi_flash_en25_config *dev_config = get_dev_config(dev);

	/* If ext mutex is not configured for this flash device, do nothing */
	if (!dev_config->ext_mutex) {
		return 0;
	}

	/* wait for signal pin to go low */
	err = ext_mutex_pin_wait(dev);
	if (err) {
		LOG_ERR("ext_mutex_pin_wait, err: %d", err);
		return err;
	}

	/* coinfigure signal pin to output active */
	err = gpio_pin_configure_dt(dev_config->ext_mutex, GPIO_OUTPUT_ACTIVE);
	if (err) {
		LOG_ERR("gpio_pin_configure_dt, err: %d", err);
		return -EIO;
	}

	/* if in slave role, check if clock is active */
	if (dev_config->ext_mutex_role == EXT_MUTEX_ROLE_SLAVE) {
		err = ext_mutex_clk_check(dev);
		if (err) {
			LOG_ERR("ext_mutex_clk_check, err: %d", err);
			return err;
		}
	}

	/* Configure CS pin to output */
	if (dev_config->bus.config.cs->gpio.port) {
		gpio_pin_configure_dt(&dev_config->bus.config.cs->gpio, GPIO_OUTPUT);
	}

	/* Wake SPI peripheral */
	err = pm_device_action_run(dev_config->bus.bus, PM_DEVICE_ACTION_RESUME);
	if (err && err != -EALREADY) {
		LOG_ERR("pm_device_action_run, err: %d", err);
	}
	return 0;
}

static int release_ext_mutex(const struct device *dev)
{
	int err = 0;
	const struct spi_flash_en25_config *dev_config = get_dev_config(dev);

	/* If ext mutex is not configured for this flash device, do nothing */
	if (!dev_config->ext_mutex) {
		return 0;
	}

	/* suspend SPI */
	err = pm_device_action_run(dev_config->bus.bus, PM_DEVICE_ACTION_SUSPEND);
	if (err && err != -EALREADY) {
		LOG_ERR("pm_device_action_run, err: %d", err);
	}

	/* Configure CS pin to disconnected */
	if (dev_config->bus.config.cs->gpio.port) {
		gpio_pin_configure_dt(&dev_config->bus.config.cs->gpio, GPIO_DISCONNECTED);
	}

	/* Configure signal pin to input */
	err = gpio_pin_configure_dt(dev_config->ext_mutex, GPIO_INPUT);
	if (err) {
		LOG_ERR("gpio_pin_configure_dt, err: %d", err);
		return -EIO;
	}
	return 0;
}
#else
static int acquire_ext_mutex(const struct device *dev) { return 0; }
static int release_ext_mutex(const struct device *dev) { return 0; }
#endif /* ANY_INST_HAS_EXT_MUTEX_GPIOS */

static int check_jedec_id(const struct device *dev)
{
	const struct spi_flash_en25_config *cfg = get_dev_config(dev);
	int err;
	uint8_t const *expected_id = cfg->jedec_id;
	uint8_t read_id[sizeof(cfg->jedec_id)];
	const uint8_t opcode = CMD_READ_ID;
	const struct spi_buf tx_buf[] = {{
		.buf = (void *)&opcode,
		.len = sizeof(opcode),
	}};
	const struct spi_buf rx_buf[] = {{
						 .len = sizeof(opcode),
					 },
					 {
						 .buf = read_id,
						 .len = sizeof(read_id),
					 }};
	DEF_BUF_SET(tx_buf_set, tx_buf);
	DEF_BUF_SET(rx_buf_set, rx_buf);

	err = spi_transceive_dt(&cfg->bus, &tx_buf_set, &rx_buf_set);
	if (err != 0) {
		LOG_ERR("SPI transaction failed with code: %d/%u", err, __LINE__);
		return -EIO;
	}

	if (memcmp(expected_id, read_id, sizeof(read_id)) != 0) {
		LOG_ERR("Wrong JEDEC ID: %02X %02X %02X, "
			"expected: %02X %02X %02X",
			read_id[0], read_id[1], read_id[2], expected_id[0], expected_id[1],
			expected_id[2]);
		return -ENODEV;
	}

	return 0;
}

/*
 * Reads 1-byte Status Register
 */
static int read_status_register(const struct device *dev, uint8_t *status)
{
	const struct spi_flash_en25_config *cfg = get_dev_config(dev);
	int err;
	const uint8_t opcode = CMD_READ_STATUS;
	const uint8_t empty = 0;
	const struct spi_buf tx_buf[] = {{
						 .buf = (void *)&opcode,
						 .len = sizeof(opcode),
					 },
					 {
						 .buf = (void *)&empty,
						 .len = sizeof(uint8_t),
					 }};
	const struct spi_buf rx_buf[] = {{
						 .len = sizeof(opcode),
					 },
					 {
						 .buf = status,
						 .len = sizeof(uint8_t),
					 }};
	DEF_BUF_SET(tx_buf_set, tx_buf);
	DEF_BUF_SET(rx_buf_set, rx_buf);

	err = spi_transceive_dt(&cfg->bus, &tx_buf_set, &rx_buf_set);
	if (err != 0) {
		LOG_ERR("SPI transaction failed with code: %d/%u", err, __LINE__);
		return -EIO;
	}

	*status = sys_le16_to_cpu(*status);
	return 0;
}

static int wait_until_ready(const struct device *dev)
{
	int err;
	uint8_t status;

	for (int i = 0; i < CONFIG_SPI_FLASH_EN25_READY_TIMEOUT; i++) {
		err = read_status_register(dev, &status);
		if (err != 0 || !(status & STATUS_REG_WRITE_IN_PROGRESS)) {
			return err;
		}
		k_msleep(1);
	}

	/* we are out of the loop so we have timed out */
	return -ETIMEDOUT;
}

static int send_cmd_op(const struct device *dev, uint8_t opcode, uint32_t delay)
{
	const struct spi_flash_en25_config *cfg = get_dev_config(dev);
	int err = 0;
	const struct spi_buf tx_buf[] = {{
		.buf = (void *)&opcode,
		.len = sizeof(opcode),
	}};
	DEF_BUF_SET(tx_buf_set, tx_buf);

	err = spi_write_dt(&cfg->bus, &tx_buf_set);

	if (err != 0) {
		LOG_ERR("SPI transaction failed with code: %d/%u", err, __LINE__);
		return -EIO;
	}

	k_busy_wait(delay);
	return 0;
}

static int perform_reset_sequence(const struct device *dev)
{
	int err;
	err = send_cmd_op(dev, CMD_RESET_ENABLE, 1);

	if (err != 0) {
		return err;
	}

	err = send_cmd_op(dev, CMD_RESET, 1);

	if (err != 0) {
		return err;
	}

	err = wait_until_ready(dev);
	return err;
}

static int set_write_enable(const struct device *dev)
{
	/* We add minimal delay of one microsecond, although datasheet says
	 * that it could be 30ns. */
	return send_cmd_op(dev, CMD_WRITE_ENABLE, 1);
}

static bool is_valid_request(off_t addr, size_t size, size_t chip_size)
{
	return (addr >= 0 && (addr + size) <= chip_size);
}

static int spi_flash_en25_read(const struct device *dev, off_t offset, void *data, size_t len)
{
	const struct spi_flash_en25_config *cfg = get_dev_config(dev);
	int err;

	if (!is_valid_request(offset, len, cfg->chip_size)) {
		return -ENODEV;
	}

	uint8_t const op_and_addr[] = {
		CMD_READ,
		(offset >> 16) & 0xFF,
		(offset >> 8) & 0xFF,
		(offset >> 0) & 0xFF,
	};
	const struct spi_buf tx_buf[] = {{
		.buf = (void *)&op_and_addr,
		.len = sizeof(op_and_addr),
	}};
	const struct spi_buf rx_buf[] = {{
						 .len = sizeof(op_and_addr),
					 },
					 {
						 .buf = data,
						 .len = len,
					 }};
	DEF_BUF_SET(tx_buf_set, tx_buf);
	DEF_BUF_SET(rx_buf_set, rx_buf);

	int m_err = acquire_ext_mutex(dev);
	if (m_err) {
		return m_err;
	}

	acquire(dev);
	err = spi_transceive_dt(&cfg->bus, &tx_buf_set, &rx_buf_set);
	release(dev);

	m_err = release_ext_mutex(dev);
	if (m_err) {
		return m_err;
	}

	if (err != 0) {
		LOG_ERR("SPI transaction failed with code: %d/%u", err, __LINE__);
	}

	return (err != 0) ? -EIO : 0;
}

static int perform_write(const struct device *dev, off_t offset, const void *data, size_t len)
{
	const struct spi_flash_en25_config *cfg = get_dev_config(dev);
	int err;
	err = set_write_enable(dev);
	if (err != 0) {
		return err;
	}

	uint8_t const op_and_addr[] = {
		CMD_PAGE_PROGRAM,
		(offset >> 16) & 0xFF,
		(offset >> 8) & 0xFF,
		(offset >> 0) & 0xFF,
	};
	const struct spi_buf tx_buf[] = {{
						 .buf = (void *)&op_and_addr,
						 .len = sizeof(op_and_addr),
					 },
					 {
						 .buf = (void *)data,
						 .len = len,
					 }};
	DEF_BUF_SET(tx_buf_set, tx_buf);

	err = spi_write_dt(&cfg->bus, &tx_buf_set);
	if (err != 0) {
		LOG_ERR("SPI transaction failed with code: %d/%u", err, __LINE__);
	} else {
		err = wait_until_ready(dev);
	}

	return (err != 0) ? -EIO : 0;
}

static int spi_flash_en25_write(const struct device *dev, off_t offset, const void *data,
				size_t len)
{
	const struct spi_flash_en25_config *cfg = get_dev_config(dev);
	int err = 0;

	if (!is_valid_request(offset, len, cfg->chip_size)) {
		return -ENODEV;
	}

	int m_err = acquire_ext_mutex(dev);
	if (m_err) {
		return m_err;
	}

	acquire(dev);

	while (len) {
		size_t chunk_len = len;
		off_t current_page_start = offset - (offset & (cfg->write_sector_size - 1));
		off_t current_page_end = current_page_start + cfg->write_sector_size;

		if (chunk_len > (current_page_end - offset)) {
			chunk_len = (current_page_end - offset);
		}

		err = perform_write(dev, offset, data, chunk_len);
		if (err != 0) {
			break;
		}

		data = (uint8_t *)data + chunk_len;
		offset += chunk_len;
		len -= chunk_len;
	}

	release(dev);

	m_err = release_ext_mutex(dev);
	if (m_err) {
		return m_err;
	}

	return err;
}

/* For this will stayed commented out as we dont need it */
// static int disable_block_protect(const struct device *dev)
//{
//     int err;
//     err = set_write_enable(dev);
//	if (err != 0) {
//         return err;
//     }
//
//	uint8_t const op_and_addr[] = {
//         CMD_PAGE_PROGRAM,
//		0b00111100,
//	};
//
//	const struct spi_buf tx_buf[] = {
//		{
//			.buf = (void *)&op_and_addr,
//			.len = sizeof(op_and_addr),
//		}
//	};
//
//	DEF_BUF_SET(tx_buf_set, tx_buf);
//
//	err = spi_write(get_dev_data(dev)->spi,
//			&get_dev_config(dev)->spi_cfg,
//			&tx_buf_set);
//	if (err != 0) {
//		LOG_ERR("SPI transaction failed with code: %d/%u",
//			err, __LINE__);
//	} else {
//		err = wait_until_ready(dev);
//	}
//
//	return (err != 0) ? -EIO : 0;
// }

static int perform_chip_erase(const struct device *dev)
{
	const struct spi_flash_en25_config *cfg = get_dev_config(dev);
	int err;
	err = set_write_enable(dev);
	if (err != 0) {
		return err;
	}

	uint8_t const chip_erase_cmd = CMD_CHIP_ERASE;
	const struct spi_buf tx_buf[] = {{
		.buf = (void *)&chip_erase_cmd,
		.len = sizeof(chip_erase_cmd),
	}};
	DEF_BUF_SET(tx_buf_set, tx_buf);

	err = spi_write_dt(&cfg->bus, &tx_buf_set);
	if (err != 0) {
		LOG_ERR("SPI transaction failed with code: %d/%u", err, __LINE__);
	} else {
		err = wait_until_ready(dev);
	}

	return (err != 0) ? -EIO : 0;
}

/**
 * @brief Check if erase can be performed at this offset with this size
 *
 * @param[in] entity_size The size of the erasable entity (block/sector of flash)
 * @param[in] offset The current offset of the erase operation
 * @param[in] requested_size The number of bytes to erase
 *
 * @retval true The requested erase operation can be performed
 * @retval false The requested erase operation can NOT be performed
 */
static bool is_erase_possible(size_t entity_size, off_t offset, size_t requested_size)
{
	/* 1. The size we want to erase must be the same or larger then the entity size
	 * 2. The offset must be a multiple of the requested entity size
	 */
	return (requested_size >= entity_size) && (offset % entity_size == 0);
}

static int perform_erase_op(const struct device *dev, uint8_t opcode, off_t offset)
{
	const struct spi_flash_en25_config *cfg = get_dev_config(dev);
	int err;
	err = set_write_enable(dev);
	if (err != 0) {
		return err;
	}

	uint8_t const op_and_addr[] = {
		opcode,
		(offset >> 16) & 0xFF,
		(offset >> 8) & 0xFF,
		(offset >> 0) & 0xFF,
	};
	const struct spi_buf tx_buf[] = {{
		.buf = (void *)&op_and_addr,
		.len = sizeof(op_and_addr),
	}};
	DEF_BUF_SET(tx_buf_set, tx_buf);

	err = spi_write_dt(&cfg->bus, &tx_buf_set);
	if (err != 0) {
		LOG_ERR("SPI transaction failed with code: %d/%u", err, __LINE__);
	} else {
		err = wait_until_ready(dev);
	}

	return (err != 0) ? -EIO : 0;
}

static int spi_flash_en25_erase(const struct device *dev, off_t offset, size_t size)
{
	const struct spi_flash_en25_config *cfg = get_dev_config(dev);
	int err = 0;

	if (!is_valid_request(offset, size, cfg->chip_size)) {
		return -ENODEV;
	}

	/* Diagnose region errors before starting to erase. */
	if (((offset % cfg->erase_sector_size) != 0) || ((size % cfg->erase_sector_size) != 0)) {
		return -EINVAL;
	}

	int m_err = acquire_ext_mutex(dev);
	if (m_err) {
		return m_err;
	}

	acquire(dev);

	if (size == cfg->chip_size) {
		err = perform_chip_erase(dev);
	} else {
		while (size) {
			/* Can we erase a full block? */
			if (is_erase_possible(cfg->erase_full_block_size, offset, size)) {
				err = perform_erase_op(dev, CMD_FULL_BLOCK_ERASE, offset);
				offset += cfg->erase_full_block_size;
				size -= cfg->erase_full_block_size;
			}
			/* Can we erase a half block? */
			else if (is_erase_possible(cfg->erase_half_block_size, offset, size)) {
				err = perform_erase_op(dev, CMD_HALF_BLOCK_ERASE, offset);
				offset += cfg->erase_half_block_size;
				size -= cfg->erase_half_block_size;
			}
			/* Can we erase a sector? */
			else if (is_erase_possible(cfg->erase_sector_size, offset, size)) {
				err = perform_erase_op(dev, CMD_SECTOR_ERASE, offset);
				offset += cfg->erase_sector_size;
				size -= cfg->erase_sector_size;
			} else {
				LOG_ERR("Unsupported erase request: "
					"size %zu at 0x%lx",
					size, (long)offset);
				err = -EINVAL;
			}

			if (err != 0) {
				break;
			}
		}
	}

	release(dev);

	m_err = release_ext_mutex(dev);
	if (m_err) {
		return m_err;
	}

	return err;
}

#if IS_ENABLED(CONFIG_FLASH_PAGE_LAYOUT)
static void spi_flash_en25_pages_layout(const struct device *dev,
					const struct flash_pages_layout **layout,
					size_t *layout_size)
{
	/* NOTE: The page size specified in the layout here is the smallest erasable sector of the
	 * chip, as specified in the zephyr flash API.
	 */
	*layout = &get_dev_config(dev)->pages_layout;
	*layout_size = 1;
}
#endif /* IS_ENABLED(CONFIG_FLASH_PAGE_LAYOUT) */

static int spi_flash_en25_init(const struct device *dev)
{
	const struct spi_flash_en25_config *dev_config = get_dev_config(dev);
	int err;

	if (!spi_is_ready(&dev_config->bus)) {
		LOG_ERR("SPI bus %s not ready", dev_config->bus.bus->name);
		return -ENODEV;
	}

	/* GPIO configure */

#if ANY_INST_HAS_WP_GPIOS
	if (dev_config->wp) {
		if (gpio_pin_configure_dt(dev_config->wp, GPIO_OUTPUT_ACTIVE)) {
			LOG_ERR("Couldn't configure write protect pin");
			return -ENODEV;
		}
		gpio_pin_set(dev_config->wp->port, dev_config->wp->pin, 1);
	}
#endif

#if ANY_INST_HAS_HOLD_GPIOS
	if (dev_config->hold) {
		if (gpio_pin_configure_dt(dev_config->hold, GPIO_OUTPUT_ACTIVE)) {
			LOG_ERR("Couldn't configure hold pin");
			return -ENODEV;
		}
		gpio_pin_set(dev_config->hold->port, dev_config->hold->pin, 1);
	}
#endif

#if ANY_INST_HAS_EXT_MUTEX_GPIOS
	if (dev_config->ext_mutex) {
		if (gpio_pin_configure_dt(dev_config->ext_mutex, GPIO_INPUT)) {
			LOG_ERR("Couldn't configure ext_mutex pin");
			return -EIO;
		}
	}
#endif

	int m_err = acquire_ext_mutex(dev);
	if (m_err) {
		return m_err;
	}

	acquire(dev);

	/* Perform reset sequence as we have seen that fetching
	 * JEDEC ID failed otherwise. */
	err = perform_reset_sequence(dev);
	if (err != 0) {
		LOG_ERR("perform_reset_sequence, err: %d", err);
		release(dev);
		release_ext_mutex(dev);
		return err;
	}

	/* Just in case the chip was in the Deep (or Ultra-Deep) Power-Down
	 * mode, issue the command to bring it back to normal operation.
	 * Exiting from the Ultra-Deep mode requires only that the CS line
	 * is asserted for a certain time, so issuing the Resume from Deep
	 * Power-Down command will work in both cases.
	 */
	send_cmd_op(dev, CMD_EXIT_DPD, dev_config->t_exit_dpd);

	/* Check jedec ID */
#ifdef CONFIG_SPI_FLASH_EN25_JEDEC_CHECK_AT_INIT
	err = check_jedec_id(dev);
	if (err != 0) {
		release(dev);
		release_ext_mutex(dev);
		return err;
	}
#else
	LOG_INF("SPI_FLASH_EN25_JEDEC_CHECK_AT_INIT is not set, skipping JEDEC ID check.");
#endif

	/* Place holder for function call, we might need it in future. */
	// err = disable_block_protect(dev);

	release(dev);

	m_err = release_ext_mutex(dev);
	if (m_err) {
		return m_err;
	}

	/* Read one byte - this puts flash into a low power state */
	uint8_t _r[4];
	err = spi_flash_en25_read(dev, 0, _r, sizeof(_r));

	return err;
}

#if IS_ENABLED(CONFIG_PM_DEVICE)
static int spi_flash_en25_pm_control(const struct device *dev, enum pm_device_action action)
{
	const struct spi_flash_en25_config *dev_config = get_dev_config(dev);

	int err = 0;
	int m_err = acquire_ext_mutex(dev);
	if (m_err) {
		return m_err;
	}
	acquire(dev);

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		send_cmd_op(dev, CMD_EXIT_DPD, dev_config->t_exit_dpd);
		break;

	case PM_DEVICE_ACTION_SUSPEND:
		send_cmd_op(dev, CMD_ENTER_DPD, dev_config->t_enter_dpd);
		break;

	default:
		err = -ENOTSUP;
	}

	release(dev);

	m_err = release_ext_mutex(dev);
	if (m_err) {
		return m_err;
	}

	return err;
}
#endif /* IS_ENABLED(CONFIG_PM_DEVICE) */

static const struct flash_parameters *flash_en25_get_parameters(const struct device *dev)
{
	ARG_UNUSED(dev);

	return &flash_en25_parameters;
}

static const struct flash_driver_api spi_flash_en25_api = {
	.read = spi_flash_en25_read,
	.write = spi_flash_en25_write,
	.erase = spi_flash_en25_erase,
	.get_parameters = flash_en25_get_parameters,
#if IS_ENABLED(CONFIG_FLASH_PAGE_LAYOUT)
	.page_layout = spi_flash_en25_pages_layout,
#endif
};

#define DT_DRV_COMPAT irnas_en25

#define XSTR(x) STR(x)
#define STR(x)	#x

#define INST_HAS_WP_GPIO(idx) DT_NODE_HAS_PROP(DT_DRV_INST(idx), wp_gpios)

#define INST_WP_GPIO_SPEC(idx)                                                                     \
	IF_ENABLED(INST_HAS_WP_GPIO(idx), (static const struct gpio_dt_spec wp_##idx =             \
						   GPIO_DT_SPEC_GET(DT_DRV_INST(idx), wp_gpios);))

#define INST_HAS_HOLD_GPIO(idx) DT_NODE_HAS_PROP(DT_DRV_INST(idx), hold_gpios)

#define INST_HOLD_GPIO_SPEC(idx)                                                                   \
	IF_ENABLED(INST_HAS_HOLD_GPIO(idx),                                                        \
		   (static const struct gpio_dt_spec hold_##idx =                                  \
			    GPIO_DT_SPEC_GET(DT_DRV_INST(idx), hold_gpios);))

#define INST_HAS_EXT_MUTEX_GPIO(idx) DT_NODE_HAS_PROP(DT_DRV_INST(idx), ext_mutex_gpios)

#define INST_EXT_MUTEX_GPIO_SPEC(idx)                                                              \
	IF_ENABLED(INST_HAS_EXT_MUTEX_GPIO(idx),                                                   \
		   (static const struct gpio_dt_spec ext_mutex_##idx =                             \
			    GPIO_DT_SPEC_GET(DT_DRV_INST(idx), ext_mutex_gpios);))

#define INST_HAS_SPI_CLK_GPIO(idx) DT_NODE_HAS_PROP(DT_DRV_INST(idx), spi_clk_gpios)

#define INST_SPI_CLK_GPIO_SPEC(idx)                                                                \
	IF_ENABLED(INST_HAS_SPI_CLK_GPIO(idx),                                                     \
		   (static const struct gpio_dt_spec spi_clk_##idx =                               \
			    GPIO_DT_SPEC_GET(DT_DRV_INST(idx), spi_clk_gpios);))

#define SPI_FLASH_EN25_INST(idx)                                                                   \
	enum {                                                                                     \
		INST_##idx##_BYTES = (DT_INST_PROP(idx, size) / 8),                                \
		INST_##idx##_PAGES = (INST_##idx##_BYTES / DT_INST_PROP(idx, erase_sector_size)),  \
	};                                                                                         \
	static struct spi_flash_en25_data inst_##idx##_data = {                                    \
		.lock = Z_SEM_INITIALIZER(inst_##idx##_data.lock, 1, 1),                           \
		IF_ENABLED(CONFIG_PM_DEVICE, (.pm_state = PM_DEVICE_STATE_ACTIVE))};               \
	INST_WP_GPIO_SPEC(idx)                                                                     \
	INST_HOLD_GPIO_SPEC(idx)                                                                   \
	INST_EXT_MUTEX_GPIO_SPEC(idx)                                                              \
	INST_SPI_CLK_GPIO_SPEC(idx)                                                                \
	static const struct spi_flash_en25_config inst_##idx##_config = {                          \
		.bus = SPI_DT_SPEC_INST_GET(idx,                                                   \
					    SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB |                \
						    SPI_WORD_SET(8) | SPI_LINES_SINGLE,            \
					    0),                                                    \
		IF_ENABLED(INST_HAS_WP_GPIO(idx), (.wp = &wp_##idx, ))                             \
			IF_ENABLED(INST_HAS_HOLD_GPIO(idx), (.hold = &hold_##idx, ))               \
				IF_ENABLED(CONFIG_FLASH_PAGE_LAYOUT,                               \
					   (.pages_layout =                                        \
						    {                                              \
							    .pages_count = INST_##idx##_PAGES,     \
							    .pages_size = DT_INST_PROP(            \
								    idx, erase_sector_size),       \
						    }, ))                                          \
					.chip_size = INST_##idx##_BYTES,                           \
		.write_sector_size = DT_INST_PROP(idx, write_sector_size),                         \
		.erase_full_block_size = DT_INST_PROP(idx, erase_full_block_size),                 \
		.erase_half_block_size = DT_INST_PROP(idx, erase_half_block_size),                 \
		.erase_sector_size = DT_INST_PROP(idx, erase_sector_size),                         \
		.t_enter_dpd =                                                                     \
			ceiling_fraction(DT_INST_PROP(idx, enter_dpd_delay), NSEC_PER_USEC),       \
		.t_exit_dpd = ceiling_fraction(DT_INST_PROP(idx, exit_dpd_delay), NSEC_PER_USEC),  \
		.use_udpd = DT_INST_PROP(idx, use_udpd),                                           \
		.jedec_id = DT_INST_PROP(idx, jedec_id),                                           \
		IF_ENABLED(INST_HAS_EXT_MUTEX_GPIO(idx), (.ext_mutex = &ext_mutex_##idx, ))        \
			IF_ENABLED(INST_HAS_SPI_CLK_GPIO(idx), (.spi_clk = &spi_clk_##idx, ))      \
				IF_ENABLED(INST_HAS_EXT_MUTEX_GPIO(idx),                           \
					   (.ext_mutex_role =                                      \
						    DT_INST_ENUM_IDX(idx, ext_mutex_role), ))};    \
	IF_ENABLED(CONFIG_FLASH_PAGE_LAYOUT,                                                       \
		   (BUILD_ASSERT((INST_##idx##_PAGES * DT_INST_PROP(idx, erase_sector_size)) ==    \
					 INST_##idx##_BYTES,                                       \
				 "erase-sector-size specified for instance " #idx " of "           \
				 "irnas,en25 is not compatible with its "                          \
				 "total size");))                                                  \
	PM_DEVICE_DT_INST_DEFINE(idx, spi_flash_en25_pm_control);                                  \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(idx, spi_flash_en25_init, PM_DEVICE_DT_INST_GET(idx),                \
			      &inst_##idx##_data, &inst_##idx##_config, POST_KERNEL,               \
			      CONFIG_SPI_FLASH_EN25_INIT_PRIORITY, &spi_flash_en25_api);

DT_INST_FOREACH_STATUS_OKAY(SPI_FLASH_EN25_INST)