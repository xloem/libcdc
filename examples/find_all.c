/* find_all.c

   Example for cdc_usb_find_all()

   This program is distributed under the GPL, version 2
*/

#include <stdio.h>
#include <stdlib.h>
#include <cdc.h>

int main(void)
{
    int ret, i;
    struct cdc_ctx *cdc;
    struct cdc_device_list *devlist, *curdev;
    char manufacturer[128], description[128], serial[128], err[256];
    int retval = EXIT_SUCCESS;

    if ((cdc = cdc_new()) == 0)
    {
        fprintf(stderr, "cdc_new failed\n");
        return EXIT_FAILURE;
    }

    if ((ret = cdc_usb_find_all(cdc, &devlist, 0, 0)) < 0)
    {
        fprintf(stderr, "cdc_usb_find_all failed: %d (%s)\n", ret, cdc_get_error_string(cdc, err, sizeof(err)));
        retval =  EXIT_FAILURE;
        goto do_deinit;
    }

    printf("Number of FTDI devices found: %d\n", ret);

    i = 0;
    for (curdev = devlist; curdev != NULL; i++)
    {
        printf("Checking device: %d\n", i);
        if ((ret = cdc_usb_get_strings(cdc, curdev->dev, manufacturer, 128, description, 128, serial, 128)) < 0)
        {
            fprintf(stderr, "cdc_usb_get_strings failed: %d (%s)\n", ret, cdc_get_error_string(cdc, err, sizeof(err)));
            retval = EXIT_FAILURE;
            goto done;
        }
        printf("Manufacturer: %s, Description: %s, Serial: %s\n\n", manufacturer, description, serial);
        curdev = curdev->next;
    }
done:
    cdc_list_free(&devlist);
do_deinit:
    cdc_free(cdc);

    return retval;
}
