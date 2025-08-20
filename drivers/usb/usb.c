#include "usb.h"
#include <string.h>
#include <stdint.h>

void usb_init(void) {
    usb_ohci_init();
}

int usb_mass_read_sector(uint8_t dev, uint32_t lba, uint8_t* buf) {
    uint8_t scsi_cmd[10] __attribute__((aligned(16))) = {
        0x28, 0, 
        (lba >> 24) & 0xFF,
        (lba >> 16) & 0xFF,
        (lba >> 8) & 0xFF,
        lba & 0xFF,
        0, 0, 1, 0
    };

    uint32_t cmd_phys = (uint32_t)scsi_cmd;
    uint32_t buf_phys = (uint32_t)buf;

    return usb_ohci_bulk_transfer(dev, scsi_cmd, cmd_phys, 10, buf, buf_phys, 512, 1);
}
