#ifndef VK_DEVICE_H_
#define VK_DEVICE_H_

#include <vulkan/vulkan.h>

struct device;

struct vk_device {
  VkInstance instance;
};

struct vk_device *vk_device_create(struct device *device);

#endif  // VK_DEVICE_H_
