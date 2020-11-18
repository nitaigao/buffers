#include "vk_device.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "device.h"

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

static bool has_extension(const VkExtensionProperties *avail,
  uint32_t availc, const char *req)
{
  // check if all required extensions are supported
  for (size_t j = 0; j < availc; ++j) {
    if (!strcmp(avail[j].extensionName, req)) {
      return true;
    }
  }

  return false;
}

static bool locateValidationLayer(const char *layer_name) {

  uint32_t available_layers_count;
  vkEnumerateInstanceLayerProperties(&available_layers_count, NULL);

  VkLayerProperties available_layers[available_layers_count];
  vkEnumerateInstanceLayerProperties(&available_layers_count, available_layers);

  for (int i = 0; i < available_layers_count; i++) {
    if (strcmp(layer_name, available_layers[i].layerName) == 0) {
      return true;
    }
  }

  return false;
}

static void create_instance(struct vk_device *vk_dev)
{
  VkResult res;

  VkApplicationInfo appInfo = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "Hello Triangle",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "No Engine",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_MAKE_VERSION(1, 1, 0)
  };

  uint32_t avail_extc = 0;
  res = vkEnumerateInstanceExtensionProperties(NULL, &avail_extc, NULL);
  if ((res != VK_SUCCESS) || (avail_extc == 0)) {
    printf("Could not enumerate instance extensions\n");
    return NULL;
  }

  VkExtensionProperties *avail_exts = calloc(avail_extc, sizeof(*avail_exts));
  res = vkEnumerateInstanceExtensionProperties(NULL, &avail_extc, avail_exts);
  if (res != VK_SUCCESS) {
    free(avail_exts);
    printf("Could not enumerate instance extensions\n");
    return NULL;
  }

  for (size_t j = 0; j < avail_extc; ++j) {
    printf("Vulkan Instance extensions %s\n", avail_exts[j].extensionName);
  }

  // create instance
  const char *req = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  const char** enable_exts = NULL;
  uint32_t enable_extc = 0;
  if (has_extension(avail_exts, avail_extc, req)) {
    enable_exts = &req;
    enable_extc++;
  }

  VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &appInfo,
    .enabledExtensionCount = enable_extc,
    .ppEnabledExtensionNames = enable_exts,
  };

  const char *vk_layer_khronos_validation = "VK_LAYER_KHRONOS_validation";
  const char *vk_layer_lunarg_standard_validation = "VK_LAYER_LUNARG_standard_validation";

  if (enableValidationLayers && locateValidationLayer(vk_layer_khronos_validation)) {
    printf("Enabled %s layer\n", vk_layer_khronos_validation);
    instance_info.ppEnabledLayerNames = &vk_layer_khronos_validation;
    instance_info.enabledLayerCount = 1;
  } else if (enableValidationLayers && locateValidationLayer(vk_layer_lunarg_standard_validation)) {
    printf("Enabled %s layer\n", vk_layer_lunarg_standard_validation);
    instance_info.ppEnabledLayerNames = &vk_layer_lunarg_standard_validation;
    instance_info.enabledLayerCount = 1;
  } else {
    instance_info.enabledLayerCount = 0;
  }

  res = vkCreateInstance(&instance_info, NULL, &vk_dev->instance);
  if (res != VK_SUCCESS) {
    printf("Failed to create Vulkan instance\n");
    return;
  }
}

static void pick_physical_device(struct device* device, struct vk_device *vk_dev)
{
  VkResult res;

  drmDevicePtr drm_device;
  drmGetDevice(device->kms_fd, &drm_device);
  if(drm_device->bustype != DRM_BUS_PCI) {
    fprintf(stderr, "Given device isn't a pci device\n");
    goto error;
  }

  uint32_t physical_device_count = 0;;
  res = vkEnumeratePhysicalDevices(vk_dev->instance, &physical_device_count, NULL);
  if (res != VK_SUCCESS || physical_device_count == 0) {
    fprintf(stderr, "Could not retrieve physical device\n");
    goto error;
  }

error:
  return;
}

void vk_device_destroy(struct vk_device *device) {
  if (device->instance) {
    // vkDestroyInstance(device->instance, NULL);
  }

  free(device);
}

struct vk_device *vk_device_create(struct device *device)
{
  (void)device;
  // if (!device->fb_modifiers) {
  //   printf("Can't use vulkan since drm doesn't support modifiers\n");
  //   return NULL;
  // }

  struct vk_device *ret = calloc(1, sizeof(*ret));
  assert(ret);

  create_instance(ret);

  if (!ret->instance) {
    goto catch_instance;
  }

  pick_physical_device(device, ret);

  vk_device_destroy(ret);

  // // create instance
  // const char *req = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  // const char** enable_exts = NULL;
  // uint32_t enable_extc = 0;
  // if (has_extension(avail_exts, avail_extc, req)) {
  //   enable_exts = &req;
  //   enable_extc++;
  // }

  // free(avail_exts);

  return ret;

catch_instance:
  vk_device_destroy(ret);
  return NULL;
}
