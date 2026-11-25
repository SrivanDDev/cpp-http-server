#pragma once

#include <string>

struct FileResult
{
    bool success;
    std::string content;
};

// Reads a file inside docRoot. Paths that escape the root are reported as not found.
FileResult readFile(const std::string &docRoot, const std::string &path);
std::string getContentType(const std::string &fileName);
