# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [IRNAS's naming guidelines](https://github.com/IRNAS/irnas-core/blob/master/GITHUB_NAMING_GUIDELINES.md).

## [Unreleased]

## [2.0.0] - 2022-01-20

### Added

-   Support for `WP` and `HOLD` pins in driver and device tree bindings.

### Changed

-   Power managment control due to API changes in NCS 1.8. This means that this version of the driver is not compatible with older NCS versions anymore.
-   Driver does not fail at init if the wrong jedec id is read at init, but it will report the error over logging.
-   Driver will perform jedec id check at the start of flash erase and will throw -ENODEV in case of error.
-   Infinite loop in `wait_until_ready` has been replaced with the 10 second timeout.

### Extra

-   Above changes related to jedec check id should be removed when the lr1110 driver is rewritten with zephyr API or when a way to move flash driver init after the lr1110 driver init is found.


[Unreleased]: https://github.com/IRNAS/pacsana-bracelet-firmware/compare/v2.0.0...HEAD

[2.0.0]: https://github.com/IRNAS/zephyr-spi-flash-en25-driver/compare/v1.1.1.../v2.0.0
