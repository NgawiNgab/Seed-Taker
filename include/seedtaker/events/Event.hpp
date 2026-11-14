#pragma once

// Event taxonomy. Adapted from BedrockTools (reference:
// BedrockTools/include/bedrocktools/events/Event.hpp). Only Frame is defined:
// new event types are added together with their producers, never ahead of
// binary evidence.

#include <cstdint>

namespace seedtaker::events {

enum class EventType : std::uint32_t {
    Frame = 1,
};

enum class EventPriority : std::int32_t {
    First = 200,
    Early = 100,
    Normal = 0,
    Late = -100,
    Last = -200
};

class Cancellable {
public:
    bool cancelled() const { return mCancelled; }
    void cancel(bool value = true) { mCancelled = value; }

private:
    bool mCancelled = false;
};

}
