set(spm_KERNEL_HEX_NAME zephyr.hex)
set(spm_ZEPHYR_BINARY_DIR /home/skobec/work/zephyr-spi-flash-en25-driver/build/spm/zephyr)
set(spm_KERNEL_ELF_NAME zephyr.elf)
list(APPEND spm_BUILD_BYPRODUCTS /home/skobec/work/zephyr-spi-flash-en25-driver/build/spm/zephyr/zephyr.hex)
list(APPEND spm_BUILD_BYPRODUCTS /home/skobec/work/zephyr-spi-flash-en25-driver/build/spm/zephyr/zephyr.elf)
list(APPEND spm_BUILD_BYPRODUCTS
    /home/skobec/work/zephyr-spi-flash-en25-driver/build/spm/libspmsecureentries.a)
