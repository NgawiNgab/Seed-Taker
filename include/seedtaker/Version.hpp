#pragma once

#include <cstdint>
#include <string_view>

namespace seedtaker {

// NOTE: scripts/package_levipack.py parses the four lines below with a regex.
// Keep the exact `inline constexpr std::string_view <Key> = "<value>";` form.
inline constexpr std::string_view Name = "SeedTaker";
inline constexpr std::string_view Author = "NgawiNgab";
inline constexpr std::string_view Description = "Standalone native Levi mod that captures and displays the Minecraft Bedrock world seed.";
inline constexpr std::string_view Version = "0.1.0";
inline constexpr std::uint32_t SdkVersion = 0;
inline constexpr std::string_view RuntimeLibrary = "libSeedTaker.so";

}
