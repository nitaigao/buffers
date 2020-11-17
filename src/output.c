#include "output.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "device.h"

static drmModeEncoderPtr find_encoder(struct device *device, drmModeConnectorPtr connector) {
  for (int i = 0; i < device->res->count_encoders; i++) {
    uint32_t encoder_id = device->res->encoders[i];
    if (encoder_id == connector->encoder_id) {
      drmModeEncoderPtr encoder = drmModeGetEncoder(device->kms_fd, encoder_id);
      return encoder;
    }
  }

  printf("Could not find device encoder\n");

  return NULL;
}

static drmModeCrtcPtr find_crtc(struct device *device, drmModeEncoderPtr encoder) {
  for (int i = 0; i < device->res->count_crtcs; i++) {
    uint32_t crtc_id = device->res->crtcs[i];
    if (crtc_id == encoder->crtc_id) {
      drmModeCrtcPtr crtc = drmModeGetCrtc(device->kms_fd, crtc_id);
      return crtc;
    }
  }

  printf("Could not find CRTC\n");

  return NULL;
}

static bool drm_is_primary_plane(struct device *device, uint32_t plane_id)
{
  uint32_t p;
  bool found = false;
  bool ret = false;
  drmModeObjectPropertiesPtr props =
    drmModeObjectGetProperties(device->kms_fd, plane_id, DRM_MODE_OBJECT_PLANE);
  if (!props) {
    return false;
  }
  for (p = 0; p < props->count_props && !found; p++) {
    drmModePropertyPtr prop = drmModeGetProperty(device->kms_fd, props->props[p]);
    if (prop) {
      if (strcmp("type", prop->name) == 0) {
        found = true;
        ret = props->prop_values[p] == DRM_PLANE_TYPE_PRIMARY;
      }
      drmModeFreeProperty(prop);
    }
  }
  drmModeFreeObjectProperties(props);
  return ret;
}

static drmModePlanePtr find_primary_plane(struct device *device) {
  for (int p = 0; p < device->num_planes; p++) {
    drmModePlanePtr plane = device->planes[p];
    int is_plane = drm_is_primary_plane(device, plane->plane_id);
    if (is_plane) {
      return plane;
    }
  }

  printf("Could not find primary plan\n");

  return NULL;
}

struct output* output_new(
  struct device *device,
  uint32_t primary_plane_id,
  uint32_t crtc_id,
  uint32_t connector_id,
  drmModeModeInfoPtr mode_info,
  int64_t refresh_nsec
) {
  struct output *ret = calloc(1, sizeof(*ret));
  assert(ret);

  ret->device = device;
  ret->primary_plane_id = primary_plane_id;
  ret->crtc_id = crtc_id;
  ret->connector_id = connector_id;
  ret->mode_info = mode_info;
  ret->refresh_nsec = refresh_nsec;

  return ret;
}

static int64_t millihz_to_nsec(uint32_t mhz)
{
  assert(mhz > 0);
  return 1000000000000LL / mhz;
}

struct output *output_create(struct device *device, drmModeConnectorPtr connector)
{
  struct output *ret = NULL;

  if (connector->encoder_id == 0) {
    printf("No encoder for %d\n", connector->connector_id);
    return NULL;
  }

  drmModeEncoderPtr encoder = find_encoder(device, connector);
  assert(encoder);

  if (encoder->crtc_id == 0) {
    printf("No CRTC for %d\n", encoder->encoder_id);
    goto err_encoder;
  }

  drmModeCrtcPtr crtc = find_crtc(device, encoder);
  assert(crtc);

  if (crtc->buffer_id == 0) {
    printf("CRTC is not active\n");
    goto err_crtc;
  }

  drmModePlanePtr primary_plane = find_primary_plane(device);
  assert(primary_plane);

  uint64_t refresh_millihz = ((crtc->mode.clock * 1000000LL / crtc->mode.htotal) +
    (crtc->mode.vtotal / 2)) / crtc->mode.vtotal;

  int64_t refresh_nsec = millihz_to_nsec(refresh_millihz);

  ret = output_new(
    device,
    primary_plane->plane_id,
    crtc->crtc_id,
    connector->connector_id,
    &crtc->mode,
    refresh_nsec);

  assert(ret);

err_crtc:
  drmModeFreeCrtc(crtc);

err_encoder:
  drmModeFreeEncoder(encoder);

  return ret;
}
