#ifndef SPOOLER_USB_LUFA_CONFIG_H
#define SPOOLER_USB_LUFA_CONFIG_H

#define USB_DEVICE_ONLY
#define ORDERED_EP_CONFIG
#define FIXED_CONTROL_ENDPOINT_SIZE 8
#define FIXED_NUM_CONFIGURATIONS 1
#define USE_FLASH_DESCRIPTORS
#define USE_STATIC_OPTIONS (USB_DEVICE_OPT_FULLSPEED | USB_OPT_REG_ENABLED | USB_OPT_MANUAL_PLL)
#define NO_INTERNAL_SERIAL
#define NO_DEVICE_SELF_POWER
#define NO_DEVICE_REMOTE_WAKEUP
#define NO_SOF_EVENTS
#define NO_CLASS_DRIVER_AUTOFLUSH
#define F_USB F_CPU

// Existing PRUSA MMU hardware identity; not an allocation for new products.
#define DEVICE_VID 0x2C99
#define DEVICE_PID 0x0004

// Control requests are polled by task(), not handled in an endpoint ISR.
#endif
