#pragma once

#include <string>

struct FileResult
{
    bool success;
    std::string content;
};

FileResult readFile(const std::string& path);

std::string getContentType(const std::string& fileName);