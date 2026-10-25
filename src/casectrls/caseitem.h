#ifndef CASEITEM_H
#define CASEITEM_H

#include <QColor>
#include <QDateTime>
#include <QList>
#include <QPointF>
#include <QString>
#include <QVariantMap>

/** @brief 方案双屏对比下拉与卡片用的轻量描述 */
struct CompareCaseItem {
    int id = -1;
    QString name;
    QString projectName;
    QDateTime createTime;
};

/** @brief 地图差异标注（预留） */
struct MapDiffMarker {
    QPointF position;
    QString description;
    QColor color;
};

struct LayerItem {
    QString name;
    QString category;
    /** @brief WFS typeName，与接口 list[].value、Capabilities Name 一致 */
    QString wfsTypeName;
    QString val;
    int level = 0;
    bool checked = false;
};

/** @brief 接口 menu/list 中的图层分组（如道路基础数据、公交基础数据） */
struct LayerGroupItem {
    QString name;
    /** @brief 分组标识：road / bus / slow / other / clip 等 */
    QString value;
    QList<LayerItem> layers;
};

struct CaseItem {
    int id;
    QString name;
    int typeId;
    QString typeName;
    /** @brief 接口 isbase：1 为基础方案，0 为普通方案 */
    int isBase = 0;
    int projectId;
    QDateTime createTime;
    QString status;
    int linkCount;
    int nodeCount;
    int zoneCount;
    int odPairCount;
    QString remark;
    QVariantMap parameters;
    /** @brief 来自 /api/case/list 的 list[] 分组结构 */
    QList<LayerGroupItem> caseLayerGroups;
    /** @brief 扁平图层列表（由 caseLayerGroups 展开，供 WFS 遍历等） */
    QList<LayerItem> caseLayers;
};

struct CaseDetailItem {
    int caseId;
    QString caseName;
    int projectId;
    QString projectName;
    QString creator;
    QDateTime createTime;
    QDateTime updateTime;
    QVariantMap roadTypes;
    QVariantMap busTypes;
    QVariantMap otherTypes;
    QVariantMap algResults;
    QList<QString> layers;
};

#endif
