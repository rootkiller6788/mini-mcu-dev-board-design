/*
 * usb_descriptors.h — USB Standard & DFU-Specific Descriptors for MCU Bootloader
 *
 * Defines all USB descriptors required for a DFU-capable composite or
 * single-function device. Covers USB 2.0 Chapter 9 standard descriptors
 * plus DFU class-specific functional descriptor.
 *
 * Knowledge Coverage:
 *   L1: USB device/configuration/interface/endpoint/string descriptor structures
 *   L2: USB enumeration process, descriptor hierarchy, alternate settings
 *   L3: BCD encoding, little-endian multi-byte field layout
 *
 * Reference: USB 2.0 Specification S9.5, S9.6
 * Berkeley EE16A/B — Memory-mapped IO and USB peripheral enumeration
 * USB DFU Class Specification v1.1 S4
 */

#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include <stdint.h>
#include <stddef.h>
#include "usb_dfu_core.h"

/* USB 2.0 Standard Descriptor Types (S9.4, Table 9-5) */
#define USB_DESC_TYPE_DEVICE                    0x01
#define USB_DESC_TYPE_CONFIGURATION             0x02
#define USB_DESC_TYPE_STRING                    0x03
#define USB_DESC_TYPE_INTERFACE                 0x04
#define USB_DESC_TYPE_ENDPOINT                  0x05
#define USB_DESC_TYPE_DEVICE_QUALIFIER          0x06
#define USB_DESC_TYPE_OTHER_SPEED_CONFIG        0x07
#define USB_DESC_TYPE_INTERFACE_POWER           0x08
#define USB_DESC_TYPE_DFU_FUNCTIONAL            0x21

/* USB Class Codes */
#define USB_CLASS_PER_INTERFACE      0x00
#define USB_CLASS_DFU                0xFE
#define USB_SUBCLASS_DFU             0x01

#define USB_PROTOCOL_DFU_RUNTIME     0x01
#define USB_PROTOCOL_DFU_MODE        0x02

/* USB Standard Device Descriptor (S9.6.1, Table 9-8) */
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
} __attribute__((packed)) usb_device_descriptor_t;

/* USB Configuration Descriptor (S9.6.3, Table 9-10) */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) usb_configuration_descriptor_t;

#define USB_CONFIG_ATTR_REMOTE_WAKEUP  (1u << 5)
#define USB_CONFIG_ATTR_SELF_POWERED   (1u << 6)
#define USB_CONFIG_ATTR_BUS_POWERED    0x80

/* USB Interface Descriptor (S9.6.5, Table 9-12) */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

/* USB Endpoint Descriptor (S9.6.6, Table 9-13) */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

#define USB_EP_DIR_OUT     0x00
#define USB_EP_DIR_IN      0x80
#define USB_EP_ADDR_MASK   0x0F
#define USB_EP_NUM(addr)   ((addr) & USB_EP_ADDR_MASK)
#define USB_EP_DIR(addr)   ((addr) & USB_EP_DIR_IN)

#define USB_EP_TYPE_CONTROL     0x00
#define USB_EP_TYPE_ISOCHRONOUS 0x01
#define USB_EP_TYPE_BULK        0x02
#define USB_EP_TYPE_INTERRUPT   0x03

/* USB String Descriptor (S9.6.7) */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bString[31];
} __attribute__((packed)) usb_string_descriptor_t;

/* Language ID string descriptor */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wLANGID[1];
} __attribute__((packed)) usb_langid_descriptor_t;

#define DFU_CONFIG_DESC_MAX_SIZE  256

typedef struct {
    usb_configuration_descriptor_t config;
    usb_interface_descriptor_t     interface;
    dfu_functional_descriptor_t    dfu_func;
} dfu_config_bundle_t;

typedef struct {
    usb_configuration_descriptor_t config;
    usb_interface_descriptor_t     interface0;
    usb_interface_descriptor_t     interface1;
    dfu_functional_descriptor_t    dfu_func;
} dfu_runtime_config_bundle_t;

/* USB Device Qualifier Descriptor (S9.6.4) */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint8_t  bNumConfigurations;
    uint8_t  bReserved;
} __attribute__((packed)) usb_device_qualifier_descriptor_t;

/* USB Setup Packet (S9.3) */
typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_setup_packet_t;

#define USB_REQ_DIR_MASK         0x80
#define USB_REQ_DIR_HOST_TO_DEV  0x00
#define USB_REQ_DIR_DEV_TO_HOST  0x80
#define USB_REQ_TYPE_MASK        0x60
#define USB_REQ_TYPE_STANDARD    0x00
#define USB_REQ_TYPE_CLASS       0x20
#define USB_REQ_TYPE_VENDOR      0x40
#define USB_REQ_RECIPIENT_MASK   0x1F
#define USB_REQ_RECIPIENT_DEVICE    0x00
#define USB_REQ_RECIPIENT_INTERFACE 0x01
#define USB_REQ_RECIPIENT_ENDPOINT  0x02

#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE     0x0A
#define USB_REQ_SET_INTERFACE     0x0B
#define USB_REQ_SYNCH_FRAME       0x0C

/* Reset types for DFU re-enumeration detection */
typedef enum {
    USB_RESET_NONE       = 0,
    USB_RESET_BUS        = 1,
    USB_RESET_POWER_ON   = 2,
    USB_RESET_SOFTWARE   = 3
} usb_reset_type_t;

/* API Declarations */
void usb_build_device_descriptor(usb_device_descriptor_t *desc,
                                  uint16_t vid, uint16_t pid,
                                  uint16_t bcd_device, uint8_t ep0_size);

void usb_build_dfu_config(dfu_config_bundle_t *bundle,
                          uint16_t transfer_size,
                          uint8_t attributes,
                          uint16_t detach_timeout,
                          uint8_t iface_num);

void usb_build_dfu_runtime_config(dfu_runtime_config_bundle_t *bundle,
                                   uint16_t transfer_size);

uint16_t usb_config_total_length(const dfu_config_bundle_t *bundle);

uint8_t usb_build_string_descriptor(usb_string_descriptor_t *desc,
                                     const char *ascii, uint8_t max_chars);

void usb_build_langid_descriptor(usb_langid_descriptor_t *desc,
                                  uint16_t langid);

uint8_t usb_parse_dfu_request(const usb_setup_packet_t *setup);

bool usb_is_dfu_class_request(const usb_setup_packet_t *setup,
                               uint8_t dfu_interface);

usb_reset_type_t usb_detect_reset_cause(uint32_t last_reset_flag);

uint16_t usb_compute_transfer_size(uint8_t ep0_max_packet);

#endif /* USB_DESCRIPTORS_H */