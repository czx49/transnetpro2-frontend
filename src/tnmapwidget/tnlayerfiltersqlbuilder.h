/*****************************************************************//**
 * \file   tnlayerfiltersqlbuilder.h
 * \brief  图层按属性过滤 — SQL 构建、JSON 与 subset 解析
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef TNLAYERFILTERSQLBUILDER_H
#define TNLAYERFILTERSQLBUILDER_H

#include "tnlayerfiltertypes.h"

#include <QString>

class QgsVectorLayer;

namespace TNLayerFilter {

class SqlBuilder
{
public:
    /** @brief SQL WHERE 片段（PostGIS / OGR SQL 等） */
    static QString buildSql(const QgsVectorLayer* layer, const QVector<Clause>& clauses, QString* errorOut = nullptr);

    /** @brief QGIS 表达式（WFS subset / filter 参数） */
    static QString buildQgsExpression(const QgsVectorLayer* layer, const QVector<Clause>& clauses,
                                      QString* errorOut = nullptr);

    /** @brief 按图层 provider 选择 SQL 或 QGIS 表达式 */
    static QString buildForLayer(const QgsVectorLayer* layer, const QVector<Clause>& clauses,
                                 QString* errorOut = nullptr);

    /** @brief 图层是否应使用 QGIS 表达式作为 subset */
    static bool layerUsesQgsExpressionSubset(const QgsVectorLayer* layer);

    /** @brief 校验子集字符串是否可被当前图层接受 */
    static bool validateSubsetForLayer(const QgsVectorLayer* layer, const QString& subset, QString* errorOut = nullptr);

    /** @deprecated 使用 buildForLayer */
    static QString build(const QgsVectorLayer* layer, const QVector<Clause>& clauses, QString* errorOut = nullptr)
    {
        return buildForLayer(layer, clauses, errorOut);
    }

    static QString toJson(const QVector<Clause>& clauses, const QString& subset);
    static bool fromJson(const QString& json, PersistedState* stateOut, QString* errorOut = nullptr);

    static PersistedState loadPersistedState(const QgsVectorLayer* layer);
    static void savePersistedState(QgsVectorLayer* layer, const QVector<Clause>& clauses, const QString& subset);
    static void clearPersistedState(QgsVectorLayer* layer);

    static QVector<Clause> tryParseSubsetString(const QgsVectorLayer* layer, const QString& subset,
                                                QString* errorOut = nullptr);

    static QVector<Clause> clausesForDialogOpen(const QgsVectorLayer* layer, QString* warningOut = nullptr);

    static bool operatorNeedsValue(Operator op);
    static bool operatorNeedsTwoValues(Operator op);
};

} // namespace TNLayerFilter

#endif // TNLAYERFILTERSQLBUILDER_H
