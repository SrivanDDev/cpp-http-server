#include "FileSystem.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

// Returns true if `path` (relative to public/) resolves to a regular file inside public/.
static bool isInsidePublic(const std::string& path, std::filesystem::path& resolved)
{
    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path root = fs::canonical("public", ec);
    if (ec)
        return false;

    // weakly_canonical resolves "..", "." and symlinks, so what we compare is
    // where the file really is on disk, not how the URL spelled it.
    resolved = fs::weakly_canonical(root / path, ec);
    if (ec)
        return false;

    // The resolved path must start with every component of the root path.
    auto rootIt = root.begin();
    auto pathIt = resolved.begin();
    for (; rootIt != root.end(); ++rootIt, ++pathIt)
    {
        if (pathIt == resolved.end() || *rootIt != *pathIt)
            return false;
    }
    return fs::is_regular_file(resolved, ec);
}

FileResult readFile(const std::string& path){
    FileResult result{false,""};
    std::filesystem::path resolved;
    if (path.find('\0') != std::string::npos || !isInsidePublic(path, resolved))
    {
        std::cerr << "[-] Refused or missing file: " << path << '\n';
        return result;
    }
    // Binary mode so images and other non-text files are sent byte-for-byte.
    std::ifstream file(resolved, std::ios::binary);
    if (!file)
    {
        std::cerr << "[-] Failed to open file: " << path << '\n';
        return result;
    }
    result.success=true;
    std::stringstream ss;
    ss<<file.rdbuf();
    result.content = ss.str();
    return result;
}

std::string getContentType(const std::string& fileName){
    size_t dot = fileName.find_last_of(".");
    if (dot == std::string::npos)
    {
        return "application/octet-stream";
    }
    std::string type = fileName.substr(dot+1);
    static const std::unordered_map<std::string, std::string> mimeTypes ={
        {"html", "text/html"},
        {"css",  "text/css"},
        {"js",   "application/javascript"},
        {"png",  "image/png"},
        {"jpg",  "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif",  "image/gif"},
        {"svg",  "image/svg+xml"},
        {"ico",  "image/x-icon"},
        {"txt",  "text/plain"},
        {"json", "application/json"}
    };
    auto it = mimeTypes.find(type);
    if (it != mimeTypes.end())
    {
        return it->second;
    }
    return "application/octet-stream";
}
