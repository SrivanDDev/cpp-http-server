#include "Server.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("The OS can assign a free server port")
{
    Server server({"127.0.0.1", 0, TEST_DOC_ROOT});
    REQUIRE(server.port() == 0);
    REQUIRE(server.start());
    CHECK(server.port() > 0);
    CHECK(server.port() <= 65535);
    CHECK_FALSE(server.start());
    server.stop();
    server.stop(); // Shutdown is idempotent.
}

TEST_CASE("A listening port cannot be bound by a second server")
{
    Server first({"127.0.0.1", 0, TEST_DOC_ROOT});
    REQUIRE(first.start());
    Server second({"127.0.0.1", first.port(), TEST_DOC_ROOT});
    CHECK_FALSE(second.start());
}
