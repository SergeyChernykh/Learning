#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace test_harness {

struct TestCase {
    std::string name;
    void (*function)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

class Registrar {
public:
    Registrar(std::string name, void (*function)()) {
        registry().push_back({std::move(name), function});
    }
};

class Failure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[noreturn]] inline void fail(const char* expression,
                              const char* file,
                              int line,
                              std::string_view details = {}) {
    std::ostringstream message;
    message << file << ':' << line << ": check failed: " << expression;
    if (!details.empty()) {
        message << " (" << details << ')';
    }
    throw Failure(message.str());
}

template <class Left, class Right>
void requireEqual(const Left& left,
                  const Right& right,
                  const char* expression,
                  const char* file,
                  int line) {
    if (!(left == right)) {
        fail(expression, file, line);
    }
}

inline int run(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";
    int selected = 0;
    int failed = 0;

    for (const auto& test : registry()) {
        if (!filter.empty() && test.name.find(filter) == std::string::npos) {
            continue;
        }

        ++selected;
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << "\n       "
                      << error.what() << '\n';
        } catch (...) {
            ++failed;
            std::cerr << "[FAIL] " << test.name
                      << "\n       unknown exception\n";
        }
    }

    if (selected == 0) {
        std::cerr << "No tests matched filter: " << filter << '\n';
        return 2;
    }

    std::cout << "\n" << (selected - failed) << '/' << selected
              << " selected tests passed\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace test_harness

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                        \
    static void TEST_CONCAT(test_function_, __LINE__)();                       \
    static ::test_harness::Registrar TEST_CONCAT(test_registrar_, __LINE__)(   \
        name, &TEST_CONCAT(test_function_, __LINE__));                         \
    static void TEST_CONCAT(test_function_, __LINE__)()

#define REQUIRE(expression)                                                    \
    do {                                                                       \
        if (!(expression)) {                                                   \
            ::test_harness::fail(#expression, __FILE__, __LINE__);             \
        }                                                                      \
    } while (false)

#define REQUIRE_EQ(left, right)                                                \
    do {                                                                       \
        const auto& test_left = (left);                                        \
        const auto& test_right = (right);                                      \
        ::test_harness::requireEqual(test_left, test_right,                     \
                                     #left " == " #right, __FILE__, __LINE__); \
    } while (false)

#define REQUIRE_THROWS_AS(expression, exception_type)                          \
    do {                                                                       \
        bool test_caught_expected_exception = false;                           \
        try {                                                                  \
            static_cast<void>(expression);                                     \
        } catch (const exception_type&) {                                      \
            test_caught_expected_exception = true;                             \
        } catch (...) {                                                        \
            ::test_harness::fail(#expression, __FILE__, __LINE__,              \
                                 "threw a different exception type");         \
        }                                                                      \
        if (!test_caught_expected_exception) {                                 \
            ::test_harness::fail(#expression, __FILE__, __LINE__,              \
                                 "did not throw " #exception_type);            \
        }                                                                      \
    } while (false)

