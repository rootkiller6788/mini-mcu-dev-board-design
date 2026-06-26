/*
 * usb_descriptors.c -- USB Descriptor Construction and Parsing
 *
 * Builds USB standard and DFU-specific descriptors for device enumeration.
 * Handles string descriptor encoding (ASCII to UTF-16LE), setup packet
 * parsing, and DFU class request identification.
 *
 * Knowledge Points:
 *   L2: USB enumeration — descriptor hierarchy construction
 *   L3: UTF-16LE encoding, BCD encoding, multi-byte field layout
 *   L5: Descriptor construction algorithms
 *   L6: DFU descriptor set for bootloader mode
 */

#include "usb_dfu_core.h"
#include "usb_descriptors.h"
#include <string.h>

/* ─── Device Descriptor Construction ─── */

void usb_build_device_descriptor(usb_device_descriptor_t *desc,
                                  uint16_t vid, uint16_t pid,
                                  uint16_t bcd_device, uint8_t ep0_size)
{
    if (!desc) return;
    memset(desc, 0, sizeof(*desc));

    desc->bLength            = sizeof(usb_device_descriptor_t);
    desc->bDescriptorType    = USB_DESC_TYPE_DEVICE;
    desc->bcdUSB             = 0x0200;  /* USB 2.0 */
    desc->bDeviceClass       = USB_CLASS_PER_INTERFACE;  /* DFU uses per-interface */
    desc->bDeviceSubClass    = 0x00;
    desc->bDeviceProtocol    = 0x00;
    desc->bMaxPacketSize0    = ep0_size;
    desc->idVendor           = vid;
    desc->idProduct           = pid;
    desc->bcdDevice          = bcd_device;
    desc->iManufacturer      = DFU_STRING_MANUFACTURER;
    desc->iProduct           = DFU_STRING_PRODUCT;
    desc->iSerialNumber      = DFU_STRING_SERIAL;
    desc->bNumConfigurations = 1;
}

/* ─── DFU Configuration Descriptor Bundle ─── */

void usb_build_dfu_config(dfu_config_bundle_t *bundle,
                          uint16_t transfer_size,
                          uint8_t attributes,
                          uint16_t detach_timeout,
                          uint8_t iface_num)
{
    if (!bundle) return;
    memset(bundle, 0, sizeof(*bundle));

    /* Configuration descriptor (9 bytes) */
    bundle->config.bLength             = sizeof(usb_configuration_descriptor_t);
    bundle->config.bDescriptorType     = USB_DESC_TYPE_CONFIGURATION;
    bundle->config.wTotalLength        = sizeof(dfu_config_bundle_t);
    bundle->config.bNumInterfaces      = 1;
    bundle->config.bConfigurationValue = 1;
    bundle->config.iConfiguration      = 0; /* No config string */
    bundle->config.bmAttributes        = USB_CONFIG_ATTR_BUS_POWERED;
    bundle->config.bMaxPower           = 50;  /* 100 mA */

    /* Interface descriptor (9 bytes) */
    bundle->interface.bLength          = sizeof(usb_interface_descriptor_t);
    bundle->interface.bDescriptorType  = USB_DESC_TYPE_INTERFACE;
    bundle->interface.bInterfaceNumber = iface_num;
    bundle->interface.bAlternateSetting = 0;
    bundle->interface.bNumEndpoints    = 0;  /* DFU uses only EP0 */
    bundle->interface.bInterfaceClass  = USB_CLASS_DFU;
    bundle->interface.bInterfaceSubClass = USB_SUBCLASS_DFU;
    bundle->interface.bInterfaceProtocol = USB_PROTOCOL_DFU_MODE;
    bundle->interface.iInterface       = DFU_STRING_DFU_MODE;

    /* DFU Functional descriptor (9 bytes) */
    bundle->dfu_func.bLength           = sizeof(dfu_functional_descriptor_t);
    bundle->dfu_func.bDescriptorType   = USB_DESC_TYPE_DFU_FUNCTIONAL;
    bundle->dfu_func.bmAttributes      = attributes;
    bundle->dfu_func.wDetachTimeOut    = detach_timeout;
    bundle->dfu_func.wTransferSize     = transfer_size;
    bundle->dfu_func.bcdDFUVersion     = 0x0110;  /* DFU 1.1 */
}

/* ─── DFU Runtime Configuration Bundle ─── */

void usb_build_dfu_runtime_config(dfu_runtime_config_bundle_t *bundle,
                                   uint16_t transfer_size)
{
    if (!bundle) return;
    memset(bundle, 0, sizeof(*bundle));

    /* Configuration descriptor */
    bundle->config.bLength             = sizeof(usb_configuration_descriptor_t);
    bundle->config.bDescriptorType     = USB_DESC_TYPE_CONFIGURATION;
    bundle->config.wTotalLength        = sizeof(dfu_runtime_config_bundle_t);
    bundle->config.bNumInterfaces      = 2;  /* App + DFU runtime */
    bundle->config.bConfigurationValue = 1;
    bundle->config.iConfiguration      = 0;
    bundle->config.bmAttributes        = USB_CONFIG_ATTR_BUS_POWERED;
    bundle->config.bMaxPower           = 50;

    /* Interface 0: Application */
    bundle->interface0.bLength          = sizeof(usb_interface_descriptor_t);
    bundle->interface0.bDescriptorType  = USB_DESC_TYPE_INTERFACE;
    bundle->interface0.bInterfaceNumber = 0;
    bundle->interface0.bAlternateSetting = 0;
    bundle->interface0.bNumEndpoints    = 0;
    bundle->interface0.bInterfaceClass  = 0xFF;  /* Vendor-specific */
    bundle->interface0.bInterfaceSubClass = 0x00;
    bundle->interface0.bInterfaceProtocol = 0x00;
    bundle->interface0.iInterface       = 0;

    /* Interface 1: DFU Runtime */
    bundle->interface1.bLength          = sizeof(usb_interface_descriptor_t);
    bundle->interface1.bDescriptorType  = USB_DESC_TYPE_INTERFACE;
    bundle->interface1.bInterfaceNumber = 1;
    bundle->interface1.bAlternateSetting = 0;
    bundle->interface1.bNumEndpoints    = 0;
    bundle->interface1.bInterfaceClass  = USB_CLASS_DFU;
    bundle->interface1.bInterfaceSubClass = USB_SUBCLASS_DFU;
    bundle->interface1.bInterfaceProtocol = USB_PROTOCOL_DFU_RUNTIME;
    bundle->interface1.iInterface       = DFU_STRING_DFU_MODE;

    /* DFU Functional descriptor */
    bundle->dfu_func.bLength           = sizeof(dfu_functional_descriptor_t);
    bundle->dfu_func.bDescriptorType   = USB_DESC_TYPE_DFU_FUNCTIONAL;
    bundle->dfu_func.bmAttributes      = DFU_ATTR_CAN_DNLOAD | DFU_ATTR_WILL_DETACH;
    bundle->dfu_func.wDetachTimeOut    = 255;
    bundle->dfu_func.wTransferSize     = transfer_size;
    bundle->dfu_func.bcdDFUVersion     = 0x0110;
}

/* ─── Configuration Total Length ─── */

uint16_t usb_config_total_length(const dfu_config_bundle_t *bundle)
{
    if (!bundle) return 0;
    return sizeof(dfu_config_bundle_t);
}

/* ─── String Descriptor Construction (§9.6.7) ─── */

uint8_t usb_build_string_descriptor(usb_string_descriptor_t *desc,
                                     const char *ascii, uint8_t max_chars)
{
    if (!desc || !ascii) return 0;

    uint8_t char_count = 0;
    const char *p = ascii;

    /* Count valid ASCII characters */
    while (*p && char_count < max_chars && char_count < 31) {
        char_count++;
        p++;
    }

    /* bLength: header (2) + UTF-16LE chars (2 bytes each) */
    desc->bLength = (uint8_t)(2 + char_count * 2);
    desc->bDescriptorType = USB_DESC_TYPE_STRING;

    /* Convert ASCII to UTF-16LE */
    p = ascii;
    for (uint8_t i = 0; i < char_count; i++) {
        desc->bString[i] = (uint16_t)(unsigned char)*p;
        p++;
    }

    return desc->bLength;
}

/* ─── Language ID Descriptor ─── */

void usb_build_langid_descriptor(usb_langid_descriptor_t *desc,
                                  uint16_t langid)
{
    if (!desc) return;

    desc->bLength = sizeof(usb_langid_descriptor_t);
    desc->bDescriptorType = USB_DESC_TYPE_STRING;
    desc->wLANGID[0] = langid;
}

/* ─── DFU Request Parsing ─── */

uint8_t usb_parse_dfu_request(const usb_setup_packet_t *setup)
{
    if (!setup) return 0xFF;

    /* Check it is a class request to an interface */
    if ((setup->bmRequestType & USB_REQ_TYPE_MASK) != USB_REQ_TYPE_CLASS)
        return 0xFF;
    if ((setup->bmRequestType & USB_REQ_RECIPIENT_MASK) != USB_REQ_RECIPIENT_INTERFACE)
        return 0xFF;

    /* bRequest is the DFU class-specific request code */
    if (setup->bRequest <= DFU_ABORT)
        return setup->bRequest;

    return 0xFF;
}

bool usb_is_dfu_class_request(const usb_setup_packet_t *setup,
                               uint8_t dfu_interface)
{
    if (!setup) return false;

    /* Must be class request to interface */
    uint8_t req_type = setup->bmRequestType & (USB_REQ_TYPE_MASK | USB_REQ_RECIPIENT_MASK);
    if (req_type != (USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE))
        return false;

    /* wIndex must match the DFU interface number */
    if ((setup->wIndex & 0xFF) != dfu_interface)
        return false;

    return true;
}

/* ─── Reset Detection ─── */

usb_reset_type_t usb_detect_reset_cause(uint32_t last_reset_flag)
{
    /* Decode reset flags from hardware register.
     * Common patterns across STM32, NXP, Silabs MCUs:
     *   Bit 26: POR/PDR reset (Power-on)
     *   Bit 25: NRST pin reset (External)
     *   Bit 19: USB reset detected
     *   Bit 17: Software reset
     */

    if (last_reset_flag & (1u << 26)) {
        return USB_RESET_POWER_ON;
    }
    if (last_reset_flag & (1u << 19)) {
        return USB_RESET_BUS;
    }
    if (last_reset_flag & (1u << 17)) {
        return USB_RESET_SOFTWARE;
    }
    if (last_reset_flag & (1u << 25)) {
        return USB_RESET_BUS;  /* Pin reset during DFU is effectively bus reset */
    }

    return USB_RESET_NONE;
}

/* ─── Transfer Size Computation ─── */

uint16_t usb_compute_transfer_size(uint8_t ep0_max_packet)
{
    /* DFU transfers are limited by:
     *   1. EP0 max packet size
     *   2. Control transfer overhead
     *   3. Flash programming page size
     *
     * Common values:
     *   EP0=8   →  64 bytes (8 x 8 bulk-style)
     *   EP0=16  → 256 bytes
     *   EP0=32  → 512 bytes
     *   EP0=64  → 1024 bytes (most common for full/high-speed DFU)
     */
    switch (ep0_max_packet) {
        case 8:   return 64;
        case 16:  return 256;
        case 32:  return 512;
        case 64:  return 1024;
        default:  return 64;
    }
}
