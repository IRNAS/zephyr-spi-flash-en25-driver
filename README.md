# TODO:


# Zepyhr SPi flash driver for EN25QH32B-104HIP2C flash chip

This repo is an example of flash driver that uses public Zephyr flash API.


## Compiling and running the main.c file

To compile and run the code run below command from the root directory of the project:

```shell
west build -b nrf9160dk_nrf9160ns && west flash
```
or
```shell
west build -b nrf52840dk_nrf52811 && west flash
```
If using nrf52811 microcontroller make sure that you commented out below two lines in file found in ncs folder:
`ncs/zephyr/boards/arm/nrf52840dk_nrf52811`.
If you don't do that the reset pin of nrf52811 will not work properly.

```
# Copyright (c) 2019 Nordic Semiconductor ASA

# SPDX-License-Identifier: Apache-2.0

# The nrf52840dk_nrf52811 board mirrors the nRF52840 DK hardware. This
# needs to be considered by certain system initialization functionality
# residing in system_nrf52811.c and SoC dependent routines in nrfx_coredep.h.
# zephyr_compile_definitions(DEVELOP_IN_NRF52840)
# zephyr_compile_definitions(NRFX_COREDEP_DELAY_US_LOOP_CYCLES=3)
```



If you connect to the serial monitor you can observe the outputs from the Ztest framework.


## Possible improvements and special notes

* You can see that in order to compile flash driver the specific Kconfig settings need to be set.
* Devicetree overlay currently uses wrong compatible label.
I did not yet found a way to include a trivial label of my own.
According to the Zephyr documentation on devicetree bindings it should be sufficient to create a `dts/bindings` folder with specific bindings yaml file.
That yaml file defines actual required properties of the device.
* Unlocking of blocking bits of the status register is not implemented.
It seems that this not needed as everything works as expected.
As long the **WP** and **HOLD** lines are kept high we should be able to communicate with the flash normally.
* We are not yet completely satisfied with the structure of the project.
Ideally we should follow general Zephyr structure with folders `drivers`, `tests` and `samples` (if needed).
That means that the current main file should actually be in `testing/flash` folder, and it should be aware of the driver.
I did not go down this route as I had problems with running the test automatically with `sanitycheck` script.
I think that `testcase.yaml` file was not setup correctly, another yaml file might also be needed.
