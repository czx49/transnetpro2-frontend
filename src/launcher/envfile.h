#ifndef ENVFILE_H
#define ENVFILE_H

#include <string>
#include <utility>
#include <vector>

/**
 * \brief  启动器 env 文件解析结果
 */
struct EnvConfig
{
    std::wstring targetExe;
    std::wstring workingDir;
    std::vector<std::pair<std::wstring, std::wstring>> variables;
};

/**
 * \brief  从 env 文件加载配置并展开占位符
 *
 * \param  filePath      env 文件完整路径
 * \param  launcherDir   启动器所在目录（%LAUNCHER_DIR%）
 * \param  packageRoot   包根目录（%PACKAGE_ROOT%）
 * \param  config        输出解析结果
 * \param  errorMessage  失败时的错误信息
 * \return 成功返回 true
 */
bool loadEnvFile(const std::wstring& filePath,
                 const std::wstring& launcherDir,
                 const std::wstring& packageRoot,
                 EnvConfig& config,
                 std::wstring& errorMessage);

/**
 * \brief  解析目标 exe 路径，不存在时尝试 Debug 后缀（tnp2_app.exe -> tnp2_appd.exe）
 */
std::wstring resolveTargetExePath(const std::wstring& launcherDir, const std::wstring& targetExeName);

#endif // ENVFILE_H
