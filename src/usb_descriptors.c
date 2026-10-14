#include "tusb.h"
#include "ghost_runtime.h"
#include "usb_descriptors.h"

#include <string.h>

_Static_assert(CFG_TUD_HID_EP_BUFSIZE >= GHOST_CONFIG_FEATURE_SIZE + 1,
               "TinyUSB HID buffer must fit the configuration payload and report ID");

#define USB_VID 0x046d
#define USB_PID 0xc31c
#define USB_BCD 0x0100

#ifndef GHOST_USB_SERIAL
#define GHOST_USB_SERIAL "fedcba9876543210"
#endif

enum {
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

enum {
    EPNUM_HID = 0x81
};

uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),

    0x06, 0x00, 0xff,                         // Usage Page (Vendor Defined)
    0x09, 0x01,                               // Usage (1)
    0xa1, 0x01,                               // Collection (Application)
    0x85, REPORT_ID_CONFIG,                   //   Report ID
    0x09, 0x02,                               //   Usage (2)
    0x15, 0x00,                               //   Logical Minimum (0)
    0x26, 0xff, 0x00,                         //   Logical Maximum (255)
    0x75, 0x08,                               //   Report Size (8)
    0x95, GHOST_CONFIG_FEATURE_SIZE,          //   Report Count
    0xb1, 0x02,                               //   Feature (Data, Variable, Absolute)
    0xc0                                      // End Collection
};

tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = USB_BCD,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 10),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "Logitech",
    "USB Keyboard/Mouse Combo",
    GHOST_USB_SERIAL,
};

static uint16_t string_desc[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    uint8_t chr_count;
    if (index == 0) {
        memcpy(&string_desc[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }

        const char *str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }

        for (uint8_t i = 0; i < chr_count; i++) {
            string_desc[1 + i] = str[i];
        }
    }

    string_desc[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return string_desc;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
    (void)instance;

    if (report_id == REPORT_ID_CONFIG && report_type == HID_REPORT_TYPE_FEATURE) {
        ghost_handle_feature_report(buffer, bufsize);
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
    (void)instance;

    if (report_id == REPORT_ID_CONFIG && report_type == HID_REPORT_TYPE_FEATURE) {
        return ghost_fill_feature_report(buffer, reqlen);
    }

    return 0;
}
