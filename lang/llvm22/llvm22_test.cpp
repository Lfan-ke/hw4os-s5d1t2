// llvm22_test.cpp — comprehensive test for the StarryOS #764 "llvm22" item.
// Compiled by clang-22 (LLVM 22.1.6) with -std=c++23, cross-linked static for 4
// arches, run on StarryOS. Exercises LLVM 22 codegen + C++23/C++20 language &
// stdlib features. Deterministic output -> exact-match a host golden.
#include <cstdio>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <optional>
#include <variant>
#include <ranges>
#include <span>
#include <bit>
#include <concepts>
#include <tuple>

// ---- C++20 concepts ----
template <typename T>
concept Addable = requires(T a, T b) { { a + b } -> std::same_as<T>; };

template <Addable T>
static T sum_all(std::span<const T> xs) {
    T acc{};
    for (auto x : xs) acc += x;
    return acc;
}

// ---- consteval (C++20 immediate function) ----
consteval int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

// ---- variant visit ----
using Val = std::variant<int, double, std::string>;
static std::string describe(const Val& v) {
    return std::visit([](auto&& x) -> std::string {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, int>)         return "int";
        else if constexpr (std::is_same_v<T, double>) return "double";
        else                                          return "string";
    }, v);
}

int main() {
    // 1) std::ranges + views (C++20): squares of evens 1..10
    auto sq = std::views::iota(1, 11)
            | std::views::filter([](int x){ return x % 2 == 0; })
            | std::views::transform([](int x){ return x * x; });
    int rsum = 0; for (int x : sq) rsum += x;     // 4+16+36+64+100 = 220
    printf("RANGES_SUM=%d\n", rsum);

    // 2) concept-constrained generic over std::span
    std::vector<int> v(100); std::iota(v.begin(), v.end(), 1);
    printf("SPAN_SUM=%d\n", sum_all<int>(v));      // 5050

    // 3) consteval fib at compile time
    constexpr int f = fib(15);
    printf("FIB15=%d\n", f);                        // 610

    // 4) std::bit (C++20): popcount / bit_width / rotl
    printf("POPCOUNT=%d\n", std::popcount(0xF0F0u));        // 8
    printf("BITWIDTH=%d\n", std::bit_width(1000u));         // 10
    printf("ROTL=%u\n", std::rotl(0x01020304u, 8));         // 0x02030401 = 33768961

    // 5) std::variant + visit
    Val a = 42, b = 3.14, c = std::string("hi");
    printf("VARIANT=%s,%s,%s\n", describe(a).c_str(), describe(b).c_str(), describe(c).c_str());

    // 6) std::optional
    std::optional<int> o = 7;
    int o2 = o.has_value() ? o.value() * 6 : -1;
    printf("OPTIONAL=%d\n", o2);                    // 42

    // 7) ranges::sort + unique on a vector
    std::vector<int> d = {5,3,3,1,4,1,5,2};
    std::ranges::sort(d);
    auto last = std::ranges::unique(d);
    d.erase(last.begin(), d.end());
    printf("UNIQUE=");
    for (size_t i = 0; i < d.size(); ++i) printf("%d%s", d[i], i+1<d.size()?",":"");
    printf("\n");                                   // 1,2,3,4,5

    // 8) structured bindings + tuple
    auto [q, r] = std::pair{100 / 7, 100 % 7};
    printf("DIVMOD=%d,%d\n", q, r);                 // 14,2

    printf("LLVM22_OK\n");
    return 0;
}
