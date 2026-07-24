#pragma once
#include<string>
#include"Request.h"

std::string trim(std::string);
std::string toLower(std::string);
void parseQueryParameters(Request& request);