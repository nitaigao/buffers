#ifndef DEVICE_H_
#define DEVICE_H_

#include <xf86drmMode.h>

struct device {
  int kms_fd;

  drmModeResPtr res;
  drmModePlanePtr *planes;
  int num_planes;

  struct output **outputs;
  int num_outputs;

  struct gbm_device *gbm_device;
};

#endif  // DEVICE_H_
