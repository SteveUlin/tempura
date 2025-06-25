#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstring>
#include <print>
#include <vector>

class HelloTriangleApplication {
 public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

 private:
  struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;

    auto isComplete() const -> bool {
      return graphics_family.has_value();
    }
  };

  void initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window_ =
        glfwCreateWindow(width_, height_, "Hello Triangle", nullptr, nullptr);
  }

  void initVulkan() {
    createInstance();
    pickPhysicalDevice();
  }

  void pickPhysicalDevice() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

    if (device_count == 0) {
      throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

    for (const auto& device : devices) {
      if (isDeviceSuitable(device)) {
        physical_device_ = device;
        break;
      }
    }
    if (physical_device_ == VK_NULL_HANDLE) {
      throw std::runtime_error("failed to find a suitable GPU!");
    }
  }

  static auto isDeviceSuitable(VkPhysicalDevice device) -> bool {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(device, &device_properties);

    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);

    QueueFamilyIndices indices = findQueueFamilies(device);
    return indices.isComplete();
  }

  static auto checkValidationLayerSupport() -> bool {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* layer_name : validation_layers_) {
      bool layer_found = false;
      for (const auto& layer_properties : available_layers) {
        if (strcmp(layer_name, layer_properties.layerName) == 0) {
          layer_found = true;
          break;
        }
      }
      if (!layer_found) {
        return false;
      }
    }
    return true;
  }

  static auto findQueueFamilies(VkPhysicalDevice device)
      -> QueueFamilyIndices {

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             queue_families.data());

    QueueFamilyIndices indices;
    const auto itr = std::ranges::find_if(
        queue_families, [](const VkQueueFamilyProperties& qfp) {
          return qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        });
    if (itr != queue_families.end()) {
      indices.graphics_family = static_cast<uint32_t>(
          std::distance(queue_families.begin(), itr));
    }
    return indices;
  }

  void createInstance() {
    if (!checkValidationLayerSupport()) {
      throw std::runtime_error(
          "validation layers requested, but not available!");
    }

    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = {},
        .pApplicationInfo = &app_info,
        .enabledLayerCount = validation_layers_.size(),
        .ppEnabledLayerNames = validation_layers_.data(),
        .enabledExtensionCount = glfw_extension_count,
        .ppEnabledExtensionNames = glfw_extensions,
    };

    if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
      throw std::runtime_error("failed to create instance!");
    }
  }

  void mainLoop() {
    while (glfwWindowShouldClose(window_) == 0) {
      glfwPollEvents();
    }
  }

  void cleanup() {
    vkDestroyInstance(instance_, nullptr);
    glfwDestroyWindow(window_);
    glfwTerminate();
  }

  static constexpr std::array<const char*, 1> validation_layers_ = {
      "VK_LAYER_KHRONOS_validation",
  };

  int width_ = 800;
  int height_ = 600;
  GLFWwindow* window_;
  VkInstance instance_;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
};

auto main() -> int {
  std::println("Hello, Triangle!");
  HelloTriangleApplication{}.run();
}
