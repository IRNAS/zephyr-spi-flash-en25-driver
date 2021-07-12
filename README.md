# Zepyhr SPi flash driver for EN25QH32B-104HIP2C flash chip

This repository contains driver and tests for interfacing with EN25QH32B-104HIP2C via the zephyr flash API.

This drive is also compatible with IS25WP032D-JBLE flash.
JEDEC id for EN25QH32B is `[1c 70 16]`
JEDEC id for IS25WP032D is `[9D 70 16]`

## Setup

Before you get started, you'll need to install the nRF Connect SDK. Here are the full instructions:

* [Windows, Linux](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/getting_started.html)

If you already have a NCS setup you can follow these steps:

1. To get the LR1110 modem library we need to update `<path to ncs>/ncs/nrf/west.yml`. First in the `remotes` section add:

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
      revision: v1.0.0
    ```

3. Then run `west update` in your freshly created bash/command prompt session.
4. Above command will clone `zephyr-spi-flash-en25-driver` repository inside of `ncs/irnas/`. You can now run samples inside it and use its en25 driver code in your application projects.

## Tests

1. Navigate to `./tests/flash_read_write`
2. Build for one of the boards with supplied overlay, of make your own. Use `west build -b nrf52840dk_nrf52811` for example.
3. flash with `west flash`
