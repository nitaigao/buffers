#ifndef DEVICE_H_
#define DEVICE_H_

#include <stdbool.h>
#include <xf86drmMode.h>
#include <gbm.h>

struct device {
  int kms_fd;

  drmModeResPtr res;
  drmModePlanePtr *planes;
  int num_planes;

  struct output **outputs;
  int num_outputs;

  bool fb_modifiers;

  struct gbm_device *gbm_device;
};

#endif  // DEVICE_H_
