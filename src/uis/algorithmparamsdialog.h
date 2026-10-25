/*****************************************************************//**
 * \file   algorithmparamsdialog.h
 * \brief  基于 algorithmConfig.json 动态生成交通分配等算法参数录入窗体
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef ALGORITHMPARAMSDIALOG_H
#define ALGORITHMPARAMSDIALOG_H

#include "framelessdialog.h"

#include <QHash>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class AlgorithmParamsDialog : public FramelessDialog {
    Q_OBJECT

public:
    explicit AlgorithmParamsDialog(QWidget* parent = nullptr);

    /**
     * \brief 载入指定 algorithmName 的参数模板（资源 :/config/algorithmConfig.json）
     */
    bool loadAlgorithm(const QString& algorithmName);

    /** \brief 当前表单参数对象（字段名为 key） */
    QVariantMap parameters() const;

    /** \brief 供 query 参数 `json` 使用的紧凑 JSON 字符串 */
    QString parametersJsonString() const;

private:
    void clearForm();
    bool buildForm(const class AlgorithmNameJson& alg);

private:
    QString m_algorithmName;
    QHash<QString, QWidget*> m_fieldEditors;
};

#endif // ALGORITHMPARAMSDIALOG_H
