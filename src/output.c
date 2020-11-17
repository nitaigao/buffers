#include "output.h"

#include <assert.h>
#include <stdlib.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "device.h"

static drmModeEncoderPtr find_encoder(struct device *device, drmModeConnectorPtr connector) {
  if (connector->encoder_id == 0) {
    printf("No encoder for %d\n", connector->connector_id);
    return NULL;
  }

  for (int i = 0; i < device->res->count_encoders; i++) {
    if (device->res->encoders[i] == connector->encoder_id) {
      return device->res->encoders[i];
    }
  }

  printf("Could not find device encoder\n");

  return NULL;
}

static drmModeCrtcPtr find_crtc(struct device *device, drmModeCrtcPtr crtc) {
  for (int p = 0; p < device->num_planes; p++) {
    if (device->planes[p]->crtc_id == crtc->crtc_id &&
        device->planes[p]->fb_id == crtc->buffer_id) {
      return device->planes[p];
    }
  }

  printf("Could not find primary plan\n");

  return NULL;
}

static drmModePlanePtr find_primary_plan(struct device *device, drmModeEncoderPtr encoder) {
  if (encoder->crtc_id == 0) {
    printf("No CRTC for %d\n", encoder->encoder_id);
    return NULL;
  }

  for (int i = 0; i < device->res->count_crtcs; i++) {
    if (device->res->crtcs[i] == encoder->crtc_id) {
      return device->res->crtcs[i];
    }
  }

  printf("Could not find CRTC\n");

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
  struct output *ret = calloc(1, sizeof(ret));
  assert(ret);

  ret->device = device;
  ret->primary_plane_id = primary_plane_id;
  ret->crtc_id = crtc_id;
  ret->connector_id = connector_id;

  return ret;
}

static int64_t millihz_to_nsec(uint32_t mhz)
{
	assert(mhz > 0);
	return 1000000000000LL / mhz;
}

struct output *output_create(struct device *device, drmModeConnectorPtr connector)
{
  int err = 0;
  struct output *ret = NULL;

  drmModeEncoderPtr encoder = find_encoder(device, connector);
  assert(encoder);

  drmModeCrtcPtr crtc = find_crtc(device, connector);
  assert(crtc);

  if (crtc->buffer_id == 0) {
    printf("CRTC is not active\n");
    goto err_crtc;
  }

  drmModePlanePtr primary_plane = find_primary_plan(device, encoder);
  assert(primary_plane);

  uint64_t refresh_millihz = ((crtc->mode.clock * 1000000LL / crtc->mode.htotal) +
    (crtc->mode.vtotal / 2)) / crtc->mode.vtotal;

  int64_t refresh_nsec = millihz_to_nsec(refresh_millihz);

  uint32_t mode_blob_id = mode_blob_create(device, &crtc->mode);

  ret = output_new(
    device,
    primary_plane->plane_id,
    crtc->crtc_id,
    connector->connector_id,
    &crtc->mode,
    refresh_nsec);

err_crtc:
  drmModeFreeCrtc(crtc);

  return ret;
}
