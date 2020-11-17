#include "output.h"

#include <assert.h>
#include <stdlib.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

struct output *output_create(struct device *device, drmModeConnectorPtr connector)
{
  struct output *ret = calloc(1, sizeof(ret));
  assert(ret);

  return ret;
}
