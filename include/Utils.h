#pragma once
#include <string>
#include <vector>

std::string trim(std::string);
std::string toLower(std::string);
std::vector<std::string> split(const std::string& ,char );
// Decodes %XX escapes (and '+' as space when plusAsSpace is true).
// Returns false if the input contains a broken escape like "%4" or "%zz".
bool urlDecode(const std::string& input, std::string& output, bool plusAsSpace);
