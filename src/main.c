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

#ifndef O_CLOEXEC
#define O_CLOEXEC	02000000	/* set close_on_exec */
#endif

struct device {
  int kms_fd;
};

static struct device *device_open(const char *filename) {
  struct device *ret = calloc(1, sizeof(*ret));

  ret->kms_fd = open(filename, O_RDWR | O_CLOEXEC, 0);
  if (ret->kms_fd < 0) {
    fprintf(stderr, "warning: couldn't open %s: %s\n", filename, strerror(errno));
    goto err;
  }

  drm_magic_t magic;
  int cookie = drmGetMagic(ret->kms_fd, &magic);
  int auth = drmAuthMagic(ret->kms_fd, magic);
  if (cookie != 0 || auth != 0) {
    fprintf(stderr, "KMS device %s is not master\n", filename);
    goto err_fd;
  }

  return ret;

err_fd:
  close(ret->kms_fd);

err:
  free(ret);
  return NULL;
}

struct device* device_create() {
  int num_devices = drmGetDevices2(0, NULL, 0);

  drmDevicePtr *devices = calloc(num_devices, sizeof(*devices));
  assert(devices);

  drmGetDevices2(0, devices, num_devices);
  printf("%d DRM devices available\n", num_devices);

  struct device *ret = NULL;

  for (int i = 0; i < num_devices; i++) {
    drmDevicePtr candidate = devices[i];

    if (!(candidate->available_nodes & (1 << DRM_NODE_PRIMARY))) {
      continue;
    }

    const char* filename = candidate->nodes[DRM_NODE_PRIMARY];
    ret = device_open(filename);

    if (ret) {
      break;
    }
  }

  drmFreeDevices(devices, num_devices);

  if (!ret) {
    fprintf(stderr, "couldn't find any suitable KMS device\n");
    goto err;
  }

err:
  return NULL;
}

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
