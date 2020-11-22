#ifndef VK_DEVICE_H_
#define VK_DEVICE_H_

#include <stdbool.h>
#include <vulkan/vulkan.h>

struct device;

struct vk_device {
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue queue;
  VkCommandPool command_pool;
  VkDescriptorPool descriptor_pool;
  VkRenderPass render_pass;
  VkDescriptorSetLayout descriptor_set_layout;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  uint32_t enabled_extensions_count;
  const char* const* enabled_extensions;

  uint32_t queue_family;
};

struct vk_device *vk_device_create(struct device *device);

#endif  // VK_DEVICE_H_
