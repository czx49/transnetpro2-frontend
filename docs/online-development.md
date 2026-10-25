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
HostName=http://gisapi.wjnet.cc
```

此地址沿用原开发包配置；实际服务地址以服务方提供的信息为准。主机地址不追加 `/api`，客户端会拼接 API 路径。网络及后端服务正常、账号有效时才能完成登录和业务联调。

2026-10-25 本机检查：该地址的 `/api/config/getversion` 返回 HTTP 404，旧文档地址 `http://gisapi.jiheshop.com` 的同一接口返回 HTTP 502。联网客户端已编译并通过启动检查，后端登录与业务流程待使用有效服务地址验证。

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
