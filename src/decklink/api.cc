#include "api.h"

#include "decklink-device-mgr.h"

// ----------------------------------------------------------------------------
namespace sesame_decklink {
namespace api {

static DeviceManager * mgr = nullptr;

bool set_paired_connectors(const int device_index, const bool enable) {
  if (mgr == nullptr) {
    return false;
  }

  return mgr->set_device_profile(device_index, enable ? bmdProfileOneSubDeviceFullDuplex : bmdProfileTwoSubDevicesHalfDuplex);
}

bool initialize() {
  decklink_platform_init();
  mgr = new DeviceManager();
  mgr->refresh_devices();

  return true;
}

bool deinitialize() {
  delete mgr;

  decklink_platform_deinit();

  return true;
}

std::vector<DecklinkDeviceInfo> get_devices() {
  if (mgr == nullptr) {
    return {};
  }

  return mgr->get_devices();
}

DecklinkInput* get_input_device(const int device_index, IDeckLinkMemoryAllocator* allocator, const int64_t group) {
  if (mgr == nullptr) {
    return nullptr;
  }

  try {
    return mgr->get_input_device(device_index, allocator, group);
  } catch (std::exception&) {
    return nullptr;
  }
}

DecklinkOutput* get_output_device(const int device_index, IDeckLinkMemoryAllocator* allocator) {
  if (mgr == nullptr) {
    return nullptr;
  }

  try {
    return mgr->get_output_device(device_index, allocator);
  } catch (std::exception&) {
    return nullptr;
  }
}

void release_output_device(DecklinkOutput* output) {
  if (mgr == nullptr) {
    return;
  }

  mgr->release_output_device(output);
}


}  // namespace api
}  // namespace sesame_decklink
