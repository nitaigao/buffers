#ifndef OUTPUT_H_
#define OUTPUT_H_

#include <xf86drm.h>
#include <xf86drmMode.h>

struct device;

struct output {
  struct device *device;
  uint32_t primary_plane_id;
  uint32_t crtc_id;
  uint32_t connector_id;
};

struct output *output_create(struct device *device, drmModeConnectorPtr connector);

struct output* output_new(
  struct device *device,
  uint32_t primary_plane_id,
  uint32_t crtc_id,
  uint32_t connector_id
);

#endif  // OUTPUT_H_
