
#ifndef _themechange_h_
#define _themechange_h_

#include <QtCore>

//#define THEMES_DEBUG_MODE //风格调试模式(读取themes下的本地文件)

/**
 * @brief ThemeChange 皮肤/主题切换工具类
 *
 * 提供应用主题（QSS皮肤）切换、主题信息管理、字体加载等功能。
 * 支持从资源文件或本地目录加载QSS，便于动态换肤和主题扩展。
 *
 * 主要功能：
 *  - 初始化主题环境
 *  - 获取所有可用主题信息
 *  - 加载指定主题（QSS）
 *  - 支持本地/资源QSS加载、字体自动加载
 *
 * 线程安全：仅主线程调用
 * 平台兼容：跨平台Qt实现
 */
class ThemeChange {
public:
    /**
     * @brief 主题信息结构体
     * @param style 主题名
     * @param desc 主题描述
     */
    struct ThemeInfo {
        ThemeInfo() {}
        ThemeInfo(const QString& s, const QString& d)
            : style(s)
            , desc(d)
        {
        }
        QString style; ///< 风格名/主题名
        QString desc;  ///< 风格描述
    };
    typedef QList<ThemeInfo> ThemeInfoList; ///< 主题信息列表

public:
    /**
     * @brief 初始化主题环境（字体、默认主题等）
     */
    static void init();

    /**
     * @brief 获取所有可用主题信息
     * @return 主题信息列表
     */
    static ThemeInfoList styles();

    /**
     * @brief 加载指定主题（QSS）
     * @param themename 主题名
     */
    static void loadTheme(const QString& themename);

    /**
     * @brief 从本地目录加载QSS内容
     * @param dir 目录路径
     * @return QSS内容
     */
    static QString loadQSSContentFromDir(const QString& dir);

private:
    /**
     * @brief 从资源文件加载QSS内容
     * @param stylename 主题名
     * @return QSS内容
     */
    static QString loadQSSContentFromQRC(const QString& stylename);

    /**
     * @brief 读取QSS文件内容
     * @param qssfile 文件路径
     * @return QSS内容
     */
    static QString readStyleFile(const QString& qssfile);

    /**
     * @brief 加载主题所需字体
     */
    static void loadFonts();
};

#endif // _themechange_h_
