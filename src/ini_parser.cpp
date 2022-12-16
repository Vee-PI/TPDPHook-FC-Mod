#include "ini_parser.h"
#include "log.h"
#include "internal_config.h"
#include <fstream>
#include <cctype>
#include <sstream>

IniFile IniFile::global;

void IniFile::parse(std::basic_istream<char>& strm)
{
    std::string section, line;

    while(std::getline(strm, line))
    {
        // strip comments
        auto pos = line.find(';');
        if(pos != std::string::npos)
            line.erase(pos);

        // strip whitespace
        for(auto it = line.begin(); it != line.end();)
        {
            if(std::isblank(*it))
                it = line.erase(it);
            else
                ++it;
        }

        // normalize to lowercase
        for(auto& it : line)
            it = (char)std::tolower(it);

        if(line.empty())
            continue;

        if(line[0] == '[')
        {
            line.erase(0, line.find_first_not_of('['));
            auto end = line.find_first_of(']');
            if(end != std::string::npos)
                line.erase(end);
            section = line;
            continue;
        }

        pos = line.find('=');

        if(pos == std::string::npos)
            continue;

        auto k = line.substr(0, pos);
        auto v = line.substr(pos + 1);

        sections_[section][k] = v;
    }
}

void IniFile::read(const std::filesystem::path& path)
{
    std::ifstream f(path);

    sections_.clear();

#ifdef USE_INTERNAL_CONFIG
    std::istringstream default_config(G_INTERNAL_CONFIG);
    parse(default_config);
#endif // USE_INTERNAL_CONFIG
    parse(f);
}

bool IniFile::get_bool(const std::string& cat, const std::string& opt, bool _default)
{
    auto& val = sections_[cat][opt];
    if(val.empty())
    {
        LOG_TRACE() << L"[CONFIG] Defaulting option: " << opt;
        val = (_default ? "true" : "false");
        return _default;
    }

    return val == "true";
}

unsigned int IniFile::get_uint(const std::string& cat, const std::string& opt, unsigned int _default)
{
    auto& val = sections_[cat][opt];
    if(val.empty())
    {
        LOG_TRACE() << L"[CONFIG] Defaulting option: " << opt;
        val = std::to_string(_default);
        return _default;
    }

    try
    {
        return std::stoul(val);
    }
    catch(...)
    {
        LOG_WARN() << L"[CONFIG] Could not read option: " << opt;
        val = std::to_string(_default);
        return _default;
    }
}

double IniFile::get_double(const std::string& cat, const std::string& opt, double _default)
{
    auto& val = sections_[cat][opt];
    if(val.empty())
    {
        LOG_TRACE() << L"[CONFIG] Defaulting option: " << opt;
        val = std::to_string(_default);
        return _default;
    }

    try
    {
        return std::stod(val);
    }
    catch(...)
    {
        LOG_WARN() << L"[CONFIG] Could not read option: " << opt;
        val = std::to_string(_default);
        return _default;
    }
}
