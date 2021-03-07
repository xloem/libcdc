/*
    Copyright 2021.  This file is part of libcdc.

    libcdc is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libcdc is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with libcdc.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <stdint.h>

/** Parity mode for cdc_set_line_coding()
    These are the same as the libftdi parity types.
*/
enum cdc_parity_type { NONE=0, ODD=1, EVEN=2, MARK=3, SPACE=4 };
/** Number of stop bits for cdc_set_line_coding()
    These are the same as the libftdi stop bits.
*/
enum cdc_stopbits_type { STOP_BIT_1=0, STOP_BIT_15=1, STOP_BIT_2=2 };
/** Number of bits for cdc_set_line_coding()
    These are a superset of the libftdi bits.
*/
enum cdc_bits_type { BITS_5=5, BITS_6=6, BITS_7=7, BITS_8=8, BITS_16=16 };

/** Automatic loading / unloading of kernel modules */
enum cdc_module_detach_mode
{
    AUTO_DETACH_CDC_MODULE = 0,
    DONT_DETACH_CDC_MODULE = 1,
    AUTO_DETACH_REATTACH_CDC_MODULE = 2
};

struct cdc_ctx
{
    /** libusb */
    struct libusb_context *usb_ctx;
    struct libusb_device_handle *usb_dev;

    /** usb read timeout */
    int usb_read_timeout;
    /** usb write teimout */
    int usb_write_timeout;

    /** Device endpoint addresses */
    int out_ep;
    int in_ep;

    /** Last error */
    char const *error_str;
    int error_code;

    /** Defines behavior in case a kernel module is already attached to the device */
    enum cdc_module_detach_mode module_detach_mode;
};

/**
    Error codes.  These are the same as the libusb error codes.
    Most functions return 0 on success or one of these codes on failure.
*/
enum cdc_error {
    /** Success (no error) */
    CDC_SUCCESS = 0,

    /** Input/output error */
    CDC_ERROR_IO = -1,

    /** Invalid parameter */
    CDC_ERROR_INVALID_PARAM = -2,

    /** Access denied (insufficient permissions) */
    CDC_ERROR_ACCESS = -3,

    /** No such device (it may have been disconnected) */
    CDC_ERROR_NO_DEVICE = -4,
    
    /** Entity not found */
    CDC_ERROR_NOT_FOUND = -5,

    /** Resource busy */
    CDC_ERROR_BUSY = -6,

    /** Operation timed out */
    CDC_ERROR_TIMEOUT = -7,
    
    /** Overflow */
    CDC_ERROR_OVERFLOW = -8,

    /** Pipe error */
    CDC_ERROR_PIPE = -9,

	/** System call interrupted (perhaps due to signal) */
	CDC_ERROR_INTERRUPTED = -10,

	/** Insufficient memory */
	CDC_ERROR_NO_MEM = -11,

	/** Operation not supported or unimplemented on this platform */
	CDC_ERROR_NOT_SUPPORTED = -12,

	/** Other error */
	CDC_ERROR_OTHER = -99
};

/**
    \brief list of usb devices created by cdc_usb_find_all()
*/
struct cdc_device_list
{
    /** pointer to next entry */
    struct ftdi_device_list *next;
    /** pointer to libusb's usb_device */
    struct libusb_device *dev;
};

/**
    Provide libcdc version information
    major: Library major version
    minor: Library minor version
    micro: Could be used for hotfixes.
    version_str: Version as (static) string
    snapshot_str: Git snapshot version if known. Otherwise "unknown" or empty string.
*/
struct cdc_version_info
{
    int major;
    int minor;
    int micro;
    char const *version_str;
    char const *snapshot_str;
};

#ifdef __cplusplus
extern "C"
{
#endif

    int cdc_init(struct cdc_ctx *cdc);
    struct cdc_ctx *cdc_new(void);
    
    void cdc_deinit(struct cdc_ctx *cdc);
    void cdc_free(struct cdc_ctx *cdc);
    void cdc_set_usbdev (struct cdc_ctx *cdc, struct libusb_device_handle *usb);
    
    struct cdc_version_info cdc_get_library_version(void);
    
    int cdc_usb_open(struct cdc_ctx *cdc, int vendor, int product);
    int cdc_usb_open_dev(struct cdc_ctx *cdc, struct libusb_device *dev);
    int cdc_usb_open_desc(struct cdc_ctx *cdc, int vendor, int product,
                          char const *description, char const *serial);
    int cdc_usb_open_desc_index(struct cdc_ctx *cdc, int vendor, int product,
                                char const *description, char const *serial, unsigned int index);
    int cdc_usb_open_bus_addr(struct cdc_ctx *cdc, uint8_t bus, uint8_t addr);
    int cdc_usb_open_dev(struct cdc_ctx *cdc, struct libusb_device *dev);
    int cdc_usb_open_string(struct cdc_ctx *cdc, char const *description);
    
    int cdc_usb_close(struct cdc_ctx *cdc);
    
    int cdc_set_line_coding(struct cdc_ctx *cdc, int baudrate,
                            enum cdc_bits_type bits, enum cdc_stopbits_type sbit,
                            enum cdc_parity_type parity);
    
    int cdc_read_data(struct cdc_ctx *cdc, unsigned char *buf, int size);
    int cdc_write_data(struct cdc_ctx *cdc, unsigned char *buf, int size);
    
    int cdc_setdtr_rts(struct cdc_ctx *cdc, int dtr, int rts);
    
    char *cdc_get_error_string(struct cdc_ctx *cdc, char *buf, int size);

#ifdef __cplusplus
}
#endif
