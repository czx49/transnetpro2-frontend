/*****************************************************************//**
 * \file   aiconfigmodel.cpp
 * \brief  `/api/config/aiconfig` 多级选词配置解析实现
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "aiconfigmodel.h"

#include <QJsonArray>
#include <QJsonObject>

namespace {

QString defaultOptionTitle()
{
    return QString::fromUtf8(u8"默认");
}

QString boxSelectL1Label()
{
    return QString::fromUtf8(u8"框选");
}

class AiConfigPayload : public PropertyJson {
public:
    explicit AiConfigPayload(const PropertyJson& data)
        : PropertyJson(data)
    {
    }

    QJsonValue rootValue() const
    {
        return m_jsonValue;
    }
};

} // namespace

bool AiConfigModel::loadFromJson(const PropertyJson& data)
{
    m_valid = false;
    m_moduleRoots.clear();
    m_currentModule = {};
    m_moduleId = MotorVehicleModuleId;

    const AiConfigPayload payload(data);
    const QJsonValue root = payload.rootValue();
    QJsonArray modules;
    if (root.isArray()) {
        modules = root.toArray();
    } else if (root.isObject()) {
        const QJsonValue nested = root.toObject().value(QStringLiteral("data"));
        if (nested.isArray()) {
            modules = nested.toArray();
        }
    }

    if (modules.isEmpty()) {
        return false;
    }

    m_moduleRoots.reserve(modules.size());
    for (const QJsonValue& value : modules) {
        if (!value.isObject()) {
            continue;
        }
        m_moduleRoots.append(parseNode(value.toObject()));
    }

    m_valid = !m_moduleRoots.isEmpty();
    if (m_valid) {
        refreshCurrentModule();
    }
    return m_valid;
}

bool AiConfigModel::isValid() const
{
    return m_valid && !m_currentModule.children.isEmpty();
}

void AiConfigModel::setModuleId(int moduleId)
{
    if (moduleId <= 0) {
        return;
    }
    if (m_moduleId == moduleId) {
        return;
    }
    m_moduleId = moduleId;
    refreshCurrentModule();
}

int AiConfigModel::moduleId() const
{
    return m_moduleId;
}

QString AiConfigModel::levelTitle(AiConfigLevel level) const
{
    switch (level) {
    case AiConfigLevel::L1:
        return QString::fromUtf8(u8"业务对象");
    case AiConfigLevel::L2:
        return QString::fromUtf8(u8"分析对象");
    }
    return QString();
}

QStringList AiConfigModel::l1Options() const
{
    const AiConfigNode* module = currentModuleNode();
    if (!module || module->children.isEmpty()) {
        return {};
    }
    return titlesFromNodes(module->children, false);
}

QStringList AiConfigModel::l2Options(int l1Index) const
{
    const AiConfigNode* module = currentModuleNode();
    if (!module || module->children.isEmpty()) {
        return {};
    }
    const int idx = boundedL1Index(l1Index);
    return titlesFromNodes(module->children.at(idx).children, true);
}

QString AiConfigModel::optionLabel(AiConfigLevel level, int l1Index, int l2Index) const
{
    switch (level) {
    case AiConfigLevel::L1: {
        const QStringList opts = l1Options();
        if (opts.isEmpty()) {
            return fallbackLabel();
        }
        return opts.at(qBound(0, l1Index, opts.size() - 1));
    }
    case AiConfigLevel::L2: {
        const QStringList opts = l2Options(l1Index);
        if (opts.isEmpty()) {
            return fallbackLabel();
        }
        return opts.at(qBound(0, l2Index, opts.size() - 1));
    }
    }
    return fallbackLabel();
}

QString AiConfigModel::l2TitleAt(int l1Index, int l2Index) const
{
    const AiConfigNode* module = currentModuleNode();
    if (!module || module->children.isEmpty()) {
        return fallbackLabel();
    }

    const int l1 = boundedL1Index(l1Index);
    const QList<AiConfigNode>& l3Nodes = module->children.at(l1).children;
    if (l3Nodes.isEmpty()) {
        return fallbackLabel();
    }
    const int l2 = qBound(0, l2Index, l3Nodes.size() - 1);
    const QString title = l3Nodes.at(l2).title;
    return title.isEmpty() ? fallbackLabel() : title;
}

bool AiConfigModel::hasBoxSelectL1Option() const
{
    return l1Options().contains(boxSelectL1Label());
}

bool AiConfigModel::isBoxSelectL1(int l1Index, int l2Index) const
{
    if (!isValid()) {
        return false;
    }
    return optionLabel(AiConfigLevel::L1, l1Index, l2Index) == boxSelectL1Label();
}

void AiConfigModel::clampSelection(int& l1Index, int& l2Index) const
{
    if (!isValid()) {
        l1Index = 0;
        l2Index = 0;
        return;
    }

    const QStringList l1 = l1Options();
    if (l1.isEmpty()) {
        l1Index = 0;
        l2Index = 0;
        return;
    }
    l1Index = boundedL1Index(l1Index);

    const QStringList l2 = l2Options(l1Index);
    if (l2.isEmpty()) {
        l2Index = 0;
    } else {
        l2Index = qBound(0, l2Index, l2.size() - 1);
    }
}

AiConfigNode AiConfigModel::parseNode(const QJsonObject& obj)
{
    AiConfigNode node;
    node.id = obj.value(QStringLiteral("id")).toInt();
    node.title = obj.value(QStringLiteral("title")).toString();
    const QJsonArray children = obj.value(QStringLiteral("children")).toArray();
    node.children.reserve(children.size());
    for (const QJsonValue& value : children) {
        if (!value.isObject()) {
            continue;
        }
        node.children.append(parseNode(value.toObject()));
    }
    return node;
}

QStringList AiConfigModel::titlesFromNodes(const QList<AiConfigNode>& nodes, bool allowFallback)
{
    QStringList titles;
    titles.reserve(nodes.size());
    for (const AiConfigNode& node : nodes) {
        titles << (node.title.isEmpty() ? defaultOptionTitle() : node.title);
    }
    if (titles.isEmpty() && allowFallback) {
        titles << defaultOptionTitle();
    }
    return titles;
}

int AiConfigModel::boundedL1Index(int l1Index) const
{
    const AiConfigNode* module = currentModuleNode();
    if (!module || module->children.isEmpty()) {
        return 0;
    }
    return qBound(0, l1Index, module->children.size() - 1);
}

const AiConfigNode* AiConfigModel::currentModuleNode() const
{
    if (m_currentModule.id <= 0 && m_currentModule.children.isEmpty()) {
        return nullptr;
    }
    return &m_currentModule;
}

void AiConfigModel::refreshCurrentModule()
{
    m_currentModule = {};
    for (const AiConfigNode& node : m_moduleRoots) {
        if (node.id == m_moduleId) {
            m_currentModule = node;
            return;
        }
    }
    if (!m_moduleRoots.isEmpty()) {
        m_currentModule = m_moduleRoots.first();
        m_moduleId = m_currentModule.id > 0 ? m_currentModule.id : m_moduleId;
    }
}

QString AiConfigModel::fallbackLabel()
{
    return defaultOptionTitle();
}
