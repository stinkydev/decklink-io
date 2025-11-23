#include "decklink-device.h"
#include "platform.h"

#include <chrono>
#include <thread>
#include <iostream>

namespace sesame_decklink {


DecklinkDevice::DecklinkDevice(IDeckLink* device, const int index) {
  this->device = device;
  this->device->AddRef();
  if (this->device->QueryInterface(IID_IDeckLinkStatus, (void**)&status) != S_OK) {
    status = nullptr;
  }
  this->info.index = index;

  initialize();
}

DecklinkDevice::~DecklinkDevice() {
  if (status != nullptr) {
    status->Release();
  }

  if (device != nullptr) {
    device->Release();
  }
}

void DecklinkDevice::initialize() {
  decklink_string_t name;
  device->GetDisplayName(&name);
  decklink_string_to_std_string(name, info.displayName);
  decklink_free_string(name);

  decklink_string_t model;
  device->GetModelName(&model);
  decklink_string_to_std_string(model, info.model);
  decklink_free_string(model);

  IDeckLinkProfileAttributes *profileAttributes;
  auto res = device->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&profileAttributes);
  if (res == S_OK) {
    int64_t value;
    profileAttributes->GetInt(BMDDeckLinkProfileID, &value);
    info.profile_id = static_cast<int>(value);

    profileAttributes->GetInt(BMDDeckLinkSubDeviceIndex, &value);
    info.device_id = static_cast<int>(value >> 32);

    profileAttributes->GetInt(BMDDeckLinkMaximumAudioChannels, &value);
    info.max_audio_channels = static_cast<int>(value);

    profileAttributes->GetInt(BMDDeckLinkNumberOfSubDevices, &value);
    info.sub_devices = static_cast<int>(value);

    profileAttributes->GetInt(BMDDeckLinkSubDeviceIndex, &value);
    info.sub_device_index = static_cast<int>(value);

    decklink_bool_t flag;
    profileAttributes->GetFlag(BMDDeckLinkSupportsExternalKeying, &flag);
    info.supports_external_keying = flag;

    profileAttributes->Release();
  }

  IDeckLinkInput* input = nullptr;
	if (device->QueryInterface(IID_IDeckLinkInput, (void **)&input) == S_OK) {
		IDeckLinkDisplayModeIterator* iterator = nullptr;
		if (input->GetDisplayModeIterator(&iterator) == S_OK) {
			IDeckLinkDisplayMode* display_mode = nullptr;

			while (iterator->Next(&display_mode) == S_OK) {
				if (display_mode == nullptr)
					continue;

				input_modes.push_back(display_mode);
			}
			iterator->Release();
		}
		input->Release();
	}
}

DecklinkDeviceInfo DecklinkDevice::get_info() {
  return info;
}

bool DecklinkDevice::wait_for_profile_id(const BMDProfileID profile_id) {
  IDeckLinkProfileAttributes *profileAttributes;
  const auto res = device->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&profileAttributes);
  if (res != S_OK) return false;

  int64_t value;
  auto start_time = std::chrono::steady_clock::now();
  bool success = false;

  while (true) {
    profileAttributes->GetInt(BMDDeckLinkProfileID, &value);
    if (value == profile_id) {
      success = true;
      break;
    } else if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(1)) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  profileAttributes->Release();
  return success;
}

bool DecklinkDevice::set_profile(const BMDProfileID profile_id) {
  IDeckLinkProfileManager *profile_manager;
  const auto res = device->QueryInterface(IID_IDeckLinkProfileManager, (void**)&profile_manager);
  if (res != S_OK) {
    return false;
  }

  IDeckLinkProfile *profile;
  const auto res2 = profile_manager->GetProfile(profile_id, &profile);
  if (res2 != S_OK) {
    profile_manager->Release();
    return false;
  }

  const auto res3 = profile->SetActive();
  profile->Release();
  profile_manager->Release();

  if (res3 != S_OK) {
    return false;
  }

  return wait_for_profile_id(profile_id);
}

std::vector<std::string> DecklinkDevice::get_peer_devices() {
  std::vector<std::string> result;
  IDeckLinkProfileIterator *iterator;
  IDeckLinkProfileManager *profile_manager;
  auto res = device->QueryInterface(IID_IDeckLinkProfileManager, (void**)&profile_manager);
  if (res != S_OK) {
    return result;
  }

  IDeckLinkProfile* profile;
  res = profile_manager->GetProfile((BMDProfileID)info.profile_id, &profile);
  if (res != S_OK) {
    profile_manager->Release();
    return result;
  }

  if (profile->GetPeers(&iterator) == S_OK) {
    IDeckLinkProfile* peer;
    while (iterator->Next(&peer) == S_OK) {
      IDeckLink* dev;
      res = peer->GetDevice(&dev);
      if (res != S_OK) {
        peer->Release();
        continue;
      }

      decklink_string_t name;
      dev->GetDisplayName(&name);
      std::string peer_name;
      decklink_string_to_std_string(name, peer_name);
      decklink_free_string(name);

      result.push_back(peer_name);
      dev->Release();
      peer->Release();
    }
    iterator->Release();
  }

  profile->Release();
  profile_manager->Release();

  return result;
}

bool DecklinkDevice::get_input_locked() {
  if (status == nullptr) {
    return false;
  }

  decklink_bool_t locked;
  status->GetFlag(bmdDeckLinkStatusVideoInputSignalLocked, &locked);
  return locked;
}

}  // namespace sesame_decklink
