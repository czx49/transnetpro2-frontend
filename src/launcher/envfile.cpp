/*****************************************************************//**
 * \file   envfile.cpp
 * \brief  启动器 env 文件解析
 *
 * \author zhangmg
 * \date   July 2026
 *********************************************************************/
#include "envfile.h"

#include <windows.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty()) {
        return std::wstring();
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring();
    }

    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &result[0], size);
    return result;
}

std::wstring readPathEnvironment()
{
    const DWORD bufferSize = 65535;
    std::vector<wchar_t> buffer(bufferSize);
    const DWORD length = GetEnvironmentVariableW(L"PATH", buffer.data(), bufferSize);
    if (length > 0 && length < bufferSize) {
        return std::wstring(buffer.data(), length);
    }
    return std::wstring();
}

std::wstring trim(const std::wstring& text)
{
    const wchar_t* whitespace = L" \t\r\n";
    const size_t start = text.find_first_not_of(whitespace);
    if (start == std::wstring::npos) {
        return std::wstring();
    }
    const size_t end = text.find_last_not_of(whitespace);
    return text.substr(start, end - start + 1);
}

std::wstring expandPlaceholders(const std::wstring& value,
                                const std::wstring& launcherDir,
                                const std::wstring& packageRoot,
                                const std::wstring& pathEnv)
{
    std::wstring result = value;

    const auto replaceAll = [&result](const std::wstring& token, const std::wstring& replacement) {
        size_t pos = 0;
        while ((pos = result.find(token, pos)) != std::wstring::npos) {
            result.replace(pos, token.size(), replacement);
            pos += replacement.size();
        }
    };

    replaceAll(L"%LAUNCHER_DIR%", launcherDir);
    replaceAll(L"%PACKAGE_ROOT%", packageRoot);
    replaceAll(L"%PATH%", pathEnv);
    return result;
}

bool fileExists(const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool readUtf8File(const std::wstring& filePath, std::string& content, std::wstring& errorMessage)
{
    std::ifstream input(filePath, std::ios::binary);
    if (!input) {
        errorMessage = L"无法打开 env 文件：\n" + filePath;
        return false;
    }

    content.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (content.size() >= 3
        && static_cast<unsigned char>(content[0]) == 0xEF
        && static_cast<unsigned char>(content[1]) == 0xBB
        && static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }
    return true;
}

} // namespace

bool loadEnvFile(const std::wstring& filePath,
                 const std::wstring& launcherDir,
                 const std::wstring& packageRoot,
                 EnvConfig& config,
                 std::wstring& errorMessage)
{
    std::string fileContent;
    if (!readUtf8File(filePath, fileContent, errorMessage)) {
        return false;
    }

    config = EnvConfig();
    const std::wstring pathEnv = readPathEnvironment();

    std::istringstream stream(fileContent);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const std::wstring wideLine = trim(utf8ToWide(line));
        if (wideLine.empty() || wideLine[0] == L'#') {
            continue;
        }

        const size_t equalPos = wideLine.find(L'=');
        if (equalPos == std::wstring::npos) {
            errorMessage = L"env 文件格式错误（缺少 =）：\n" + wideLine;
            return false;
        }

        const std::wstring key = trim(wideLine.substr(0, equalPos));
        const std::wstring value = expandPlaceholders(trim(wideLine.substr(equalPos + 1)),
                                                      launcherDir,
                                                      packageRoot,
                                                      pathEnv);
        if (key.empty()) {
            continue;
        }

        if (key == L"TARGET_EXE") {
            config.targetExe = value;
        } else if (key == L"WORKING_DIR") {
            config.workingDir = value;
        } else {
            config.variables.emplace_back(key, value);
        }
    }

    if (config.targetExe.empty()) {
        errorMessage = L"env 文件缺少 TARGET_EXE 配置";
        return false;
    }

    if (config.workingDir.empty()) {
        config.workingDir = launcherDir;
    }

    return true;
}

std::wstring resolveTargetExePath(const std::wstring& launcherDir, const std::wstring& targetExeName)
{
    std::wstring primaryPath = launcherDir + L"\\" + targetExeName;
    if (fileExists(primaryPath)) {
        return primaryPath;
    }

    const size_t dotPos = targetExeName.rfind(L'.');
    if (dotPos != std::wstring::npos) {
        const std::wstring debugName = targetExeName.substr(0, dotPos) + L"d" + targetExeName.substr(dotPos);
        const std::wstring debugPath = launcherDir + L"\\" + debugName;
        if (fileExists(debugPath)) {
            return debugPath;
        }
    }

    return primaryPath;
}
