#pragma once
#include "Request.h"
#include<string>

Request parseRequest(const std::string& );
bool parseRequestLine(const Request&  , std::stringstream );
bool parseHeaders(Request& , std::stringstream& );
void parseQueryParameters(Request& );