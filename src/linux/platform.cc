#include "../decklink/platform.h"
#include <string>
#include <cstdlib>

IDeckLinkIterator *create_decklink_iterator_instance(void)
{
	return CreateDeckLinkIteratorInstance();
}

IDeckLinkDiscovery *create_decklink_discovery_instance(void)
{
	return CreateDeckLinkDiscoveryInstance();
}

IDeckLinkVideoConversion *create_video_conversion_instance(void)
{
	return CreateVideoConversionInstance();
}

bool decklink_platform_init(void)
{
	// No initialization needed on Linux
	return true;
}

void decklink_platform_deinit(void)
{
	// No cleanup needed on Linux
}

bool decklink_string_to_std_string(decklink_string_t input, std::string &output)
{
	if (input == nullptr)
		return false;

	output = std::string(input);

	return true;
}

void decklink_free_string(decklink_string_t input)
{
	if (input != nullptr)
		free((void *)input);
}
