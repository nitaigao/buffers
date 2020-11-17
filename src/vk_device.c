#include "vk_device.h"

#include <assert.h>
#include <stdlib.h>

struct vk_device *vk_device_create(struct device *device)
{
  struct vk_device *ret = calloc(1, sizeof(ret));
  assert(ret);

  return ret;
}
