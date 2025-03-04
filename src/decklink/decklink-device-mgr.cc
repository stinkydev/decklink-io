#include "decklink-device-mgr.h"
#include "decklink-device-instance.h"
#include "decklink-device.h"

namespace sesame_decklink {

void DeviceManager::refresh_devices() {
  const auto devs = enumerator.get_devices();

  int index = 0;
  devices.clear();
  for (auto dev : devs) {
    std::unique_ptr<DecklinkDevice> device = std::make_unique<DecklinkDevice>(dev, index++);
    devices.push_back(std::move(device));
  }
}

void DeviceManager::initialize_devices() {
  for (auto &dev : devices) {
    dev->initialize();
    update_peers_for_device(dev.get());
  }
}

void DeviceManager::update_peers_for_device(DecklinkDevice* device) {
  device->info.peer_devices.clear();
  const auto peers = device->get_peer_devices();
  for (auto peer : peers) {
    for (auto &dev : devices) {
      const auto &info = dev->get_info();
      if (info.displayName == peer) {
        device->info.peer_devices.push_back(dev->get_info().index);
      }
    }
  }
}

std::vector<DecklinkDeviceInfo> DeviceManager::get_devices() {
  std::vector<DecklinkDeviceInfo> result;
  for (auto &dev : devices) {
    const auto info = dev->get_info();
    result.push_back(info);
  }
  return result;
}

std::unique_ptr<DecklinkInput> DeviceManager::get_input_device(const int device_index, IDeckLinkMemoryAllocator* allocator, const int64_t group) {
  if (device_index < 0 || device_index >= devices.size()) {
    return nullptr;
  }

  return std::make_unique<DecklinkInput>(devices[device_index].get(), allocator, group);
}

bool DeviceManager::set_device_profile(const int device_index, const BMDProfileID profile_id) {
  if (device_index < 0 || device_index >= devices.size()) {
    return false;
  }

  auto res = devices[device_index]->set_profile(profile_id);
  if (!res) {
    return false;
  }

  initialize_devices();

  return res;
}

}