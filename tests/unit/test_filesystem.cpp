#include "FileSystem.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Document root access and MIME types")
{
    CHECK(readFile(TEST_DOC_ROOT, "index.html").success);
    CHECK(!readFile(TEST_DOC_ROOT, "does-not-exist.html").success);
    CHECK(!readFile(TEST_DOC_ROOT, "../README.md").success);
    CHECK(!readFile(TEST_DOC_ROOT, "../../../CMakeLists.txt").success);
    CHECK(!readFile(TEST_DOC_ROOT, "../../../../etc/passwd").success);
    CHECK(!readFile(TEST_DOC_ROOT, "/etc/passwd").success); // absolute path
    CHECK(!readFile(TEST_DOC_ROOT, "").success);            // the directory itself
    CHECK(getContentType("x.html") == "text/html");
    CHECK(getContentType("noext") == "application/octet-stream");
}
