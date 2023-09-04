#pragma once
#include <string>

namespace SharedLib
{
    bool IsFile(const std::string& pathName);

    bool IsAbsolutePath(const std::string& pathName);

    bool GetAbsolutePathName(const std::string& pathName, std::string& output); // The input can be a path name of a file or a directory. The output will be the abosolute path.

    void CleanOrCreateDir(const std::string& dir); // The input should be an absolute path.
}