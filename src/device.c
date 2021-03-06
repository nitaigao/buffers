#include "device.h"

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
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

#ifndef O_CLOEXEC
#define O_CLOEXEC	02000000  /* set close_on_exec */
#endif

#include "output.h"
#include "vk_device.h"

static struct device *device_open(const char *filename) {
  int err = 0;

  struct device *ret = calloc(1, sizeof(*ret));
  assert(ret);

  ret->kms_fd = open(filename, O_RDWR | O_CLOEXEC);
  if (ret->kms_fd < 0) {
    fprintf(stderr, "Couldn't open %s: %s\n", filename, strerror(errno));
    goto err;
  }

  // drm_magic_t magic;
  // int cookie = drmGetMagic(ret->kms_fd, &magic);
  // int auth = drmAuthMagic(ret->kms_fd, magic);
  // if (cookie != 0 || auth != 0) {
  //   fprintf(stderr, "KMS device %s is not master\n", filename);
  //   goto err_fd;
  // }

  err = drmSetClientCap(ret->kms_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

  if (err != 0) {
    fprintf(stderr, "No support for universal planes\n");
    goto err_fd;
  }

  err = drmSetClientCap(ret->kms_fd, DRM_CLIENT_CAP_ATOMIC, 1);
  if (err != 0) {
    fprintf(stderr, "No support for atomic\n");
    goto err_fd;
  }

  uint64_t cap = 0;

  err = drmGetCap(ret->kms_fd, DRM_CAP_ADDFB2_MODIFIERS, &cap);
  ret->fb_modifiers = (err == 0 && cap != 0);

  printf("Device %s framebuffer modifiers\n",
    (ret->fb_modifiers) ? "supports" : "does not support");

  ret->res = drmModeGetResources(ret->kms_fd);
  if (!ret->res) {
    fprintf(stderr, "Couldn't get card resources for %s\n", filename);
    goto err_fd;
  }

  drmModePlaneResPtr plane_res = drmModeGetPlaneResources(ret->kms_fd);
  if (!plane_res) {
    fprintf(stderr, "Device %s has no planes\n", filename);
    goto err_res;
  }

  if (ret->res->count_crtcs <= 0
    || ret->res->count_connectors <= 0
    || ret->res->count_encoders <= 0
    || plane_res->count_planes <= 0)
  {
    printf("Device %s is not a KMS device\n", filename);
    goto err_plane_res;
  }

  ret->planes = calloc(plane_res->count_planes, sizeof(*ret->planes));
  ret->num_planes = plane_res->count_planes;
  assert(ret->planes);

  for (unsigned int i = 0; i < plane_res->count_planes; i++) {
    ret->planes[i] = drmModeGetPlane(ret->kms_fd, plane_res->planes[i]);
    assert(ret->planes[i]);
  }

  ret->outputs = calloc(ret->res->count_connectors, sizeof(*ret->outputs));
  assert(ret->outputs);

  for (int i = 0; i < ret->res->count_connectors; i++) {
    drmModeConnectorPtr connector = drmModeGetConnector(ret->kms_fd, ret->res->connectors[i]);
    struct output *output = output_create(ret, connector);

    if (!output) {
      continue;
    }

    ret->outputs[ret->num_outputs++] = output;
  }

  if (ret->num_outputs == 0) {
    fprintf(stderr, "Device %s has no active outputs\n", filename);
    goto err_outputs;
  }

  ret->gbm_device = gbm_create_device(ret->kms_fd);

  ret->vk_device = vk_device_create(ret);

  printf("Using device %s with %d outputs and %d planes\n", filename,
    ret->num_outputs, ret->num_planes);

  return ret;

err_outputs:
  free(ret->outputs);
  for (int i = 0; i < ret->num_planes; i++) {
    drmModeFreePlane(ret->planes[i]);
  }
  free(ret->planes);

err_plane_res:
  drmModeFreePlaneResources(plane_res);

err_res:
  drmModeFreeResources(ret->res);

err_fd:
  close(ret->kms_fd);

err:
  free(ret);
  return NULL;
}

struct device* device_create() {
  int num_devices = drmGetDevices2(0, NULL, 0);

  if (num_devices <= 0) {
    fprintf(stderr, "No KSM devices present\n");
    goto err;
  }

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
    fprintf(stderr, "Couldn't find any suitable KMS device\n");
    goto err;
  }

err:
  return NULL;
}
