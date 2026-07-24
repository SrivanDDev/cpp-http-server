#pragma once

#include <string>

struct FileResult
{
    bool success;
    std::string content;
};

FileResult readFile(const std::string& );

std::string getContentType(const std::string& );