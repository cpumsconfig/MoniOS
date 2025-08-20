#ifndef USB_H
#define USB_H

#include <stdint.h>

// USB 描述符
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} USBDeviceDescriptor;

// 初始化 USB
void usb_init(void);

// Mass Storage
int usb_mass_read_sector(uint8_t dev, uint32_t lba, uint8_t* buf);

// 核心接口
int usb_control_transfer(uint8_t dev, uint8_t* setup, uint32_t setup_phys,
                         uint8_t* data, uint32_t data_phys,
                         uint32_t len, int dir);

int usb_ohci_bulk_transfer(uint8_t dev, uint8_t* cmd, uint32_t cmd_phys,
                           uint8_t cmd_len, uint8_t* buf, uint32_t buf_phys,
                           uint32_t len, int dir);

#endif
