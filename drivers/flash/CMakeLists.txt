zephyr_include_directories(.)
zephyr_library()
zephyr_library_sources_ifdef(CONFIG_SPI_FLASH_EN25 spi_flash_en25.c)
