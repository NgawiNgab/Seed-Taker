#pragma once
// Host-gate stub for nlohmann/json. Only the surface Seed Taker touches:
// contains/operator[]/get/is_number_integer/erase/assignment.
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace nlohmann {
class json {
public:
    json() = default;
    json(const char*) {}
    json(std::int64_t) {}
    json(double) {}
    json(bool) {}

    bool contains(const std::string&) const { return false; }
    void erase(const std::string&) {}

    json& operator[](const std::string&) { return *this; }
    const json& operator[](const std::string&) const { return *this; }
    json& operator[](const char*) { return *this; }

    json& operator=(const json&) = default;
    // One templated sink: real nlohmann::json accepts every scalar, and a
    // set of scalar overloads makes `j[k] = someInt` ambiguous.
    template <class T>
    json& operator=(const T&) { return *this; }

    bool is_number_integer() const { return false; }
    bool is_number_float() const { return false; }
    bool is_boolean() const { return false; }
    bool is_string() const { return false; }
    bool is_object() const { return false; }
    bool is_array() const { return false; }

    template <class T>
    T get() const { return T{}; }

    std::string dump() const { return "{}"; }

    // items(): the real iterator yields (key, value) pairs; ModuleMenu
    // structured-binds it. An empty vector keeps the loop body type-checked.
    std::vector<std::pair<std::string, json>> items() const { return {}; }
};

inline std::istream& operator>>(std::istream& in, json&) { return in; }
inline std::ostream& operator<<(std::ostream& out, const json&) { return out; }
}
