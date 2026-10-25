/*****************************************************************//**
 * \file   tnp2_launcher.cpp
 * \brief  TransNetPro2 启动器入口
 * \details 读取 tnp2.env 配置环境变量并启动 tnp2_app.exe
 *
 * \author zhangmg
 * \date   July 2026
 *********************************************************************/
#include "envfile.h"

#include <windows.h>

#include <string>
#include <vector>

namespace {

std::wstring getParentDirectory(const std::wstring& path)
{
    const size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return path;
    }
    return path.substr(0, pos);
}

std::wstring formatWin32Error(DWORD errorCode)
{
    wchar_t* messageBuffer = nullptr;
    const DWORD size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                          | FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr,
                                      errorCode,
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<LPWSTR>(&messageBuffer),
                                      0,
                                      nullptr);
    std::wstring message;
    if (size > 0 && messageBuffer != nullptr) {
        message.assign(messageBuffer, size);
        LocalFree(messageBuffer);
    } else {
        message = L"未知错误";
    }
    return message;
}

void showError(const std::wstring& message)
{
    MessageBoxW(nullptr, message.c_str(), L"TransNetPro2 Launcher", MB_ICONERROR);
}

} // namespace

int WINAPI wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, PWSTR pCmdLine, int nCmdShow)
{
    wchar_t modulePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) {
        showError(L"无法获取启动器路径");
        return 1;
    }

    const std::wstring launcherDir = getParentDirectory(modulePath);
    const std::wstring packageRoot = getParentDirectory(launcherDir);
    const std::wstring envFilePath = launcherDir + L"\\tnp2.env";

    EnvConfig config;
    std::wstring errorMessage;
    if (!loadEnvFile(envFilePath, launcherDir, packageRoot, config, errorMessage)) {
        showError(errorMessage);
        return 1;
    }

    for (const auto& item : config.variables) {
        if (!SetEnvironmentVariableW(item.first.c_str(), item.second.c_str())) {
            showError(L"设置环境变量失败：\n" + item.first + L"\n" + formatWin32Error(GetLastError()));
            return 1;
        }
    }

    if (!SetCurrentDirectoryW(config.workingDir.c_str())) {
        showError(L"切换工作目录失败：\n" + config.workingDir + L"\n" + formatWin32Error(GetLastError()));
        return 1;
    }

    const std::wstring targetExe = resolveTargetExePath(launcherDir, config.targetExe);
    if (GetFileAttributesW(targetExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        showError(L"找不到目标程序：\n" + targetExe);
        return 1;
    }

    std::wstring cmdLine = L"\"" + targetExe + L"\"";
    if (pCmdLine != nullptr && *pCmdLine != L'\0') {
        cmdLine += L" ";
        cmdLine += pCmdLine;
    }

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = static_cast<WORD>(nCmdShow);

    PROCESS_INFORMATION processInfo = {};
    std::vector<wchar_t> cmdLineBuffer(cmdLine.begin(), cmdLine.end());
    cmdLineBuffer.push_back(L'\0');

    if (!CreateProcessW(nullptr,
                        cmdLineBuffer.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        0,
                        nullptr,
                        config.workingDir.c_str(),
                        &startupInfo,
                        &processInfo)) {
        showError(L"启动程序失败：\n" + targetExe + L"\n" + formatWin32Error(GetLastError()));
        return 1;
    }

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    return 0;
}
