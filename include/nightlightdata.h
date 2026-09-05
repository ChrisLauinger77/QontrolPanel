#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// Only the two legacy CloudStore layouts understood by this application are
// accepted. New Windows layouts fail closed, without rewriting registry data.
namespace NightLightData
{
    inline std::optional<bool> enabled(std::span<const uint8_t> data)
    {
        if (data.size() < 4 || data[0] != 2 || data[1] || data[2] || data[3])
            return {};
        if (data.size() == 41 && data[18] == 0x13)
            return false;
        if (data.size() == 43 && data[18] == 0x15 && data[23] == 0x10 && data[24] == 0)
            return true;
        return {};
    }
    inline std::optional<std::vector<uint8_t>> toggle(std::span<const uint8_t> data)
    {
        const auto state = enabled(data);
        if (!state.has_value())
            return {};
        std::vector<uint8_t> result(data.begin(), data.end());
        if (*state)
            result.erase(result.begin() + 23, result.begin() + 25);
        else
            result.insert(result.begin() + 23, {0x10, 0});
        result[18] = *state ? 0x13 : 0x15;
        for (int i = 10; i < 15; ++i)
        {
            if (++result[i] != 0)
                break;
        }
        return result;
    }
} // namespace NightLightData
