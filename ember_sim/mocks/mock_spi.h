#ifndef MOCK_SPI_H
#define MOCK_SPI_H

#include <stdint.h>

typedef struct VirtualSPIDevice VirtualSPIDevice;

typedef void (*SPI_OnSelect)(VirtualSPIDevice *dev);
typedef void (*SPI_OnDeselect)(VirtualSPIDevice *dev);
typedef uint8_t (*SPI_OnByte)(VirtualSPIDevice *dev, uint8_t tx_byte);

struct VirtualSPIDevice {
    const char *name;
    void *user_data;
    SPI_OnSelect   on_select;
    SPI_OnDeselect on_deselect;
    SPI_OnByte     on_byte;
};

void mock_spi_add_device(uintptr_t spi_base, VirtualSPIDevice *dev);
void mock_spi_load_device_yaml(uintptr_t spi_base, const char *yaml_path);

#endif