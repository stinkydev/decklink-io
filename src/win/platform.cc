#include "../decklink/platform.h"
#include <string>
#include <memory>

inline std::string ws_to_utf8(const wchar_t* utf16)
{
	if (!utf16) {
		return std::string();
	}

	auto const cch = static_cast<int>(wcslen(utf16));
	std::shared_ptr<char> utf8;
	auto const cb = WideCharToMultiByte(CP_UTF8, 0, utf16, cch,
		nullptr, 0, nullptr, nullptr);
	if (cb > 0)
	{
		utf8 = std::shared_ptr<char>(reinterpret_cast<char*>(malloc(static_cast<size_t>(cb) + 1)), free);
		WideCharToMultiByte(CP_UTF8, 0, utf16, cch, utf8.get(), cb, nullptr, nullptr);
		*(utf8.get() + cch) = '\0';
	}
	if (!utf8) {
		return std::string();
	}
	return std::string(utf8.get(), cb);
}

IDeckLinkDiscovery *create_decklink_discovery_instance(void) {
	IDeckLinkDiscovery *instance;
	const HRESULT result = CoCreateInstance(CLSID_CDeckLinkDiscovery, nullptr, CLSCTX_ALL, IID_IDeckLinkDiscovery, (void **)&instance);
	return result == S_OK ? instance : nullptr;
}

IDeckLinkIterator *create_decklink_iterator_instance(void) {
	IDeckLinkIterator *iterator;
	const HRESULT result = CoCreateInstance(CLSID_CDeckLinkIterator, nullptr, CLSCTX_ALL, IID_IDeckLinkIterator, (void **)&iterator);
	return result == S_OK ? iterator : nullptr;
}

IDeckLinkVideoConversion *create_video_conversion_instance(void) {
	IDeckLinkVideoConversion *conversion;
	const HRESULT result = CoCreateInstance(CLSID_CDeckLinkVideoConversion, nullptr, CLSCTX_ALL, IID_IDeckLinkVideoConversion, (void **)&conversion);
	return result == S_OK ? conversion : nullptr;
}

bool decklink_string_to_std_string(decklink_string_t input, std::string &output) {
	if (input == nullptr)
		return false;

  std::string res = ws_to_utf8(input);
	output.resize(res.length());
  std::strcpy(&output[0], &res[0]);

	return true;
}
