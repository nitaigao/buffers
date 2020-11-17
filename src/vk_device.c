#include "vk_device.h"

#include <assert.h>
#include <stdlib.h>

#include "device.h"

struct vk_device *vk_device_create(struct device *device)
{
  (void)device;
  // if (!device->fb_modifiers) {
  //   printf("Can't use vulkan since drm doesn't support modifiers\n");
  //   return NULL;
  // }

  struct vk_device *ret = calloc(1, sizeof(*ret));
  assert(ret);

  return ret;
}
