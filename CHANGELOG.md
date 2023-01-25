# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [IRNAS's naming guidelines](https://github.com/IRNAS/irnas-core/blob/master/GITHUB_NAMING_GUIDELINES.md).

## [Unreleased]

### Added

- SPI external mutex functionality.

## [3.0.0] - 2022-09-26

### Changed

-   Rework how sector sizes are specified in DTS.

## [2.1.0] - 2022-07-07

### Changed

-   Remove unnecessary board overlays form tests.
-   Migrate code to run on NCS 2.0

## [2.0.5] - 2022-05-03

### Added

-   Added Kconfig option `CONFIG_SPI_FLASH_EN25_JEDEC_CHECK_AT_INIT` with default `n`. If set to `y`, the jedec id check will be performed during driver init and will not init the glash device if it fails.

### Fixed

-   Fix errors in the test suite.
-   Fix error handling in `relese_ext_mutex`.

## [2.0.4] - 2022-04-19

### Fixed

-   Remove double call to acquire in `spi_flash_en25_pm_control`.

## [2.0.3] - 2022-03-23

### Fixed

-   Replace deprecated `CONFIG_DEVICE_POWER_MANAGEMENT` with `CONFIG_PM_DEVICE`

## [2.0.2] - 2022-02-17

### Fixed

-   Fixed spi_flash_en25_pm_control not behaving correctly and not returning any value at the end of the function.

## [2.0.1] - 2022-02-17

### Added

-   Configurable wait for ready timeout via the `CONFIG_SPI_FLASH_EN25_READY_TIMEOUT` KConfig option.

### Changed

-   Removed jedec id check in erase function.

## [2.0.0] - 2022-01-20

### Added

-   Support for `WP` and `HOLD` pins in driver and device tree bindings.

### Changed

-   Power management control due to API changes in NCS 1.8. This means that this version of the driver is not compatible with older NCS versions anymore.
-   Driver does not fail at init if the wrong jedec id is read at init, but it will report the error over logging.
-   Driver will perform jedec id check at the start of flash erase and will throw -ENODEV in case of error.
-   Infinite loop in `wait_until_ready` has been replaced with the 10 second timeout.

### Extra

-   Above changes related to jedec check id should be removed when the lr1110 driver is rewritten with zephyr API or when a way to move flash driver init after the lr1110 driver init is found.

[Unreleased]: https://github.com/IRNAS/zephyr-spi-flash-en25-driver/compare/v3.0.0...HEAD

[3.0.0]: https://github.com/IRNAS/zephyr-spi-flash-en25-driver/compare//v2.1.0...v3.0.0

[2.1.0]: https://github.com/IRNAS/zephyr-spi-flash-en25-driver/compare/v2.0.5.../v2.1.0

[2.0.5]: https://github.com/IRNAS/zephyr-spi-flash-en25-driver/compare/v2.0.4.../v2.0.5

[2.0.4]: https://github.com/IRNAS/zephyr-spi-flash-en25-driver/compare/v2.0.3.../v2.0.4

[2.0.3]: https://github.com/IRNAS/zephyr-spi-flash-en25-driver/compare/v2.0.2.../v2.0.3

[2.0.2]: https://github.com/IRNAS/zephyr-spi-flash-en25-driver/compare/v2.0.1.../v2.0.2

[2.0.1]: https://github.com/IRNAS/zephyr-spi-flash-en25-driver/compare/v2.0.0.../v2.0.1

[2.0.0]: https://github.com/IRNAS/zephyr-spi-flash-en25-driver/compare/v1.1.1.../v2.0.0
