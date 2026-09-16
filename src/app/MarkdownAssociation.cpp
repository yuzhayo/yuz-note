#include "MarkdownAssociation.h"

#include <cstdio>
#include <cwchar>
#include <windows.h>

namespace YuzNote {
namespace {

constexpr wchar_t kProgId[] = L"Yuzhayo.YuzNote.Markdown";
constexpr wchar_t kExtMd[] = L"Software\\Classes\\.md";
constexpr wchar_t kExtMarkdown[] = L"Software\\Classes\\.markdown";
constexpr wchar_t kCommandSubkey[] =
    L"Software\\Classes\\Yuzhayo.YuzNote.Markdown\\shell\\open\\command";

bool SetDefaultValue(const wchar_t *subkey, const wchar_t *value)
{
    HKEY hkey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &hkey, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const DWORD bytes = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
    const LSTATUS st = RegSetValueExW(hkey, nullptr, 0, REG_SZ,
                                      reinterpret_cast<const BYTE *>(value), bytes);
    RegCloseKey(hkey);
    return st == ERROR_SUCCESS;
}

bool DefaultValueEquals(const wchar_t *subkey, const wchar_t *expected)
{
    HKEY hkey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subkey, 0, KEY_QUERY_VALUE, &hkey)
        != ERROR_SUCCESS) {
        return false;
    }
    wchar_t current[512] = {};
    DWORD bytes = sizeof(current);
    // Empty name = the default (unnamed) value.
    const LSTATUS st = RegQueryValueExW(hkey, L"", nullptr, nullptr,
                                        reinterpret_cast<LPBYTE>(current), &bytes);
    RegCloseKey(hkey);
    return st == ERROR_SUCCESS && wcscmp(current, expected) == 0;
}

} // namespace

void RegisterMarkdownAssociation(void *, const char *)
{
    wchar_t exe[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0) {
        return;
    }
    wchar_t command[4096] = {};
    _snwprintf_s(command, _TRUNCATE, L"\"%s\" \"%%1\"", exe);
    SetDefaultValue(kExtMd, kProgId);
    SetDefaultValue(kExtMarkdown, kProgId);
    SetDefaultValue(kCommandSubkey, command);
}

void UnregisterMarkdownAssociation(void *, const char *)
{
    RegDeleteTreeW(HKEY_CURRENT_USER,
                   L"Software\\Classes\\Yuzhayo.YuzNote.Markdown");
    // Remove our defaults only if still ours — never touch other apps' values.
    if (DefaultValueEquals(kExtMd, kProgId)) {
        RegDeleteKeyValueW(HKEY_CURRENT_USER, kExtMd, L"");
    }
    if (DefaultValueEquals(kExtMarkdown, kProgId)) {
        RegDeleteKeyValueW(HKEY_CURRENT_USER, kExtMarkdown, L"");
    }
}

} // namespace YuzNote
