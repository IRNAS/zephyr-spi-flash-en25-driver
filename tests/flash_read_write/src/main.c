#include <devicetree.h>
#include <drivers/flash.h>
#include <drivers/gpio.h>
#include <logging/log_ctrl.h>
#include <pm/device.h>
#include <storage/flash_map.h>
#include <zephyr.h>
#include <ztest.h>

#define FLASH_DEVICE	  DT_LABEL(DT_NODELABEL(en25qh32b))
#define CHIP_SIZE_BITS	  DT_PROP(DT_NODELABEL(en25qh32b), size)
#define ERASE_SECTOR_SIZE DT_PROP(DT_NODELABEL(en25qh32b), erase_sector_size)

/* Since we erase the test region, it's offset must be a multiple of the erase-sector-size */
#define TEST_REGION_OFFSET (ERASE_SECTOR_SIZE * 4)
/* The size of test region must also be a multiple of the erase-sector-size, for the same reason */
#define TEST_REGION_SIZE   (ERASE_SECTOR_SIZE * 2)

#define ERASE_BUF_SIZE ERASE_SECTOR_SIZE

/* Define stattically so not a lot of additional stack space is required */
static uint8_t write_buf[TEST_REGION_SIZE];
static uint8_t read_buf[TEST_REGION_SIZE];
static uint8_t erase_check_buf[ERASE_BUF_SIZE];

static void test_get_binding()
{
	const struct device *flash_dev;
	flash_dev = device_get_binding(FLASH_DEVICE);
	zassert_not_null(flash_dev, "Returned device is null");

	if (strcmp(CONFIG_BOARD, "nrf9160dk_nrf9160") == 0) {
		const struct device *gpio_dev;
		gpio_dev = device_get_binding("GPIO_0");
		gpio_pin_configure(gpio_dev, 16, GPIO_OUTPUT); // LR1110 reset
		gpio_pin_configure(gpio_dev, 18, GPIO_OUTPUT); // LR1110 nss
		gpio_pin_configure(gpio_dev, 19, GPIO_OUTPUT); // LR1110 pwr en
		gpio_pin_set(gpio_dev, 16, 1);
		gpio_pin_set(gpio_dev, 18, 1);
		gpio_pin_set(gpio_dev, 19, 1);
	}
}

static void test_full_erase_full_read_write()
{
	const struct device *flash_dev;
	int i, j;
	int err;
	uint8_t data;
	struct flash_pages_info pages_info;
	size_t page_count, chip_size;

	flash_dev = device_get_binding(FLASH_DEVICE);
	zassert_not_null(flash_dev, "Returned device is null");

	page_count = flash_get_page_count(flash_dev);
	(void)flash_get_page_info_by_idx(flash_dev, 0, &pages_info);
	chip_size = page_count * pages_info.size;
	err = flash_read(flash_dev, TEST_REGION_OFFSET, &data, 1);

	/* Chip size that we get from multiplication is in bytes,
	 * but CHIP_SIZE_BITS that we get from device tree macro is in bits*/
	zassert_equal((chip_size * 8), CHIP_SIZE_BITS, "Chip sizes are not equal");
	zassert_equal(pages_info.size, ERASE_SECTOR_SIZE, "Page sizes are not equal");
	zassert_equal(err, 0, "Flash read failed");

	printk(" INFO - Starting full flash erase, this will take around 15 "
	       "seconds...\n");
	err = flash_erase(flash_dev, 0, chip_size);
	zassert_equal(err, 0, "Full flash erase failed");

	printk(" INFO - Checking that everything was erased, this will take some "
	       "time...\n");
	for (i = 0; i < page_count; i++) {
		err = flash_read(flash_dev, ERASE_BUF_SIZE * i, erase_check_buf, ERASE_BUF_SIZE);
		zassert_equal(err, 0, "Flash read failed at i: %d", i);
		for (j = 0; j < ERASE_BUF_SIZE; j++) {
			zassert_equal(erase_check_buf[j], 0xFF,
				      "ERROR at erase_check_buf[%d]: expected 0x%02X, got "
				      "0x%02X\n,\n i is: %d",
				      j, 0xFF, erase_check_buf[j], i);
		}
	}

	printk(" INFO - Writing entire flash with dummy values, this will take some "
	       "time...\n");
	for (i = 0; i < ERASE_BUF_SIZE; ++i) {
		erase_check_buf[i] = i;
	}
	for (i = 0; i < page_count; i++) {
		err = flash_write(flash_dev, ERASE_BUF_SIZE * i, erase_check_buf, ERASE_BUF_SIZE);
		zassert_equal(err, 0, "Flash write failed at i: %d", i);
	}

	/* Empty buffer just in case*/
	for (i = 0; i < ERASE_BUF_SIZE; ++i) {
		erase_check_buf[i] = 0;
	}

	printk(" INFO - Reading entire flash, this will take some time...\n");
	for (i = 0; i < page_count; i++) {
		err = flash_read(flash_dev, ERASE_BUF_SIZE * i, erase_check_buf, ERASE_BUF_SIZE);
		zassert_equal(err, 0, "Flash read failed at i: %d", i);
		for (j = 0; j < ERASE_BUF_SIZE; j++) {
			zassert_equal(erase_check_buf[j], (uint8_t)j,
				      "ERROR at erase_check_buf[%d]: expected 0x%02X, got "
				      "0x%02X\n,\n i is: %d",
				      j, j, erase_check_buf[j], i);
		}
	}
}

static void test_erase_read_write()
{
	const struct device *flash_dev;
	int i;
	int err;
	uint8_t data;
	struct flash_pages_info pages_info;
	size_t page_count, chip_size;

	flash_dev = device_get_binding(FLASH_DEVICE);
	zassert_not_null(flash_dev, "Returned device is null");

	page_count = flash_get_page_count(flash_dev);
	(void)flash_get_page_info_by_idx(flash_dev, 0, &pages_info);
	chip_size = page_count * pages_info.size;
	err = flash_read(flash_dev, TEST_REGION_OFFSET, &data, 1);

	/* Chip size that we get from multiplication is in bytes,
	 * but CHIP_SIZE_BITS that we get form device tree macro is in bits*/
	zassert_equal((chip_size * 8), CHIP_SIZE_BITS, "Chip sizes are not equal");
	zassert_equal(pages_info.size, ERASE_SECTOR_SIZE, "Page sizes are not equal");
	zassert_equal(err, 0, "Flash read failed");

	++data;
	for (i = 0; i < TEST_REGION_SIZE; ++i) {
		write_buf[i] = (uint8_t)(data + i);
	}

	err = flash_erase(flash_dev, TEST_REGION_OFFSET, TEST_REGION_SIZE);
	zassert_equal(err, 0, "Flash region erase failed");

	err = flash_read(flash_dev, TEST_REGION_OFFSET, read_buf, TEST_REGION_SIZE);
	zassert_equal(err, 0, "Flash read failed");

	for (i = 0; i < TEST_REGION_SIZE; ++i) {
		zassert_equal(read_buf[i], 0xFF,
			      "ERROR at read_buf[%d]: expected 0x%02X, got 0x%02X\n", i, 0xFF,
			      read_buf[i]);
	}

	err = flash_write(flash_dev, TEST_REGION_OFFSET, write_buf, TEST_REGION_SIZE);
	zassert_equal(err, 0, "Flash write failed");

	err = flash_write(flash_dev, TEST_REGION_OFFSET + TEST_REGION_SIZE / 2,
			  &write_buf[TEST_REGION_SIZE / 2], TEST_REGION_SIZE / 2);
	zassert_equal(err, 0, "Flash write failed");

	err = flash_read(flash_dev, TEST_REGION_OFFSET, read_buf, TEST_REGION_SIZE);
	zassert_equal(err, 0, "Flash read failed");

	for (i = 0; i < TEST_REGION_SIZE; ++i) {
		zassert_equal(read_buf[i], write_buf[i],
			      "ERROR at read_buf[%d]: expected 0x%02X, got 0x%02X\n", i,
			      write_buf[i], read_buf[i]);
	}
}

#define TEST_AREA_MAX DT_PROP(DT_NODELABEL(en25qh32b), size)
#define EXPECTED_SIZE 1024
#define CANARY	      0xff

static uint8_t __aligned(4) expected[EXPECTED_SIZE];

static void test_setup1(void)
{
	int rc;
	const struct device *flash_dev;
	struct flash_pages_info page_info;

	flash_dev = device_get_binding(FLASH_DEVICE);
	const struct flash_parameters *flash_parameters = flash_get_parameters(flash_dev);

	/* For tests purposes use page (in nrf_qspi_nor page = 64 kB) */
	flash_get_page_info_by_offs(flash_dev, TEST_REGION_OFFSET, &page_info);

	/* Check if test region is not empty */
	uint8_t buf[EXPECTED_SIZE];

	rc = flash_read(flash_dev, TEST_REGION_OFFSET, buf, EXPECTED_SIZE);
	zassert_equal(rc, 0, "Cannot read flash");

	/* Fill test buffer with random data */
	for (int i = 0; i < EXPECTED_SIZE; i++) {
		expected[i] = i;
	}

	/* Check if tested region fits in flash */
	zassert_true((TEST_REGION_OFFSET + EXPECTED_SIZE) < TEST_AREA_MAX,
		     "Test area exceeds flash size");

	/* Check if flash is cleared */
	bool is_buf_clear = true;

	for (off_t i = 0; i < EXPECTED_SIZE; i++) {
		if (buf[i] != flash_parameters->erase_value) {
			is_buf_clear = false;
			break;
		}
	}

	if (!is_buf_clear) {
		/* erase page */
		rc = flash_erase(flash_dev, page_info.start_offset, page_info.size);
		zassert_equal(rc, 0, "Flash memory not properly erased");
	}
}

static void test_read_unaligned_address(void)
{
	int rc;
	uint8_t buf[EXPECTED_SIZE];
	const struct device *flash_dev;
	struct flash_pages_info page_info;

	flash_dev = device_get_binding(FLASH_DEVICE);

	flash_get_page_info_by_offs(flash_dev, TEST_REGION_OFFSET, &page_info);

	rc = flash_write(flash_dev, page_info.start_offset, expected, EXPECTED_SIZE);
	zassert_equal(rc, 0, "Cannot write to flash");

	/* read buffer length*/
	for (off_t len = 0; len < 25; len++) {
		/* address offset */
		for (off_t ad_o = 0; ad_o < 4; ad_o++) {
			/* buffer offset; leave space for buffer guard */
			for (off_t buf_o = 1; buf_o < 5; buf_o++) {
				/* buffer overflow protection */
				buf[buf_o - 1] = CANARY;
				buf[buf_o + len] = CANARY;
				memset(buf + buf_o, 0, len);
				rc = flash_read(flash_dev, page_info.start_offset + ad_o,
						buf + buf_o, len);
				zassert_equal(rc, 0, "Cannot read flash");
				zassert_equal(memcmp(buf + buf_o, expected + ad_o, len), 0,
					      "Flash read failed at len=%d, "
					      "ad_o=%d, buf_o=%d",
					      len, ad_o, buf_o);
				/* check buffer guards */
				zassert_equal(buf[buf_o - 1], CANARY,
					      "Buffer underflow at len=%d, "
					      "ad_o=%d, buf_o=%d",
					      len, ad_o, buf_o);
				zassert_equal(buf[buf_o + len], CANARY,
					      "Buffer overflow at len=%d, "
					      "ad_o=%d, buf_o=%d",
					      len, ad_o, buf_o);
			}
		}
	}
}

static void test_low_power()
{
	const struct device *flash_dev;
	int err;
	flash_dev = device_get_binding(FLASH_DEVICE);

#if IS_ENABLED(CONFIG_PM_DEVICE)
	printk("Putting the flash device into low power state...\n");
	err = pm_device_action_run(flash_dev, PM_DEVICE_ACTION_SUSPEND);
	zassert_equal(err, 0, "Setting low power mode failed");
#endif
}

void test_main(void)
{
	ztest_test_suite(flash_driver_test, ztest_unit_test(test_get_binding),
			 ztest_unit_test(test_erase_read_write),
			 ztest_unit_test(test_full_erase_full_read_write),
			 ztest_unit_test(test_setup1), ztest_unit_test(test_read_unaligned_address),
			 ztest_unit_test(test_low_power));

	ztest_run_test_suite(flash_driver_test);
}
