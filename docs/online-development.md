# 联网开发与 GitHub 协作

## 当前版本

本项目是 Qt5/C++ 桌面软件的二期前端，使用 HTTP API、WFS 地图服务和后端算法服务。离线版计划已停止，当前构建不包含离线请求拦截、本地免密码登录和离线数据库实现。

GitHub 保存源码和资源。Qt、QGIS、GDAL、WebView2 开发包、编译后的 EXE/DLL 和本地运行配置需在各开发机器准备。

## 在当前电脑启动

1. 进入 `transnetpro2-feature-client-dev` 文件夹。
2. 首次运行或修改源码后，双击 `编译联网版.cmd`，等待构建成功。
3. 双击同一文件夹中的 `启动软件.cmd`。
4. 使用软件后端账号登录。此账号由软件服务方提供，与 GitHub 账号独立。

外层“软件原型”文件夹的 `启动软件.cmd` 与 `启动联网版.cmd` 也已指向这个二期前端。旧离线入口已停用。

也可在 PowerShell 中运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build_online.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_online.ps1
```

生成程序为 `out/bin/tnp2_app.exe`，构建缓存为 `build-online/`。通过启动脚本运行，以正确设置 DLL、Qt 插件和 QGIS 环境。

## 联网服务器配置

首次启动时，脚本把 `config/httpserver.ini.example` 复制为本机 `out/bin/httpserver.ini`，后续保留本机配置。

```ini
[Config]
HostName=http://gisapi.jiheshop.com
```

此地址来自二期 API 文档，已验证公开接口可直接访问。主机地址不追加 `/api`，客户端会拼接 API 路径。实际登录与业务联调还需有效的软件账号。

2026-10-25 本机复核：直接访问 `http://gisapi.jiheshop.com` 的 `/api/config/getversion`、`/api/config/base` 和 `/api/config/api` 均返回 HTTP 200。版本接口的业务码为 200、版本为 2.0.0；基础信息接口业务码为 200，站点名称与本项目一致。本机代理访问该地址时曾返回 502，因此代理结果不能用于判定服务器停机。

客户端已对 `httpserver.ini` 中配置的后端主机设置直连规则，覆盖版本、登录、业务 HTTP、SSE 及同主机的 QGIS 请求；其它主机仍遵循代理配置，系统代理设置不会被修改。更换后端地址后需重启软件。

如果仍看到 `getversion` 或 `user/login` 的 Bad Gateway 弹窗，先退出旧程序，再用 `启动软件.cmd` 启动重新编译后的版本。网络检查通过后，登录仍需软件业务账号。

原开发包中的 `http://gisapi.wjnet.cc` 对版本与基础信息接口返回 HTTP 404，当前二期默认配置已改用上面的已验证地址。

如需更换服务器：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_online.ps1 -ServerUrl "https://后端服务器地址"
```

本机 `httpserver.ini` 位于被 Git 忽略的 `out/` 目录，避免把本机配置上传。

## 修改代码

使用 Visual Studio 2022 或熟悉的代码编辑器打开仓库目录。修改并保存后重新编译，再启动检查界面和操作流程。

| 内容 | 主要位置 |
|------|----------|
| 主界面、登录、页面与业务工作台 | `src/uis/` |
| 首页卡片、项目列表和新建项目 | `src/homectrls/` |
| 方案、图层树、地图工具栏和 WFS | `src/casectrls/` |
| 公共弹窗与基础控件 | `src/publicctrls/` |
| 主题、颜色和样式 | `src/themechange/`，以及各界面的样式定义 |
| 图标与资源 | `src/icons/`、`src/icons.qrc` |
| 后端接口和会话 | `src/httpapis/` |

遵守 `CLAUDE.md` 与 `docs/ui-conventions.md`；文件使用 UTF-8，源码换行使用 CRLF。

## 提交并上传

在仓库根目录打开 PowerShell。开始修改前，如果工作区干净，可先拉取：

```powershell
git pull --ff-only
```

完成修改、编译与人工检查后：

```powershell
git status
git diff
git add src/uis/loginpanel.cpp
git diff --cached
git commit -m "调整登录页面布局"
git push
```

`git add` 后填写本次实际修改的文件或目录；新文件也需加入。`git commit` 保存到本机历史，`git push` 才会上传 GitHub。文档修改无需编译。

仓库：https://github.com/czx49/transnetpro2-frontend ，当前默认分支为 `main`。

请勿提交密码、Token、`.env`、`ai_client.ini`、Mockplus 缓存、编译产物和开发依赖。`.gitignore` 已排除这些常见本地文件。

## 换电脑与排查

- 新电脑先克隆私有仓库，再安装 Visual Studio 2022 的“使用 C++ 的桌面开发”和 CMake 工具，并准备匹配的 x64 开发包。
- 开发包目录内应包含 `Qt5`、`gdal-dev`、`qgis-ltr-dev`、`WebView2`。可设置 `$env:TNP_DEPS_ROOT = 'D:\developenv'`，或为编译、启动脚本传 `-DependencyRoot`。
- QGIS Analysis 还需要匹配的 x64 `gsl.dll` 与 `cblas.dll`。当前电脑自动从原开发包的 `algorithm/ctap` 目录复制；其它机器可通过 `TNP_GSL_BIN` 指定这两个 DLL 所在目录。
- 缺少 DLL 或插件时，通过启动脚本运行，并检查开发包路径。`-CheckOnly` 可检查主要文件和后端配置。
- 登录失败时核对软件账号、后端地址和网络；启动日志位于 `out/bin/logs/`。启动成功不代表业务接口已验证。
- 当前电脑的 Git 仓库配置使用既有本地代理 `127.0.0.1:7890`。推送出现连接错误时，确认代理已开启；其它电脑按自身网络设置代理。
- GitHub 登录过期时使用 Git Credential Manager 浏览器/设备授权，无需把账号密码写进仓库。

## 网络回归检查

`tests/networkproxy` 是独立的 QtTest 项目，可使用 Qt5 的 CMake 前缀配置并运行。默认测试用本机 HTTP 服务及模拟故障代理，验证后端直连和其它主机继续使用代理。
额外设置 `TNP_LIVE_BACKEND=http://gisapi.jiheshop.com` 后，会检查公开版本、基础信息和空凭据登录的参数校验；此检查不会使用真实账号，不能替代真实登录验收。
