#include "Server.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Invalid server configurations fail without binding")
{
    Server invalidHost({"invalid", 0, TEST_DOC_ROOT});
    CHECK_FALSE(invalidHost.start());
    Server negativePort({"127.0.0.1", -1, TEST_DOC_ROOT});
    CHECK_FALSE(negativePort.start());
    Server oversizedPort({"127.0.0.1", 65536, TEST_DOC_ROOT});
    CHECK_FALSE(oversizedPort.start());
}
