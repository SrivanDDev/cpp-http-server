#pragma once
#include "Request.h"
#include <sstream>
#include <string>

Request parseRequest(const std::string& );
bool parseRequestLine(Request& , std::stringstream& );
bool parseHeaders(Request& , std::stringstream& );
bool parseQueryParameters(Request& );
