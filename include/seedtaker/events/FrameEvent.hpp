#pragma once

#include <seedtaker/events/Event.hpp>

namespace seedtaker::events {

struct FrameEvent {
    static constexpr EventType type = EventType::Frame;
};

}
