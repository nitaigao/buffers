#include "vk_device.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <shader.frag.h>
#include <shader.vert.h>

#include "device.h"

#define BUFFER_QUEUE_DEPTH 3

static const VkFormat swapChainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

static bool extensions_has_extension(VkExtensionProperties *extensions,
  uint32_t extensions_count, const char *extension_name)
{
  for (size_t i = 0; i < extensions_count; i++) {
    VkExtensionProperties extension = extensions[i];
    bool has = strcmp(extension.extensionName, extension_name) == 0;
    if (has) {
      return true;
    }
  }

  return false;
}

static bool has_extension(const char *extension_name)
{
  bool has = false;

  VkResult res;

  uint32_t available_extensions_count;

  res = vkEnumerateInstanceExtensionProperties(NULL, &available_extensions_count, NULL);
  if (res != VK_SUCCESS || available_extensions_count == 0) {
    printf("Could not enumerate instance extensions\n");
    return false;
  }

  VkExtensionProperties *available_extensions = calloc(available_extensions_count,
    sizeof(VkExtensionProperties));

  res = vkEnumerateInstanceExtensionProperties(NULL,
    &available_extensions_count, available_extensions);

  if (res != VK_SUCCESS) {
    printf("Could not enumerate instance extensions\n");
    goto error;
  }

  for (size_t i = 0; i < available_extensions_count; i++) {
    has = extensions_has_extension(available_extensions,
      available_extensions_count, extension_name);

    if (has) {
      break;
    }
  }

error:
  free(available_extensions);

  return has;
}

static bool locateValidationLayer(const char *layer_name)
{
  uint32_t available_layers_count;
  vkEnumerateInstanceLayerProperties(&available_layers_count, NULL);

  VkLayerProperties available_layers[available_layers_count];
  vkEnumerateInstanceLayerProperties(&available_layers_count, available_layers);

  for (uint32_t i = 0; i < available_layers_count; i++) {
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

  VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &appInfo,
    .enabledExtensionCount = vk_dev->enabled_extensions_count,
    .ppEnabledExtensionNames = vk_dev->enabled_extensions,
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

bool device_has_extension(VkPhysicalDevice physical_device, const char *extension_name)
{
  VkResult res;

  uint32_t extension_count = 0;

  res = vkEnumerateDeviceExtensionProperties(physical_device, NULL, &extension_count, NULL);
  if (res != VK_SUCCESS || extension_count == 0) {
    fprintf(stderr, "Could not enumerate device extensions (1)");
    return false;
  }

  VkExtensionProperties *extensions = calloc(extension_count, sizeof(VkExtensionProperties));
  res = vkEnumerateDeviceExtensionProperties(physical_device, NULL, &extension_count, extensions);
  if (res != VK_SUCCESS) {
    fprintf(stderr, "Could not enumerate device extensions (2)");
    return false;
  }

  bool has_extension = extensions_has_extension(extensions, extension_count, extension_name);

  free(extensions);

  return has_extension;
}

static bool device_matches(drmPciBusInfoPtr pci_bus_info, VkPhysicalDevice physical_device)
{
  bool has_pci_extension = device_has_extension(physical_device,
    VK_EXT_PCI_BUS_INFO_EXTENSION_NAME);

  if (!has_pci_extension) {
    fprintf(stderr, "Physical device has not support for VK_EXT_pci_bus_info\n");
    return false;
  }

  VkPhysicalDevicePCIBusInfoPropertiesEXT pci_props = {0};
  pci_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT;

  VkPhysicalDeviceProperties2 physical_device_props = {0};
  physical_device_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  physical_device_props.pNext = &pci_props;

  vkGetPhysicalDeviceProperties2(physical_device, &physical_device_props);
  bool match = pci_props.pciBus == pci_bus_info->bus &&
    pci_props.pciDevice == pci_bus_info->dev &&
    pci_props.pciDomain == pci_bus_info->domain &&
    pci_props.pciFunction == pci_bus_info->func;

  VkPhysicalDeviceProperties *props = &physical_device_props.properties;
  uint32_t vv_major = (props->apiVersion >> 22);
  uint32_t vv_minor = (props->apiVersion >> 12) & 0x3ff;
  uint32_t vv_patch = (props->apiVersion) & 0xfff;

  uint32_t dv_major = (props->driverVersion >> 22);
  uint32_t dv_minor = (props->driverVersion >> 12) & 0x3ff;
  uint32_t dv_patch = (props->driverVersion) & 0xfff;

  const char* dev_type = "unknown";

  switch (props->deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      dev_type = "integrated";
      break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      dev_type = "discrete";
      break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      dev_type = "cpu";
      break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      dev_type = "gpu";
      break;
    default:
      break;
  }

  printf("Vulkan device: '%s'\n", props->deviceName);
  printf("  Device type: '%s'\n", dev_type);
  printf("  Supported API version: %u.%u.%u\n", vv_major, vv_minor, vv_patch);
  printf("  Driver version: %u.%u.%u\n", dv_major, dv_minor, dv_patch);
  printf("  match: %d\n", match);

  return match;
}

static VkPhysicalDevice find_pci_device(VkInstance instance, drmPciBusInfoPtr pci)
{
  VkResult res;

  VkPhysicalDevice physical_device = VK_NULL_HANDLE;

  uint32_t physical_device_count = 0;
  res = vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL);
  if (res != VK_SUCCESS || physical_device_count == 0) {
    fprintf(stderr, "Could not retrieve physical device\n");
    goto error;
  }

  VkPhysicalDevice *physical_devices = calloc(physical_device_count, sizeof(VkPhysicalDevice));
  res = vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices);
  if (res != VK_SUCCESS || physical_device_count == 0) {
    fprintf(stderr, "Could not retrieve physical device");
    goto error;
  }

  printf("PCI bus: %04x:%02x:%02x.%x\n", pci->domain, pci->bus, pci->dev, pci->func);


  for (unsigned i = 0u; i < physical_device_count; i++) {
    VkPhysicalDevice temp_physical_device = physical_devices[i];
    bool match = device_matches(pci, temp_physical_device);
    if (match) {
      physical_device = temp_physical_device;
      break;
    }
  }

  if (physical_device == VK_NULL_HANDLE) {
    fprintf(stderr, "Can't find vulkan physical device for drm dev\n");
    goto error;
  }

error:
  free(physical_devices);

  return physical_device;
}

static bool check_memory_extensions(VkPhysicalDevice physical_device)
{
  const char* memory_extensions[] = {
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,  // required by drm ext

    // NOTE: strictly speaking this extension is required to
    // correctly transfer image ownership but since no mesa
    // driver implements its yet (no even an updated patch for that),
    // let's see how far we get without it
    // VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
  };

  size_t memory_extensions_count = sizeof(memory_extensions) / sizeof(memory_extensions[0]);

  for (unsigned i = 0u; i < memory_extensions_count; i++) {
    if (!device_has_extension(physical_device, memory_extensions[i])) {
      fprintf(stderr, "Physical device doesn't supported required extension: %s\n",
        memory_extensions[i]);
      return false;
    }
  }

  return true;
}

static void locate_queue_families(struct vk_device *ret)
{
  uint32_t queue_family_count;
  vkGetPhysicalDeviceQueueFamilyProperties(ret->physical_device, &queue_family_count, NULL);

  VkQueueFamilyProperties *queue_family_properties = calloc(queue_family_count,
    sizeof(VkQueueFamilyProperties));

  vkGetPhysicalDeviceQueueFamilyProperties(ret->physical_device,
    &queue_family_count, queue_family_properties);

  uint32_t queue_family = -1;

  for (uint32_t i = 0; i < queue_family_count; i++) {
    if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      ret->queue_family = i;
      break;
    }
  }

  assert(queue_family);
}

static void pick_physical_device(struct device* device, struct vk_device *vk_dev)
{
  drmDevicePtr drm_device;
  drmGetDevice(device->kms_fd, &drm_device);
  if (drm_device->bustype != DRM_BUS_PCI) {
    fprintf(stderr, "Given device isn't a pci device\n");
    goto error;
  }

  vk_dev->physical_device = find_pci_device(vk_dev->instance, drm_device->businfo.pci);
  assert(vk_dev->physical_device);

  bool has_memory_extensions = check_memory_extensions(vk_dev->physical_device);
  if (!has_memory_extensions) {
    goto error;
  }

  bool has_semaphore = device_has_extension(vk_dev->physical_device,
    VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);

  if (!has_semaphore) {
    fprintf(stderr, "Physical device doesn't supported extension %s, which "
      "is required for explicit fencing.", VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    goto error;
  }

  locate_queue_families(vk_dev);

error:
  return;
}

static void create_logical_device(struct vk_device *vk_dev)
{
  VkResult res;

  float priority = 1.0f;

  VkDeviceQueueCreateInfo queue_info = {0};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = vk_dev->queue_family;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &priority;

  const char* mem_exts[] = {
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
  };

  VkDeviceCreateInfo device_info = {0};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount = 1;
  device_info.pQueueCreateInfos = &queue_info;
  device_info.enabledExtensionCount = 4;
  device_info.ppEnabledExtensionNames = mem_exts;

  res = vkCreateDevice(vk_dev->physical_device, &device_info, NULL, &vk_dev->device);
  if (res != VK_SUCCESS){
    fprintf(stderr, "Failed to create vulkan device");
    goto error;
  }

  vkGetDeviceQueue(vk_dev->device, vk_dev->queue_family, 0, &vk_dev->queue);

error:
  return;
}

void vk_device_destroy(struct vk_device *device) {
  if (device->instance) {
    // vkDestroyInstance(device->instance, NULL);
  }

  free(device);
}

static void create_command_pool(struct vk_device *vk_dev)
{
  VkResult res;

  VkCommandPoolCreateInfo cpi = {0};
  cpi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  cpi.queueFamilyIndex = vk_dev->queue_family;

  res = vkCreateCommandPool(vk_dev->device, &cpi, NULL, &vk_dev->command_pool);

  if (res != VK_SUCCESS) {
    fprintf(stderr, "vkCreateCommandPool failed to create");
    goto error;
  }

error:
  return;
}

static void create_descriptor_pool(struct vk_device *vk_dev)
{
  VkResult res;

  VkDescriptorPoolSize pool_size = {0};
  pool_size.descriptorCount = BUFFER_QUEUE_DEPTH;
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

  VkDescriptorPoolCreateInfo dpi = {0};
  dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  dpi.maxSets = BUFFER_QUEUE_DEPTH;
  dpi.poolSizeCount = 1;
  dpi.pPoolSizes = &pool_size;

  res = vkCreateDescriptorPool(vk_dev->device, &dpi, NULL, &vk_dev->descriptor_pool);

  if (res != VK_SUCCESS) {
    fprintf(stderr, "vkCreateDescriptorPool");
    goto error;
  }

error:
  return;
}

static void create_render_pass(struct vk_device *vk_dev)
{
  VkAttachmentDescription attachment = {0};
  attachment.format = swapChainImageFormat;
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkAttachmentReference color_ref = {0};
  color_ref.attachment = 0u;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {0};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;

  VkRenderPassCreateInfo rp_info = {0};
  rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rp_info.attachmentCount = 1;
  rp_info.pAttachments = &attachment;
  rp_info.subpassCount = 1;
  rp_info.pSubpasses = &subpass;

  VkResult res = vkCreateRenderPass(vk_dev->device, &rp_info, NULL, &vk_dev->render_pass);

  if (res != VK_SUCCESS) {
    fprintf(stderr, "Failed to create vkCreateRenderPass");
    goto error;
  }

error:
  return;
}

static void create_graphics_pipeline(struct vk_device *vk_dev)
{
  VkResult res;

  VkDescriptorSetLayoutBinding binding = {0};
  binding.binding = 0;
  binding.descriptorCount = 1;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo dli = {0};
  dli.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  dli.bindingCount = 1u;
  dli.pBindings = &binding;

  res = vkCreateDescriptorSetLayout(vk_dev->device, &dli, NULL, &vk_dev->descriptor_set_layout);

  if (res != VK_SUCCESS) {
    fprintf(stderr, "vkCreateDescriptorSetLayout");
    goto error;
  }

  VkPipelineLayoutCreateInfo pli = {0};
  pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pli.setLayoutCount = 1;
  pli.pSetLayouts = &vk_dev->descriptor_set_layout;

  res = vkCreatePipelineLayout(vk_dev->device, &pli, NULL, &vk_dev->pipeline_layout);

  if (res != VK_SUCCESS) {
    fprintf(stderr, "vkCreatePipelineLayout");
    goto error;
  }

  VkShaderModule vert_module;
  VkShaderModule frag_module;

  VkShaderModuleCreateInfo si = {0};
  si.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  si.codeSize = sizeof(shader_vert_data);
  si.pCode = shader_vert_data;

  res = vkCreateShaderModule(vk_dev->device, &si, NULL, &vert_module);
  if (res != VK_SUCCESS) {
    fprintf(stderr, "Failed to create vertex shader module");
    goto error;
  }

  si.codeSize = sizeof(shader_frag_data);
  si.pCode = shader_frag_data;
  res = vkCreateShaderModule(vk_dev->device, &si, NULL, &frag_module);

  if (res != VK_SUCCESS) {
    fprintf(stderr, "Failed to create fragment shader module");
    vkDestroyShaderModule(vk_dev->device, vert_module, NULL);
    goto error;
  }

  VkPipelineShaderStageCreateInfo pipe_stages[2] = {{
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vert_module, "main", NULL
    }, {
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, frag_module, "main", NULL
    }
  };

  // info
  VkPipelineInputAssemblyStateCreateInfo assembly = {0};
  assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

  VkPipelineRasterizationStateCreateInfo rasterization = {0};
  rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization.cullMode = VK_CULL_MODE_NONE;
  rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterization.lineWidth = 1.f;

  VkPipelineColorBlendAttachmentState blend_attachment = {0};
  blend_attachment.blendEnable = false;
  blend_attachment.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT |
    VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT;

  VkPipelineColorBlendStateCreateInfo blend = {0};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1;
  blend.pAttachments = &blend_attachment;

  VkPipelineMultisampleStateCreateInfo multisample = {0};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineViewportStateCreateInfo viewport = {0};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;

  VkDynamicState dynStates[2] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamic = {0};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.pDynamicStates = dynStates;
  dynamic.dynamicStateCount = 2;

  VkPipelineVertexInputStateCreateInfo vertex = {0};
  vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkGraphicsPipelineCreateInfo pipe_info = {0};
  pipe_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipe_info.layout = vk_dev->pipeline_layout;
  pipe_info.renderPass = vk_dev->render_pass;
  pipe_info.subpass = 0;
  pipe_info.stageCount = 2;
  pipe_info.pStages = pipe_stages;

  pipe_info.pInputAssemblyState = &assembly;
  pipe_info.pRasterizationState = &rasterization;
  pipe_info.pColorBlendState = &blend;
  pipe_info.pMultisampleState = &multisample;
  pipe_info.pViewportState = &viewport;
  pipe_info.pDynamicState = &dynamic;
  pipe_info.pVertexInputState = &vertex;

  // could use a cache here for faster loading
  VkPipelineCache cache = VK_NULL_HANDLE;
  res = vkCreateGraphicsPipelines(vk_dev->device, cache, 1, &pipe_info, NULL, &vk_dev->pipeline);

  vkDestroyShaderModule(vk_dev->device, vert_module, NULL);
  vkDestroyShaderModule(vk_dev->device, frag_module, NULL);

  if (res != VK_SUCCESS) {
    fprintf(stderr, "failed to create vulkan pipeline: %d\n", res);
    goto error;
  }

error:
  return;
}

struct vk_device *vk_device_create(struct device *device)
{
  if (!device->fb_modifiers) {
    printf("Can't use vulkan since drm doesn't support modifiers\n");
    return NULL;
  }

  struct vk_device *ret = calloc(1, sizeof(*ret));
  assert(ret);

  create_instance(ret);
  pick_physical_device(device, ret);
  create_logical_device(ret);
  create_command_pool(ret);
  create_descriptor_pool(ret);
  create_render_pass(ret);
  create_graphics_pipeline(ret);

  return ret;

catch_instance:
  vk_device_destroy(ret);
  return NULL;
}
