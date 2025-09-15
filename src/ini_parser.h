#pragma once
#include <filesystem>
#include <unordered_map>
#include <string>
#include <istream>
#include "typedefs.h"

// note: cannot be exported as-is because CRT is static linked
// so passing objects between modules may cause Bad Things (TM)
class IniFile
{
private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections_;

    void parse(std::basic_istream<char>& strm);

public:
    IniFile() = default;
    IniFile(const std::filesystem::path& path) { read(path); }

    void read(const std::filesystem::path& path);

    auto& operator[](const std::string& k) { return sections_[k]; }

    static IniFile global;

    bool get_bool(const std::string& cat, const std::string& opt, bool _default = false);
    unsigned int get_uint(const std::string& cat, const std::string& opt, unsigned int _default = ID_NONE);
    double get_double(const std::string& cat, const std::string& opt, double _default);
};
