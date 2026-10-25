/*****************************************************************//**
 * \file   tnlayerfiltertypes.cpp
 * \brief  图层按属性过滤 — 类型工具实现
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "tnlayerfiltertypes.h"

#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsvectorlayer.h>

namespace TNLayerFilter {

FieldKind fieldKindForField(const QgsField& field)
{
    switch (field.type()) {
    case QMetaType::Bool:
        return FieldKind::Boolean;
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::Float:
        return FieldKind::Numeric;
    case QMetaType::QDate:
    case QMetaType::QDateTime:
    case QMetaType::QTime:
        return FieldKind::DateTime;
    case QMetaType::QString:
        return FieldKind::String;
    default:
        return FieldKind::Other;
    }
}

FieldKind fieldKindForLayerField(const QgsVectorLayer* layer, const QString& fieldName)
{
    if (!layer) {
        return FieldKind::Other;
    }
    const int idx = layer->fields().indexFromName(fieldName);
    if (idx < 0) {
        return FieldKind::Other;
    }
    return fieldKindForField(layer->fields().at(idx));
}

QString joinTypeToString(JoinType join)
{
    switch (join) {
    case JoinType::And:
        return QStringLiteral("and");
    case JoinType::Or:
        return QStringLiteral("or");
    default:
        return QString();
    }
}

JoinType joinTypeFromString(const QString& s)
{
    const QString t = s.trimmed().toLower();
    if (t == QLatin1String("or")) {
        return JoinType::Or;
    }
    if (t == QLatin1String("and")) {
        return JoinType::And;
    }
    return JoinType::None;
}

QString operatorToCode(Operator op)
{
    switch (op) {
    case Operator::Eq:
        return QStringLiteral("eq");
    case Operator::Ne:
        return QStringLiteral("ne");
    case Operator::Gt:
        return QStringLiteral("gt");
    case Operator::Ge:
        return QStringLiteral("ge");
    case Operator::Lt:
        return QStringLiteral("lt");
    case Operator::Le:
        return QStringLiteral("le");
    case Operator::Between:
        return QStringLiteral("between");
    case Operator::IsNull:
        return QStringLiteral("is_null");
    case Operator::IsNotNull:
        return QStringLiteral("is_not_null");
    case Operator::LikeContains:
        return QStringLiteral("like_contains");
    case Operator::LikeStarts:
        return QStringLiteral("like_starts");
    case Operator::LikeEnds:
        return QStringLiteral("like_ends");
    }
    return QStringLiteral("eq");
}

Operator operatorFromCode(const QString& code)
{
    const QString c = code.trimmed().toLower();
    if (c == QLatin1String("ne")) {
        return Operator::Ne;
    }
    if (c == QLatin1String("gt")) {
        return Operator::Gt;
    }
    if (c == QLatin1String("ge")) {
        return Operator::Ge;
    }
    if (c == QLatin1String("lt")) {
        return Operator::Lt;
    }
    if (c == QLatin1String("le")) {
        return Operator::Le;
    }
    if (c == QLatin1String("between")) {
        return Operator::Between;
    }
    if (c == QLatin1String("is_null")) {
        return Operator::IsNull;
    }
    if (c == QLatin1String("is_not_null")) {
        return Operator::IsNotNull;
    }
    if (c == QLatin1String("like_contains")) {
        return Operator::LikeContains;
    }
    if (c == QLatin1String("like_starts")) {
        return Operator::LikeStarts;
    }
    if (c == QLatin1String("like_ends")) {
        return Operator::LikeEnds;
    }
    return Operator::Eq;
}

QString operatorDisplayName(Operator op, FieldKind kind)
{
    Q_UNUSED(kind)
    switch (op) {
    case Operator::Eq:
        return QString::fromUtf8(u8"等于");
    case Operator::Ne:
        return QString::fromUtf8(u8"不等于");
    case Operator::Gt:
        return kind == FieldKind::DateTime ? QString::fromUtf8(u8"晚于") : QString::fromUtf8(u8"大于");
    case Operator::Ge:
        return kind == FieldKind::DateTime ? QString::fromUtf8(u8"不早于") : QString::fromUtf8(u8"大于等于");
    case Operator::Lt:
        return kind == FieldKind::DateTime ? QString::fromUtf8(u8"早于") : QString::fromUtf8(u8"小于");
    case Operator::Le:
        return kind == FieldKind::DateTime ? QString::fromUtf8(u8"不晚于") : QString::fromUtf8(u8"小于等于");
    case Operator::Between:
        return QString::fromUtf8(u8"介于");
    case Operator::IsNull:
        return QString::fromUtf8(u8"为空");
    case Operator::IsNotNull:
        return QString::fromUtf8(u8"不为空");
    case Operator::LikeContains:
        return QString::fromUtf8(u8"包含");
    case Operator::LikeStarts:
        return QString::fromUtf8(u8"开头是");
    case Operator::LikeEnds:
        return QString::fromUtf8(u8"结尾是");
    }
    return QString::fromUtf8(u8"等于");
}

QStringList operatorCodesForFieldKind(FieldKind kind)
{
    QStringList codes;
    switch (kind) {
    case FieldKind::Numeric:
    case FieldKind::DateTime:
        codes << QStringLiteral("eq") << QStringLiteral("ne") << QStringLiteral("gt") << QStringLiteral("ge")
              << QStringLiteral("lt") << QStringLiteral("le") << QStringLiteral("between") << QStringLiteral("is_null")
              << QStringLiteral("is_not_null");
        break;
    case FieldKind::String:
        codes << QStringLiteral("eq") << QStringLiteral("ne") << QStringLiteral("like_contains")
              << QStringLiteral("like_starts") << QStringLiteral("like_ends") << QStringLiteral("is_null")
              << QStringLiteral("is_not_null");
        break;
    case FieldKind::Boolean:
        codes << QStringLiteral("eq") << QStringLiteral("is_null") << QStringLiteral("is_not_null");
        break;
    default:
        codes << QStringLiteral("eq") << QStringLiteral("ne") << QStringLiteral("is_null")
              << QStringLiteral("is_not_null");
        break;
    }
    return codes;
}

} // namespace TNLayerFilter
