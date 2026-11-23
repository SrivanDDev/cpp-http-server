#pragma once

#include <string>

struct FileResult
{
    bool success;
    std::string content;
};

// Reads a file that lives inside public/. Paths that try to escape public/
// (for example "../README.md") are refused and reported as not found.
FileResult readFile(const std::string& );

std::string getContentType(const std::string& );
