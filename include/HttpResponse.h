#pragma once
#include <string>

// extraHeaders must be complete header lines, each ending in "\r\n".
std::string buildResponse(const std::string& , const std::string& , const std::string&, const std::string& extraHeaders = "");
