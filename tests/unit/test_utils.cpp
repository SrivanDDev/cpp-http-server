#include "Utils.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("URL escapes and plus signs are decoded")
{
    std::string out;
    REQUIRE(urlDecode("a%2Fb", out, false));
    CHECK(out == "a/b");
    REQUIRE(urlDecode("a+b", out, false));
    CHECK(out == "a+b");
    REQUIRE(urlDecode("a+b", out, true));
    CHECK(out == "a b");
    CHECK(!urlDecode("%4", out, false));
}
