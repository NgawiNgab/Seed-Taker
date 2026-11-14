#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define SEEDTAKER_API __attribute__((visibility("default")))
#else
#define SEEDTAKER_API
#endif
