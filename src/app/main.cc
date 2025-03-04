#include <iostream>
#include <chrono>
#include <decklink/api.h>
#include <decklink/decklink-types.h>

volatile bool stop = false;

int main() {
  sesame_decklink::api::initialize();
  const int num_devices = (int)sesame_decklink::api::get_devices().size();
  if (num_devices == 0) {
    std::cout << "No devices found" << std::endl;
    return 1;
  }

  for (int i = 0; i < num_devices; i++) {
    const auto res = sesame_decklink::api::set_paired_connectors(i, false);
    if (!res) {
      std::cout << "Failed to set paired connectors" << std::endl;
    }
  }

  const auto devices = sesame_decklink::api::get_devices();
  for (const auto& device : devices) {
    std::cout << "Device: " << device.displayName << std::endl;
    std::cout << "Model: " << device.model << std::endl;
    std::cout << "Profile ID: " << device.profile_id << std::endl;
    std::cout << "Index: " << device.index << std::endl;
    std::cout << "Device ID: " << device.device_id << std::endl;
    std::cout << "Sub devices: " << device.sub_devices << std::endl;
    std::cout << "Sub device index: " << device.sub_device_index << std::endl;
    std::cout << "Max audio channels: " << device.max_audio_channels << std::endl;
    std::cout << "Supports external keying: " << device.supports_external_keying << std::endl;
    std::cout << "Peer devices: ";
    for (const auto& peer : device.peer_devices) {
      std::cout << peer << " ";
    }
    std::cout << std::endl;
  }

  auto input = sesame_decklink::api::get_input_device(0, nullptr, 0);
  if (input == nullptr) {
    std::cout << "Failed to get input device" << std::endl;
    return 1;
  }

  input->set_input_signal_changed_callback([](const bool locked) {
    std::cout << "Input signal locked: " << locked << std::endl;
  });

  input->set_event_callback([](const DecklinkEventType type) {
    if (type == DecklinkEventType::DECKLINK_EVENT_TYPE_ERROR) {
      std::cout << "Error event" << std::endl;
    } else if (type == DecklinkEventType::DECKLINK_EVENT_TYPE_STREAM_RESTART) {
      std::cout << "Stream restart event" << std::endl;
    }
  });

  if (input->start(BMDDisplayMode::bmdModeHD1080i50)) {
    std::cout << "Press any key to stop" << std::endl;
    std::cin.get();
    input->stop();
    input->Release();
    sesame_decklink::api::deinitialize();
    return 0;
  } else {
    std::cout << "Failed to start input" << std::endl;
    return 1;
  }
}
