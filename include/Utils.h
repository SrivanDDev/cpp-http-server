#pragma once

#include <string>
#include <vector>

std::string trim(std::string str);
std::string toLower(std::string str);
std::vector<std::string> split(const std::string &str, char delimiter);
// Decodes %XX escapes (and '+' as space when plusAsSpace is true).
// Returns false if the input contains a broken escape like "%4" or "%zz".
bool urlDecode(const std::string &input, std::string &output, bool plusAsSpace);
