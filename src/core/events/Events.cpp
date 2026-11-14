#include <seedtaker/events/EventBus.hpp>

namespace seedtaker::events {

EventBus& bus() {
    static EventBus instance;
    return instance;
}

}
