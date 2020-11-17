#include <stdio.h>

#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <linux/kd.h>
#include <linux/major.h>
#include <linux/vt.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "device.h"

int main() {
  struct device *device = device_create();
  if (!device) {
    printf("Failed to get device\n");
    goto err_device;
  }
  printf("got device %d", device->kms_fd);

err_device:
  return 0;
}
