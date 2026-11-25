#pragma once

#include "Request.h"

#include <sstream>
#include <string>

Request parseRequest(const std::string &rawRequest);
bool parseRequestLine(Request &request, std::stringstream &ss);
bool parseHeaders(Request &request, std::stringstream &ss);
bool parseQueryParameters(Request &request);
