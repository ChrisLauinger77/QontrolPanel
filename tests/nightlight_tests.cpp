#include "nightlightdata.h"
#include <iostream>
#include <cstdlib>

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::cerr << "Failed at line " << __LINE__ << '\n';                                                        \
            return EXIT_FAILURE;                                                                                       \
        }                                                                                                              \
    } while (false)
int main()
{
    for (size_t length = 0; length < 1024; ++length)
    {
        const std::vector<uint8_t> malformed(length, 0xff);
        CHECK(!NightLightData::enabled(malformed).has_value());
        CHECK(!NightLightData::toggle(malformed).has_value());
    }
    std::vector<uint8_t> off(41, 0);
    off[0] = 2;
    off[18] = 0x13;
    off[22] = 0xab; // Opaque bytes must survive the edit.
    off[10] = 0xff;
    const auto on = NightLightData::toggle(off);
    CHECK(on && on->size() == 43);
    CHECK(NightLightData::enabled(*on) == true);
    CHECK((*on)[22] == 0xab && (*on)[10] == 0 && (*on)[11] == 1);
    const auto again = NightLightData::toggle(*on);
    CHECK(again && NightLightData::enabled(*again) == false);
    for (size_t i = 0; i < off.size(); ++i)
    {
        if (i < 10 || i >= 15)
            CHECK((*again)[i] == off[i]);
    }
    auto invalid = *on;
    invalid[23] = 0xff;
    CHECK(!NightLightData::toggle(invalid));
    off.push_back(0);
    CHECK(!NightLightData::toggle(off));
    return EXIT_SUCCESS;
}
