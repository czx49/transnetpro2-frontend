#ifndef LAYERSTYLEFACTORY_H
#define LAYERSTYLEFACTORY_H

#include <QVariantMap>
#include <QString>
#include <functional>
#include <memory>

class TNMapWidget;
class QgsVectorLayer;
class QgsMapCanvas;
class QgsGradientColorRamp;

// 图层样式工厂：按图层类型 val 或预设 key 生成并应用 QGIS 渲染样式
class LayerStyleFactory {
public:
    /** @brief WFS 图层 customProperty 键，存业务 val（如 bus_way）供恢复默认样式 */
    static QString layerValPropertyKey();

    static bool isOtherOdLayer(const QString& layerVal, const QString& typeName = QString());

    static bool applyDefaultStyle(QgsVectorLayer* layer, const QString& layerVal,
                                  const QString& typeName = QString());
    static bool applyStyle(QgsVectorLayer* layer, const QString& styleKey);
    static bool applyDefaultStyle(TNMapWidget* map, const QString& dataSourceUri, const QString& layerVal,
                                  const QString& typeName = QString());

    /** @brief 从 customProperty 或 typename/表名后缀解析业务 val */
    static QString resolveLayerVal(QgsVectorLayer* layer, const QString& layerVal = QString(),
                                   const QString& typeName = QString());

    static bool applyOtherOdDemandStyle(QgsVectorLayer* layer);
    static bool applyOtherOdDemandStyle(QgsVectorLayer* layer, std::function<void(bool applied)> done);
    static bool applyOtherOdDemandStyle(QgsVectorLayer* layer,
                                        QgsMapCanvas* canvas,
                                        std::function<void(bool applied)> done,
                                        bool skipZeroDelayFirstAttempt = false);
    /** @brief 同步尝试应用 OD demand 样式（无退避定时器） */
    static bool tryApplyOtherOdDemandStyle(QgsVectorLayer* layer, QgsMapCanvas* canvas = nullptr);
    static QVariantMap createOtherOdDemandSpec(QgsVectorLayer* layer);

    /** @brief 路网流量分布；colorRampName 默认 RdYlGn；useEqualCount 为等数量分级 */
    static bool applyTrafficFlowStyle(QgsVectorLayer* layer,
                                      const QString& colorRampName = QStringLiteral("RdYlGn"),
                                      bool useEqualCount = false);
    static bool applyTrafficSaturationStyle(QgsVectorLayer* layer);
    static bool applyPathTraceStyle(QgsVectorLayer* layer);

    static bool fieldMinMax(QgsVectorLayer* layer, const QString& field, double& min, double& max);
    static QVariantMap createGraduatedEqualIntervalSpec(
        const QString& field,
        double min,
        double max,
        int classCount,
        std::shared_ptr<QgsGradientColorRamp> ramp,
        bool scaleLineWidthByClass = false);
};

#endif
