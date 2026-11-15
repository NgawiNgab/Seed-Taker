#pragma once
// Host-gate stub for signature resolution (F-002). The real preloader takes
// a vector of patterns + a library name and returns pattern -> address.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace pl::memory {
std::map<std::string, std::uintptr_t> resolveSignatures(const std::vector<std::string>& patterns,
                                                        const char* libraryName);
}
