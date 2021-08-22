#pragma once
#include <string>

/* conversions between shift-jis and unicode */
std::wstring sjis_to_utf(const std::string& str);
std::wstring sjis_to_utf(const char *str, std::size_t sz);
std::string utf_to_sjis(const std::wstring& str);

/* UTF-8 to wchar_t */
std::wstring utf_widen(const std::string& str);

/* wchar_t to UTF-8 */
std::string utf_narrow(const std::wstring& str);

/* system default ANSI codepage to unicode */
std::wstring ansi_to_utf(const std::string& str);

/* convenience functions */
static inline std::string sjis_to_utf8(const std::string& str) { return utf_narrow(sjis_to_utf(str)); }
static inline std::string sjis_to_utf8(const char *str, std::size_t sz) { return utf_narrow(sjis_to_utf(str, sz)); }
static inline std::string utf8_to_sjis(const std::string& str) { return utf_to_sjis(utf_widen(str)); }
