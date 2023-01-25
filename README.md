# Zepyhr SPi flash driver for EN25QH32B-104HIP2C flash chip

This repository contains driver and tests for interfacing with EN25QH32B-104HIP2C via the zephyr flash API.

This drive is compatible with the following flash chips:

- `EN25QH32B` with JEDEC id `[1c 70 16]`
- `IS25WP032D` with JEDEC id `[9D 70 16]`
- `W25Q32JVSSIQ` with JEDEC id `[EF 40 16]`
- `AT25SL321` with JEDEC id `[1F 42 16]`

## Setup

Before you get started, you'll need to install the nRF Connect SDK. Here are the full instructions:

* [Windows, Linux](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/getting_started.html)

If you already have a NCS setup you can follow these steps:

1. To get the driver we need to update `<path to ncs>/ncs/nrf/west.yml`. First in the `remotes` section add:

   ```yaml
    - name: irnas
      url-base: https://github.com/irnas
   ```

2. Then in the `projects` section add at the bottom:

    ```yaml
    - name: zephyr-spi-flash-en25-driver
      repo-path: zephyr-spi-flash-en25-driver
      path: irnas/zephyr-spi-flash-en25-driver
      remote: irnas
      revision: v3.0.0
    ```

3. Then run `west update` in your freshly created bash/command prompt session.
4. Above command will clone `zephyr-spi-flash-en25-driver` repository inside of `ncs/irnas/`. You can now run samples inside it and use its en25 driver code in your application projects.

5. Add the flash DTS entry to your board definition or overlay file. For example:

```dts

&spi0 {
   // ...

    en25qh32b: en25qh32b@0 {
        reg = <0>;
        label = "EN25QH32B";
        status = "okay";
        compatible = "irnas,en25";

        jedec-id = [ 1c 70 16  ];  // EN25
        size = <(4194304 * 8)>;

        write-sector-size = <256>;
        erase-full-block-size = <65536>;
        erase-half-block-size = <32768>;
        erase-sector-size = <4096>;

        spi-max-frequency = <4000000>;

        enter-dpd-delay = <30>;
        exit-dpd-delay = <30>;

        wp-gpios = <&gpio0 22 0>;
        hold-gpios = <&gpio0 23 0>;
    };
};

```

## External mutex

This driver can be used on multiple MCUs to use the same SPI flash peripheral. To achieve this, a GPIO line must be shared between the MCUs,
the flash chip must be the only peripheral on this SPI bus and both MCUs must be using this driver to communicate with the external flash.
To enable this feature, add `ext-mutex-gpios`, `ext-mutex-role` and `spi-clk-gpios` to the DTS flash definition.

For two MCUs, set one `ext-mutex-role` to `master` and one to `slave`. For more than 2 MCUs, set one to `master` and all others to `slave`.

For example:

```dts

&spi0 {
   // ...

    en25qh32b: en25qh32b@0 {
        // ...

        ext-mutex-gpios =  <&gpio0 24 (GPIO_ACTIVE_HIGH | GPIO_PULL_DOWN)>;
        ext-mutex-role = "slave";
        spi-clk-gpios = <&gpio0 28 0>;  // must be same as the pinctrl entry of the enclosing spi peripheral
    };
};

```
The setting `SPI_FLASH_EN25_EXTERNAL_MUTEX_TIMEOUT` can also be configured to specify the amount of time
a MCU is willing to wait for the SPI lock to be released.

## Tests

1. Navigate to `./tests/flash_read_write`
2. Build for one of the boards with supplied overlay, of make your own. Use `west build -b nrf52840dk_nrf52811` for example.
3. flash with `west flash`
