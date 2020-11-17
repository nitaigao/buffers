#ifndef OUTPUT_H_
#define OUTPUT_H_

#include <xf86drm.h>
#include <xf86drmMode.h>

struct device;

struct output {
  int a;
};

struct output *output_create(struct device *device, drmModeConnectorPtr connector);

#endif  // OUTPUT_H_
