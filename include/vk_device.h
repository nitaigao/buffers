#ifndef VK_DEVICE_H_
#define VK_DEVICE_H_

struct device;

struct vk_device {
  int a;
};

struct vk_device *vk_device_create(struct device *device);

#endif  // VK_DEVICE_H_
