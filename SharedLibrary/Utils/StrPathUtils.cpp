#include "StrPathUtils.h"
#include <vector>
#include <filesystem>
#include <algorithm>

namespace SharedLib
{
    bool IsFile(const std::string& pathName)
    {
        size_t found = pathName.find_last_of(".");
        if (found != std::string::npos)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    bool IsAbsolutePath(const std::string& pathName)
    {
        size_t found = pathName.find_first_of(":");
        if (found != std::string::npos)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    void CleanOrCreateDir(
        const std::string& dir)
    {
        if (std::filesystem::exists(dir))
        {
            std::filesystem::remove_all(dir);
        }

        std::filesystem::create_directory(dir);
    }

    bool GetAbsolutePathName(
        const std::string& pathName,
        std::string& output)
    {
        if (IsAbsolutePath(pathName))
        {
            output = pathName;
            return true;
        }
        else
        {
            std::filesystem::path workPath = std::filesystem::current_path();
            std::string workPathStr = workPath.string();

            size_t found = pathName.find_first_of(".");
            if (found == std::string::npos)
            {
                return false;
            }

            std::string pathNameSubStr = pathName.substr(found + 1);
            std::replace(pathNameSubStr.begin(), pathNameSubStr.end(), '/', '\\');

            output = workPathStr + pathNameSubStr;
            return true;
        }
    }

    void GetAllFileNames(
        const std::string& dir,
        std::vector<std::string>& outputVec)
    {
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            outputVec.push_back(entry.path().filename().string());
        }
    }
}