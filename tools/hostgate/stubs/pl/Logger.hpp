#pragma once
// Host-gate stub. Mirrors the surfaces Seed Taker uses (F-002/F-056).
// Never shipped; syntax checking only.
#include <string>
#include <string_view>
#include <utility>

namespace pl::log {
class Logger {
public:
    static Logger& getOrCreate(std::string_view name) {
        static Logger instance;
        (void)name;
        return instance;
    }
    template <class... Args>
    void info(std::string_view, Args&&...) {}
    template <class... Args>
    void warn(std::string_view, Args&&...) {}
    template <class... Args>
    void error(std::string_view, Args&&...) {}
    template <class... Args>
    void debug(std::string_view, Args&&...) {}
};
}
