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
/** \addtogroup libcdc */
/* @{ */

#include <errno.h>
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdc.h"
#include "cdc_version_i.h"

#define cdc_return(code, str, ...) \
    if (cdc) {                     \
        if (str) {                 \
            cdc->error_str = (str);\
        }                          \
        cdc->error_code = (code);  \
        __VA_ARGS__;               \
        return cdc->error_code;    \
    } else {                       \
        return (code);             \
    }

#define cdc_check(code, str, ...)    \
    do {                             \
        int __code = (code);         \
        if (__code < 0) {            \
            cdc_return(__code, str,  \
                       __VA_ARGS__); \
        }                            \
    } while(0);

/**
    Internal function to determine interfaces and endpoint addresses.
    \internal

    \param cdc pointer to cdc_ctx
    \param dev usb device to probe
    \param out_ep pointer to storage for out endpoint
    \param in_ep pointer to storage for in endpoint
    \param ctrl_iface pointer to storage for ctrl interface index
    \param data_iface pointer to storage for data interface index
    \param cfg_idx pointer to storage for configuration index

    \return CDC_SUCCESS on success or CDC_ERROR code on failure
*/
static int cdc_usb_endpoints_internal (struct cdc_ctx *cdc, libusb_device *dev, int *out_ep, int *in_ep, int *ctrl_iface, int *data_iface, int *cfg_idx)
{
    struct libusb_device_descriptor desc;
    cdc_check(dev ? CDC_SUCCESS : CDC_ERROR_INVALID_PARAM, "libusb_device *dev");

    cdc_check(libusb_get_device_descriptor(dev, &desc), "libusb_get_device_descriptor");

    for (int c = 0; c < desc.bNumConfigurations; c ++) {
        struct libusb_config_descriptor *config;
        cdc_check(libusb_get_config_descriptor(dev, c, &config), "libusb_get_config_descriptor");
        for (int i = 0; i < config->bNumInterfaces; i ++) {
            struct libusb_interface const *interface = &config->interface[i];
            for (int a = 0; a < interface->num_altsetting; a ++) {
                struct libusb_interface_descriptor const *setting = &interface->altsetting[a];
                if (setting->bInterfaceClass == 10 && setting->bNumEndpoints) {
                    /* CDC Data interface */
                    if (data_iface) {
                        *data_iface = i;
                    }
                    if (ctrl_iface) {
                        *ctrl_iface = i ^ 1;
                    }
                    if (cfg_idx) {
                        *cfg_idx = c;
                    }
                    if (in_ep) {
                        *in_ep = setting->endpoint[0].bEndpointAddress;
                    }
                    if (out_ep) {
                        *out_ep = setting->endpoint[0].bEndpointAddress;
                    }
                    if (setting->bNumEndpoints > 1) {
                        if (setting->endpoint[1].bEndpointAddress & 0x80) {
                            if (out_ep) {
                                *out_ep = setting->endpoint[1].bEndpointAddress;
                            }
                        } else {
                            if (in_ep) {
                                *in_ep = setting->endpoint[1].bEndpointAddress;
                            }
                        }
                    }
                    libusb_free_config_descriptor(config);
                    return CDC_SUCCESS;
                }
            }
        }
        libusb_free_config_descriptor(config);
    }
    cdc_return(CDC_ERROR_NOT_FOUND, "cdc endpoints");
}

/**
    Internal function to close usb device pointer.
    Sets cdc->usb_dev to NULL.
    \internal

    \param cdc pointer to cdc_ctx

    \retval none
*/
static void cdc_usb_close_internal (struct cdc_ctx *cdc)
{
    if (cdc && cdc->usb_dev)
    {
        libusb_close (cdc->usb_dev);
        cdc->usb_dev = NULL;
    }
}

/**
    Initialises a cdc_ctx.

    \param cdc pointer to cdc_ctx

    \return CDC_SUCCESS on success or CDC_ERROR code on failure

    \remark This should be called before all other functions
*/
int cdc_init(struct cdc_ctx *cdc)
{
    cdc->usb_ctx = NULL;
    cdc->usb_dev = NULL;
    cdc->usb_read_timeout = 5000;
    cdc->usb_write_timeout = 5000;
    cdc->error_str = "cdc_init";
    cdc->module_detach_mode = AUTO_DETACH_CDC_MODULE;

    cdc_check(libusb_init(&cdc->usb_ctx), "libusb_init");

    return CDC_SUCCESS;
}

/**
    Allocate and initialise a new cdc_ctx

    \return a pointer to a new cdc_ctx, or NULL on failure
*/
struct cdc_ctx *cdc_new(void)
{
    struct cdc_ctx *cdc = (struct cdc_ctx *)malloc(sizeof(struct cdc_ctx));
    if (cdc == NULL) {
        return NULL;
    }

    if (cdc_init(cdc) != CDC_SUCCESS) {
        free(cdc);
        return NULL;
    }

    return cdc;
}

/**
    Deinitialises a cdc_ctx.

    \param cdc pointer to cdc_ctx
*/
void cdc_deinit(struct cdc_ctx *cdc)
{
    if (cdc == NULL) {
        return;
    }

    cdc_usb_close_internal(cdc);

    if (cdc->usb_ctx)
    {
        libusb_exit(cdc->usb_ctx);
        cdc->usb_ctx = NULL;
    }
}

/**
    Deinitialise and free a cdc_ctx.

    \param cdc pointer to cdc_ctx
*/
void cdc_free(struct cdc_ctx *cdc)
{
    cdc_deinit(cdc);
    free(cdc);
}

/**
    Use an already open libusb device.

    \param cdc pointer to cdc_ctx
    \param usb libusb libusb_device_handle to use
*/
void cdc_set_usbdev (struct cdc_ctx *cdc, libusb_device_handle *usb)
{
    if (cdc == NULL) {
        return;
    }

    cdc->usb_dev = usb;
}

/**
    Get libcdc library version
    
    \return cdc_version_info Library version information
*/
struct cdc_version_info cdc_get_library_version(void)
{
    struct cdc_version_info ver;

    ver.major = CDC_MAJOR_VERSION;
    ver.minor = CDC_MINOR_VERSION;
    ver.micro = CDC_MICRO_VERSION;
    ver.version_str = CDC_VERSION_STRING;
    ver.snapshot_str = CDC_SNAPSHOT_VERSION;

    return ver;
}

/**
    Finds all CDC devices with given VID:PID on the usb bus.  Creates a new
    cdc_device_list which needs to be deallocated by cdc_list_free() after
    use.  With VID:PID 0:0, search for all CDC devices.

    \param cdc pointer to cdc_ctx
    \param devlist Pointer where to store list of found devices
    \param vendor Vendor ID to search for
    \param product Product ID to search for

    \return CDC_SUCCESS on success or CDC_ERROR code on failure
*/
int cdc_usb_find_all(struct cdc_ctx *cdc, struct cdc_device_list **devlist, int vendor, int product)
{
    struct cdc_device_list **curdev;
    libusb_device *dev;
    libusb_device **devs;
    int count = 0;
    int i = 0;

    cdc_check(libusb_get_device_list(cdc->usb_ctx, &devs), "libusb_get_device_list");

    curdev = devlist;
    *curdev = NULL;

    while ((dev = devs[i++]) != NULL) {
        if (!vendor && !product) {
            int result = cdc_usb_endpoints_internal(cdc, dev, NULL, NULL, NULL, NULL, NULL);
            if (result == CDC_ERROR_NOT_FOUND) {
                continue;
            }
            cdc_check(result, NULL, libusb_free_device_list(devs,1));
        } else {
            struct libusb_device_descriptor desc;
            cdc_check(
                libusb_get_device_descriptor(dev, &desc),
                "libusb_get_device_descriptor",
                libusb_free_device_list(devs,1));
            if (desc.idVendor != vendor || desc.idProduct != product) {
                continue;
            }
        }

        *curdev = (struct cdc_device_list*)malloc(sizeof(struct cdc_device_list));
        if (!*curdev) {
            cdc_return(CDC_ERROR_NO_MEM, "out of memory", libusb_free_device_list(devs,1));
        }
        (*curdev)->next = NULL;
        (*curdev)->dev = dev;
        libusb_ref_device(dev);
        curdev = &(*curdev)->next;
        count ++;
    }
    libusb_free_device_list(devs,1);
    return count;
}

/**
    Frees a usb device list.

    \param devlist USB device list created by cdc_usb_find_all()
*/
void cdc_list_free(struct cdc_device_list **devlist)
{
    struct cdc_device_list *curdev, *next;

    for (curdev = *devlist; curdev != NULL;)
    {
        next = curdev->next;
        libusb_unref_device(curdev->dev);
        free(curdev);
        curdev = next;
    }

    *devlist = NULL;
}

/**
    Return device ID strings from the usb device.

    The parameters manufacturer, description and serial may be NULL
    or pointer to buffers to store the fetched strings.

    \note This version only closes the device if it was opened by it.

    \param cdc pointer to cdc_ctx
    \param dev libusb usb_dev to use
    \param manufacturer Store manufacturer string here if not NULL
    \param mnf_len Buffer size of manufacturer string
    \param description Store product description string here if not NULL
    \param desc_len Buffer size of product description string
    \param serial Store serial string here if not NULL
    \param serial_len Buffer size of serial string

    \retval   0: all fine
    \retval  -1: wrong arguments
    \retval  -4: unable to open device
    \retval  -7: get product manufacturer failed
    \retval  -8: get product description failed
    \retval  -9: get serial number failed
    \retval -11: libusb_get_device_descriptor() failed
*/
int cdc_usb_get_strings(struct cdc_ctx *cdc, struct libusb_device *dev,
                          char *manufacturer, int mnf_len,
                          char *description, int desc_len,
                          char *serial, int serial_len)
{
    struct libusb_device_descriptor desc;
    char need_open;

    cdc_check(cdc ? CDC_SUCCESS : CDC_ERROR_INVALID_PARAM, "struct cdc_ctx *cdc");
    cdc_check(dev ? CDC_SUCCESS : CDC_ERROR_INVALID_PARAM, "struct libusb_device *dev");

    need_open = (cdc->usb_dev == NULL);
    if (need_open) {
        cdc_check(libusb_open(dev, &cdc->usb_dev), "libusb_open");
    }

    cdc_check(libusb_get_device_descriptor(dev, &desc), "libusb_get_device_descriptor");

    if (manufacturer != NULL)
    {
        cdc_check(
            libusb_get_string_descriptor_ascii(cdc->usb_dev, desc.iManufacturer, (unsigned char *)manufacturer, mnf_len),
            "libusb_get_string_descriptor_ascii manufacturer",
            if (need_open) { cdc_usb_close_internal(cdc); }
        );
    }

    if (description != NULL)
    {
        cdc_check(
            libusb_get_string_descriptor_ascii(cdc->usb_dev, desc.iProduct, (unsigned char *)description, desc_len),
            "libusb_get_string_descriptor_ascii product",
            if (need_open) { cdc_usb_close_internal(cdc); }
        );
    }

    if (serial != NULL)
    {
        cdc_check(
            libusb_get_string_descriptor_ascii(cdc->usb_dev, desc.iSerialNumber, (unsigned char *)serial, serial_len),
            "libusb_get_string_descriptor_ascii serial",
            if (need_open) { cdc_usb_close_internal(cdc); }
        );
    }

    if (need_open) {
        cdc_usb_close_internal(cdc);
    }

    return 0;
}

/**
    Opens a cdc device given by a usb_device.

    \param cdc pointer to cdc_ctx
    \param dev libusb usb_dev to use

    \return CDC_SUCCESS on success or CDC_ERROR code on failure
*/
int cdc_usb_open_dev(struct cdc_ctx *cdc, libusb_device *dev)
{
    int ctrl_iface, data_iface, cfg_idx, result, detach_errno = 0;

    cdc_check(cdc ? CDC_SUCCESS : CDC_ERROR_INVALID_PARAM, "struct cdc_ctx *cdc");
    cdc_check(cdc_usb_endpoints_internal(cdc, dev, &cdc->out_ep, &cdc->in_ep, &ctrl_iface, &data_iface, &cfg_idx), NULL);
    cdc_check(libusb_open(dev, &cdc->usb_dev), "libusb_open");

    /* Try to detach cdc-acm module.
       The CDC-ACM Class defines two interfaces:
       the Control interface and the Data interface.
       Note this might harmlessly fail.
     */
    if (cdc->module_detach_mode == AUTO_DETACH_CDC_MODULE) {
        libusb_detach_kernel_driver(cdc->usb_dev, ctrl_iface);
        libusb_detach_kernel_driver(cdc->usb_dev, data_iface);
        detach_errno = errno;
    } else if (cdc->module_detach_mode == AUTO_DETACH_REATTACH_CDC_MODULE) {
        libusb_set_auto_detach_kernel_driver(cdc->usb_dev, ctrl_iface);
        libusb_set_auto_detach_kernel_driver(cdc->usb_dev, data_iface);
        detach_errno = errno;
    }

    /* set configuration */
    result = libusb_set_configuration(cdc->usb_dev, cfg_idx);
    if (result < 0) {
        if (detach_errno == EPERM) {
            cdc_return(CDC_ERROR_ACCESS, "missing permissions to detach kernel module");
        } else {
            cdc_return(result, "libusb_set_configuration");
        }
    }

    cdc_check(
        libusb_claim_interface(cdc->usb_dev, data_iface),
        "libusb_claim_interface",
        cdc_usb_close_internal(cdc)
    );

    /* Set line state to 9600 8N1 */
    cdc_check(
        cdc_set_line_coding(cdc, 9600, BITS_8, STOP_BIT_1, NONE),
        NULL,
        cdc_usb_close_internal(cdc)
    );

    return CDC_SUCCESS;
}

/**
    Opens the first device with given vendor and product ids.

    \param cdc pointer to cdc_ctx
    \param vendor Vendor ID
    \param product Product ID

    \return CDC_SUCCESS on success or CDC_ERROR code on failure
*/
int cdc_usb_open(struct cdc_ctx *cdc, int vendor, int product)
{
    return cdc_usb_open_desc(cdc, vendor, product, NULL, NULL);
}

/**
    Opens the first device with a given vendor id, product id,
    description and serial.

    \param cdc pointer to cdc_ctx
    \param vendor Vendor ID
    \param product Product ID
    \param description Description to search for.  Use NULL if not needed.
    \param serial Serial to search for.  Use NULL if not needed.

    \return CDC_SUCCESS on success or CDC_ERROR code on failure
*/
int cdc_usb_open_desc(struct cdc_ctx *cdc, int vendor, int product,
                      char const *description, char const *serial)
{
    return cdc_usb_open_desc_index(cdc,vendor,product,description,serial,0);
}

/**
    Opens the index-th device with a given, vendor id, product id,
    description and serial.

    \param cdc pointer to cdc_ctx
    \param vendor Vendor ID
    \param product Product ID
    \param description Description to search for.  Use NULL if not needed.
    \param serial Serial to search for.  Use NULL if not needed.
    \param index Number of matching device to open if there are more than one, starts with 0.

    \return CDC_SUCCESS on success or CDC_ERROR code on failure
*/
int cdc_usb_open_desc_index(struct cdc_ctx *cdc, int vendor, int product,
                            char const *description, char const *serial, unsigned int index)
{
    libusb_device *dev;
    libusb_device **devs;
    char string[256];
    int i = 0;

    cdc_check(cdc ? CDC_SUCCESS : CDC_ERROR_INVALID_PARAM, "struct cdc_ctx *cdc");

    cdc_check(libusb_get_device_list(cdc->usb_ctx, &devs), "libusb_get_device_list");

    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        int res;

        cdc_check(
            libusb_get_device_descriptor(dev, &desc),
            "libusb_get_device_descriptor", 
            libusb_free_device_list(devs,1)
        );

        if (desc.idVendor == vendor && desc.idProduct == product)
        {
            cdc_check(
                libusb_open(dev, &cdc->usb_dev),
                "usb_open", 
                libusb_free_device_list(devs,1)
            );

            if (description != NULL) {
                cdc_check(
                    libusb_get_string_descriptor_ascii(cdc->usb_dev, desc.iProduct, (unsigned char *)string, sizeof(string)),
                    "libusb_get_string_descriptor_ascii product description",
                    libusb_free_device_list(devs,1)
                );

                if (strncmp(string, description, sizeof(string)) != 0) {
                    cdc_usb_close_internal(cdc);
                    continue;
                }                                             

            }
            if(serial != NULL) {
                cdc_check(
                    libusb_get_string_descriptor_ascii(cdc->usb_dev, desc.iSerialNumber, (unsigned char *)string, sizeof(string)),
                    "libusb_get_string_descriptor_ascii serial number",
                    libusb_free_device_list(devs,1)
                );
                if (strncmp(string, serial, sizeof(string)) != 0)
                {
                    cdc_usb_close_internal(cdc);
                    continue;
                }
            }

            cdc_usb_close_internal(cdc);

            if (index > 0) {
                index --;
                continue;
            }

            res = cdc_usb_open_dev(cdc, dev);
            libusb_free_device_list(devs,1);
            return res;
        }
    }

    /* device not found */
    cdc_return(CDC_ERROR_NOT_FOUND, "device not found");
}

/**
    Opens the device at a given USB bus and device address.

    \param cdc pointer to cdc_ctx
    \param bus Bus number
    \param addr Device address

    \return CDC_SUCCESS on success or CDC_ERROR code on failure
*/
int cdc_usb_open_bus_addr(struct cdc_ctx *cdc, uint8_t bus, uint8_t addr)
{
    libusb_device *dev;
    libusb_device **devs;
    
    int i = 0;

    cdc_check(cdc ? CDC_SUCCESS : CDC_ERROR_INVALID_PARAM, "struct cdc_ctx *cdc");

    cdc_check(libusb_get_device_list(cdc->usb_ctx, &devs), "libusb_get_device_list");

    while ((dev = devs[i++]) != NULL) {
        if (libusb_get_bus_number(dev) == bus && libusb_get_device_address(dev) == addr) {
            int res;
            res = cdc_usb_open_dev(cdc, dev);
            libusb_free_device_list(devs,1);
            return res;
        }
    }

    /* device not found */
    cdc_return(CDC_ERROR_NOT_FOUND, "device not found");
}

/**
    Closes the cdc device.  Call cdc_deinit() if you're cleaning up.

    \param cdc pointer to cdc_ctx

    \return CDC_SUCCESS on success or CDC_ERROR code on failure
*/
int cdc_usb_close(struct cdc_ctx *cdc)
{
    int result = 0;
    
    cdc_check(cdc ? CDC_SUCCESS: CDC_ERROR_INVALID_PARAM, "struct cdc_ctx *cdc");

    for (int if_num = 0; if_num < 2; if_num ++) {
        result = libusb_release_interface(cdc->usb_dev, if_num);
        if (result != CDC_SUCCESS) {
            break;
        }
    }

    cdc_usb_close_internal (cdc);

    cdc_check(result, "libusb_release_interface");
    return CDC_SUCCESS;
}

/**
    Set (RS232) line characteristics and baud rate.

    \param cdc pointer to cdc_ctx
    \param baudrate baud rate to set
    \param bits Number of bits
    \param sbit Number of stop bits
    \param parity Parity mode

    \return CDC_SUCCESS on success or CDC_ERROR code on failure
*/
int cdc_set_line_coding(struct cdc_ctx *cdc, int baudrate,
                        enum cdc_bits_type bits, enum cdc_stopbits_type sbit,
                        enum cdc_parity_type parity)
{
    cdc_check(cdc ? CDC_SUCCESS: CDC_ERROR_INVALID_PARAM, "struct cdc_ctx *cdc");
    cdc_check(cdc->usb_dev ? CDC_SUCCESS: CDC_ERROR_NO_DEVICE, "not opened");

    uint8_t coding[7] = {
        baudrate & 0xff,
        (baudrate >> 8) & 0xff,
        (baudrate >> 16) & 0xff,
        (baudrate >> 24) & 0xff,
        sbit,
        parity,   
        bits
    };
    cdc_check(
        libusb_control_transfer(cdc->usb_dev, 0x21, 0x20, 0, 0, coding, sizeof(coding), 0),
        "libusb_control_transfer"
    );
    
    return CDC_SUCCESS;
}

/**
    Writes data

    \param cdc pointer to cdc_ctx
    \param buf Buffer with the data
    \param size Size of the buffer

    \retval <0: CDC_ERROR code
    \retval >=0: number of bytes written
*/
int cdc_write_data(struct cdc_ctx *cdc, unsigned char *buf, int size)
{
    int actual_size = 0;
    int result = libusb_bulk_transfer(cdc->usb_dev, cdc->in_ep, buf, size, &actual_size, cdc->usb_write_timeout);

    if (result == LIBUSB_ERROR_TIMEOUT && actual_size != 0) {
        result = LIBUSB_SUCCESS;
    }
    cdc_check(
        result,
        "libusb_bulk_transfer"
    );
    return actual_size;
}

/**
    Reads data

    \param cdc pointer to cdc_ctx
    \param buf Buffer to fill
    \param size Size of the buffer

    \retval <0: CDC_ERROR code
    \retval >=0: number of bytes read
*/
int cdc_read_data(struct cdc_ctx *cdc, unsigned char *buf, int size)
{
    int actual_size = 0;
    int result = libusb_bulk_transfer(cdc->usb_dev, cdc->out_ep, buf, size, &actual_size, cdc->usb_read_timeout);
    if (result == LIBUSB_ERROR_TIMEOUT && actual_size != 0) {
        result = LIBUSB_SUCCESS;
    }
    cdc_check(
        result,
        "libusb_bulk_transfer"
    );
    return actual_size;
}

/**
    Set dtr and rts line

    \param cdc pointer to cdc_context
    \param dtr DTR state to set line to (1 or 0)
    \param rts RTS state to set line to (1 or 0)

    \return CDC_SUCCESS on success or CDC_ERROR code on failure
*/
int cdc_setdtr_rts(struct cdc_ctx *cdc, int dtr, int rts)
{
    unsigned short usb_val;

    cdc_check(cdc ? CDC_SUCCESS: CDC_ERROR_INVALID_PARAM, "struct cdc_ctx *cdc");
    cdc_check(cdc->usb_dev ? CDC_SUCCESS: CDC_ERROR_NO_DEVICE, "not opened");

    usb_val = 0;
    if (dtr) {
        usb_val |= 0x01;
    }
    if (rts) {
        usb_val |= 0x02;
    }
    cdc_check(
        libusb_control_transfer(cdc->usb_dev, 0x21, 0x22, usb_val, 0, NULL, 0, 0),
        "libusb_control_transfer"
    );
    
    return CDC_SUCCESS;
}

/**
    Produce a string representation for the last error code

    \param cdc pointer to cdc_context
    \param buf string storage
    \param size length of storage

    \return The passed storage buffer containing error string
*/
char *cdc_get_error_string(struct cdc_ctx *cdc, char *buf, int size)
{
    char const * ctx;
    int code;
    if (cdc == NULL) {
        ctx = "struct cdc_ctx *cdc";
        code = CDC_ERROR_INVALID_PARAM;
    } else {
        ctx = cdc->error_str;
        code = cdc->error_code;
    }
    snprintf(buf, size, "%s %s %s", ctx, libusb_error_name(code), libusb_strerror(code));
    return buf;
}

/* @} end of doxygen libcdc group */
