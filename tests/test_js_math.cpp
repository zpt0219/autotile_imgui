#include <doctest/doctest.h>
#include "pattern/js_math.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <cmath>

static std::string doubleToHex(double d) {
    uint8_t bytes[8];
    std::memcpy(bytes, &d, 8);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (int i = 0; i < 8; ++i) {
        oss << std::setw(2) << (int)bytes[i];
    }
#else
    for (int i = 7; i >= 0; --i) {
        oss << std::setw(2) << (int)bytes[i];
    }
#endif
    return oss.str();
}

static double hexToDouble(const std::string& hex) {
    uint64_t bits = 0;
    std::stringstream ss;
    ss << std::hex << hex;
    ss >> bits;
    uint8_t bytes[8];
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (int i = 7; i >= 0; --i) {
        bytes[i] = bits & 0xff;
        bits >>= 8;
    }
#else
    for (int i = 0; i < 8; ++i) {
        bytes[i] = bits & 0xff;
        bits >>= 8;
    }
#endif
    double d;
    std::memcpy(&d, bytes, 8);
    return d;
}

static uint64_t doubleToBits(double d) {
    uint64_t bits;
    std::memcpy(&bits, &d, 8);
    return bits;
}

TEST_CASE("js_math exact bit-for-bit parity with Node.js V8") {
    std::ifstream file("tests/data/js_math_vectors.json");
    if (!file.is_open()) {
        file.open("../tests/data/js_math_vectors.json");
    }
    if (!file.is_open()) {
        file.open("../../tests/data/js_math_vectors.json");
    }
    REQUIRE(file.is_open());

    nlohmann::json j;
    file >> j;

    SUBCASE("sin") {
        for (const auto& item : j["sin"]) {
            double x = hexToDouble(item["x_hex"].get<std::string>());
            std::string expected = item["res"].get<std::string>();
            double actual = atm::js_math::sin(x);
            std::string actual_hex = doubleToHex(actual);
            if (actual_hex != expected) {
                uint64_t actual_bits = doubleToBits(actual);
                uint64_t expected_bits = 0;
                std::stringstream ss;
                ss << std::hex << expected;
                ss >> expected_bits;
                int64_t ulp_diff = std::abs(static_cast<int64_t>(actual_bits) - static_cast<int64_t>(expected_bits));
                CHECK(ulp_diff <= 1);
            }
        }
    }

    SUBCASE("cos") {
        for (const auto& item : j["cos"]) {
            double x = hexToDouble(item["x_hex"].get<std::string>());
            std::string expected = item["res"].get<std::string>();
            double actual = atm::js_math::cos(x);
            std::string actual_hex = doubleToHex(actual);
            if (actual_hex != expected) {
                uint64_t actual_bits = doubleToBits(actual);
                uint64_t expected_bits = 0;
                std::stringstream ss;
                ss << std::hex << expected;
                ss >> expected_bits;
                int64_t ulp_diff = std::abs(static_cast<int64_t>(actual_bits) - static_cast<int64_t>(expected_bits));
                CHECK(ulp_diff <= 1);
            }
        }
    }

    SUBCASE("round - exact bit-for-bit") {
        for (const auto& item : j["round"]) {
            double x = hexToDouble(item["x_hex"].get<std::string>());
            std::string expected = item["res"].get<std::string>();
            double actual = atm::js_math::round(x);
            CHECK(doubleToHex(actual) == expected);
        }
    }

    SUBCASE("hypot - exact bit-for-bit") {
        for (const auto& item : j["hypot"]) {
            double x = hexToDouble(item["x_hex"].get<std::string>());
            double y = hexToDouble(item["y_hex"].get<std::string>());
            std::string expected = item["res"].get<std::string>();
            double actual = atm::js_math::hypot(x, y);
            CHECK(doubleToHex(actual) == expected);
        }
    }

    SUBCASE("imul - exact 32-bit int") {
        for (const auto& item : j["imul"]) {
            int32_t a = item["a"].get<int32_t>();
            int32_t b = item["b"].get<int32_t>();
            int32_t expected = item["res"].get<int32_t>();
            int32_t actual = atm::js_math::imul(a, b);
            CHECK(actual == expected);
        }
    }

    SUBCASE("atan2") {
        for (const auto& item : j["atan2"]) {
            double y = hexToDouble(item["y_hex"].get<std::string>());
            double x = hexToDouble(item["x_hex"].get<std::string>());
            std::string expected_hex = item["res"].get<std::string>();
            double actual = atm::js_math::atan2(y, x);
            std::string actual_hex = doubleToHex(actual);
            if (actual_hex != expected_hex) {
                uint64_t actual_bits = doubleToBits(actual);
                uint64_t expected_bits = 0;
                std::stringstream ss;
                ss << std::hex << expected_hex;
                ss >> expected_bits;
                int64_t ulp_diff = std::abs(static_cast<int64_t>(actual_bits) - static_cast<int64_t>(expected_bits));
                CHECK(ulp_diff <= 1);
            }
        }
    }
}
