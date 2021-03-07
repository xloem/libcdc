/* simple.c

   Simple libcdc usage example

   This program is distributed under the GPL, version 3
*/

#include <stdio.h>
#include <stdlib.h>
#include <cdc.h>

int main(void)
{
    int ret;
    char errbuf[256];
    struct cdc_ctx *cdc;
    struct cdc_version_info version;
    if ((cdc = cdc_new()) == 0) {
        fprintf(stderr, "ftdi_new failed\n");
        return EXIT_FAILURE;
    }

    version = cdc_get_library_version();
    printf("initialised libcdc %s (major: %d, minor %d, micro: %d, snapshot ver: %s)\n",
           version.version_str, version.major, version.minor, version.micro,
           version.snapshot_str);

    if ((ret = cdc_usb_open(cdc, 0x2458, 0x0001)) < 0)
    {
        fprintf(stderr, "unable to open cdc device: %d (%s)\n", ret, cdc_get_error_string(cdc, errbuf, sizeof(errbuf)));
        cdc_free(cdc);
        return EXIT_FAILURE;
    }

    if ((ret = cdc_usb_close(cdc)) < 0)
    {
        fprintf(stderr, "unable to close cdc device: %d (%s)\n", ret, cdc_get_error_string(cdc, errbuf, sizeof(errbuf)));
        cdc_free(cdc);
        return EXIT_FAILURE;
    }

    cdc_free(cdc);

    return EXIT_SUCCESS;
}
