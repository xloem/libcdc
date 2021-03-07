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

#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdc.h"
#include "cdc_version_i.h"

#define cdc_return(code, str, ...) \
    if (cdc) {                     \
        cdc->error_str = (str);    \
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
    Internal function to close usb device pointer.
    Set cdc->usb_dev to NULL.
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

    /* get from lsusb -v */
    cdc->out_ep = 0x83; /* reading */
    cdc->in_ep = 0x02; /* writing */

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
    Opens a cdc device given by a usb_device.

    \param cdc pointer to cdc_ctx
    \param dev libusb usb_dev to use

    \return CDC_SUCCESS on success or CDC_ERROR code on failure
*/
int cdc_usb_open_dev(struct cdc_ctx *cdc, libusb_device *dev)
{
    /*struct libusb_device_descriptor desc;
    struct libusb_config_descriptor *config0;*/

    cdc_check(cdc ? CDC_SUCCESS : CDC_ERROR_INVALID_PARAM, "struct cdc_ctx *cdc");
    cdc_check(libusb_open(dev, &cdc->usb_dev), "libusb_open");

    /* Try to detach cdc-acm module.
       The CDC-ACM Class defines two interfaces:
       the Control interface and the Data interface.
       Note this might harmlessly fail.
     */
    for (int if_num = 0; if_num < 2; if_num ++) {
        if (cdc->module_detach_mode == AUTO_DETACH_CDC_MODULE) {
            libusb_detach_kernel_driver(cdc->usb_dev, if_num);
        } else if (cdc->module_detach_mode == AUTO_DETACH_REATTACH_CDC_MODULE) {
            libusb_set_auto_detach_kernel_driver(cdc->usb_dev, if_num);
        }
        cdc_check(
            libusb_claim_interface(cdc->usb_dev, if_num),
            "libusb_claim_interface",
            cdc_usb_close_internal(cdc)
        );
    }


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
#if 0







/**
 * metapair of previous situation going now.
 * is somewhat similar to convulsion issues, slower.
 * karl requests to not do shielding project right now.  reason was present, misplaced.  related to growth of issue.
 */


/**
 * concept of maybe two unmet neuron promises, with active promise
 *
 *  concept of sidespread taking smooth healing
 *              split, boss-bubble, neuron-expression.
 *       supporting sidespread some.
 *                  goal.  sidespread has behavior of.
 */


/* messy situation produced nonaligned fetus-focus.  was a failure to reliably repeat a usually well-repeated memory crutch [and a new rep/community made near injury healing]. */
    /* re: cpunks list.  worry. */ /* karl does not support harm.  harm is hard to define. the group model learning harm [in pair with boss]. */
        /* shielding.  very slow, this project.  very very slow. */
        /* shielding ideas: tool for measuring effectiveness. wiki for sharing knowledge [gnuradio has a wiki, so does wikipedia, and there's wikiapiary near a huge wiki index] */
            /* shielding measurement tool. could turn into a quick diy shielded room toolkit. */
            /* processing of logical knowledge around shielding.  learning. */
            /* judgement bits all scattered.  maybe deduction tool could be helpful. :-/ */
            /* poor experience with this.  influenced to follow trainings, doesn't usually get training. */
                /* there exist multiple specifications on how-to-shield.  these could make a clear shielding-way.
 *                 additionally the physics are pretty simple.  tool-idea just needs consistent work. */
            /* has many false-starts. [publicised] */
                /* next step: ensure at least one how-to-shield specification is linked from openemissions repo. */
            /* this is _really_ [] against.  strongest. karl has partial reason for strongest, involves time-energy spent on focus, and his personal pattern. */
 *                




/*
            }
        }        
    }
*/

#endif
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

/* @} end of doxygen libftdi group */
