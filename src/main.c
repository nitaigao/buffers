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

  drmModeResPtr res;
	drmModePlanePtr *planes;
	int num_planes;

  struct output **outputs;
	int num_outputs;

  struct gbm_device *gbm_device;
};

struct vk_device {
  int a;
};

struct output {
  int a;
};

struct vk_device *vk_device_create(struct device *device)
{
  struct vk_device *ret = calloc(1, sizeof(ret));
  return ret;
}

struct output *output_create(struct device *device, drmModeConnectorPtr connector)
{
  struct output *ret = calloc(1, sizeof(ret));
  return ret;
}

static struct device *device_open(const char *filename) {
  int err = 0;

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

  err = drmSetClientCap(ret->kms_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
	if (err != 0) {
		fprintf(stderr, "no support for universal planes\n");
		goto err_fd;
	}

  err |= drmSetClientCap(ret->kms_fd, DRM_CLIENT_CAP_ATOMIC, 1);
  if (err != 0) {
		fprintf(stderr, "no support for atomic\n");
		goto err_fd;
	}

  ret->res = drmModeGetResources(ret->kms_fd);
	if (!ret->res) {
		fprintf(stderr, "couldn't get card resources for %s\n", filename);
    goto err_fd;
  }

  drmModePlaneResPtr plane_res = drmModeGetPlaneResources(ret->kms_fd);
	if (!plane_res) {
		fprintf(stderr, "device %s has no planes\n", filename);
		goto err_res;
	}

  if (ret->res->count_crtcs <= 0
   || ret->res->count_connectors <= 0
   || ret->res->count_encoders <= 0
   || plane_res->count_planes <= 0)
   {
		printf("device %s is not a KMS device\n", filename);
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
		fprintf(stderr, "device %s has no active outputs\n", filename);
		goto err_outputs;
	}

  struct vk_device *vulkan_device  = vk_device_create(ret);
  (void)vulkan_device;

	printf("using device %s with %d outputs and %d planes\n", filename, ret->num_outputs, ret->num_planes);

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
