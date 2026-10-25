/*****************************************************************//**
 * \file   tnlayerfiltertypes.h
 * \brief  图层按属性过滤 — 子句与运算符类型定义
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef TNLAYERFILTERTYPES_H
#define TNLAYERFILTERTYPES_H

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

class QgsField;
class QgsVectorLayer;

namespace TNLayerFilter {

const QString kCustomPropertyKey = QStringLiteral("tnp_attr_filter_v1");
/** @brief WFS 客户端显示过滤表达式（QGIS 表达式） */
const QString kDisplayFilterPropertyKey = QStringLiteral("tnp_layer_display_filter");
/** @brief 首次应用客户端过滤前的渲染器 XML 备份 */
const QString kRendererBackupPropertyKey = QStringLiteral("tnp_layer_renderer_backup_v1");
/** @brief 路段默认 type 过滤已下推到 WFS URI 时的标记（值为表达式；仅创建时写入） */
const QString kTypeServerFilterPropertyKey = QStringLiteral("tnp_wfs_type_server_filter");
/** @brief 工作台/双屏分析分级样式占用路段层（值为字段名，恢复默认样式时清除） */
const QString kWayAnalysisStylePropertyKey = QStringLiteral("tnp_way_analysis_style");
/** @brief 路段默认 type 过滤 fail-open 存在性校验已完成（值为已校验表达式；clearTypeFilter 时清除） */
const QString kTypeFilterVerifiedPropertyKey = QStringLiteral("tnp_way_type_filter_verified");
const int kMaxUniqueValues = 500;

enum class FieldKind
{
    Numeric = 0,
    String,
    DateTime,
    Boolean,
    Other
};

enum class JoinType
{
  None = 0,
  And,
  Or
};

enum class Operator
{
    Eq = 0,
    Ne,
    Gt,
    Ge,
    Lt,
    Le,
    Between,
    IsNull,
    IsNotNull,
    LikeContains,
    LikeStarts,
    LikeEnds
};

struct Clause
{
    JoinType join = JoinType::None;
    QString fieldName;
    Operator op = Operator::Eq;
    QVariant value1;
    QVariant value2;
    bool isValid() const { return !fieldName.trimmed().isEmpty(); }
};

struct PersistedState
{
    int version = 1;
    QString subset;
    QVector<Clause> clauses;
};

FieldKind fieldKindForField(const QgsField& field);
FieldKind fieldKindForLayerField(const QgsVectorLayer* layer, const QString& fieldName);

QString joinTypeToString(JoinType join);
JoinType joinTypeFromString(const QString& s);

QString operatorToCode(Operator op);
Operator operatorFromCode(const QString& code);
QString operatorDisplayName(Operator op, FieldKind kind);

QStringList operatorCodesForFieldKind(FieldKind kind);

} // namespace TNLayerFilter

Q_DECLARE_METATYPE(TNLayerFilter::Clause)

#endif // TNLAYERFILTERTYPES_H
