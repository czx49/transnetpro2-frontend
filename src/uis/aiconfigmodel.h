/*****************************************************************//**
 * \file   aiconfigmodel.h
 * \brief  `/api/config/aiconfig` 多级选词配置解析
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef AICONFIGMODEL_H
#define AICONFIGMODEL_H

#include "httpapijson.h"

#include <QStringList>

enum class AiConfigLevel {
    L1 = 0,
    L2
};

struct AiConfigNode {
    int id = 0;
    QString title;
    QList<AiConfigNode> children;
};

/**
 * \brief 解析 aiconfig 树（按模块 id 过滤 L2/L3 选词），供智能对话选词条使用
 */
class AiConfigModel {
public:
    static constexpr int MotorVehicleModuleId = 1;
    static constexpr int SlowTrafficModuleId = 2;
    static constexpr int PublicTransportModuleId = 3;

    bool loadFromJson(const PropertyJson& data);
    bool isValid() const;

    void setModuleId(int moduleId);
    int moduleId() const;

    QString levelTitle(AiConfigLevel level) const;
    QStringList l1Options() const;
    QStringList l2Options(int l1Index) const;

    QString optionLabel(AiConfigLevel level, int l1Index, int l2Index) const;
    QString l2TitleAt(int l1Index, int l2Index) const;
    bool hasBoxSelectL1Option() const;
    bool isBoxSelectL1(int l1Index, int l2Index) const;
    void clampSelection(int& l1Index, int& l2Index) const;

private:
    static AiConfigNode parseNode(const QJsonObject& obj);
    static QStringList titlesFromNodes(const QList<AiConfigNode>& nodes, bool allowFallback);
    static QString fallbackLabel();
    int boundedL1Index(int l1Index) const;
    const AiConfigNode* currentModuleNode() const;
    void refreshCurrentModule();

    QList<AiConfigNode> m_moduleRoots;
    AiConfigNode m_currentModule;
    int m_moduleId = MotorVehicleModuleId;
    bool m_valid = false;
};

#endif // AICONFIGMODEL_H
