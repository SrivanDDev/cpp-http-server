#include "Utils.h"
#include<string>
#include<sstream>
#include <algorithm>
#include <cctype>

std::string trim(std::string str){
    size_t first = str.find_first_not_of(" \t\r\n");
    if(first == std::string::npos)
        return"";
    size_t last = str.find_last_not_of(" \r\n\t");
    return str.substr(first,last-first+1);
}
std::string toLower(std::string str){
    std::transform(str.begin(),str.end(),str.begin(),[](unsigned char c){return std::tolower(c);});
    return str;
}
std::vector<std::string> split(const std::string& str,char delimiter){
    std::vector<std::string> result;
    std::string token;
    std::stringstream ss(str);
    while (std::getline(ss,token,delimiter))
    {
        result.push_back(token);
    }
    return result;
}