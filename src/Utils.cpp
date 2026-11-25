#include "Utils.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

std::string trim(std::string str)
{
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(" \r\n\t");
    return str.substr(first, last - first + 1);
}
std::string toLower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}
std::vector<std::string> split(const std::string &str, char delimiter)
{
    std::vector<std::string> result;
    std::string token;
    std::stringstream ss(str);
    while (std::getline(ss, token, delimiter))
    {
        result.push_back(token);
    }
    return result;
}

static int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool urlDecode(const std::string &input, std::string &output, bool plusAsSpace)
{
    output.clear();
    for (size_t i = 0; i < input.size(); ++i)
    {
        char c = input[i];
        if (c == '%')
        {
            if (i + 2 >= input.size())
                return false;
            int high = hexValue(input[i + 1]);
            int low = hexValue(input[i + 2]);
            if (high < 0 || low < 0)
                return false;
            output.push_back(static_cast<char>(high * 16 + low));
            i += 2;
        }
        else if (c == '+' && plusAsSpace)
        {
            output.push_back(' ');
        }
        else
        {
            output.push_back(c);
        }
    }
    return true;
}
