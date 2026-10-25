#ifndef SYMBOLRENDERERCONFIG_H
#define SYMBOLRENDERERCONFIG_H

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include "qgsmaplayer.h"
#include "qgsvectorlayer.h"
#include "qgsrenderer.h"
#include "qgscolorrampimpl.h"

// 符号渲染配置工具类：根据传入参数构建并应用 QGIS 渲染器
class SymbolRendererConfig {
public:
    // 渲染器类型枚举，覆盖常见样式设计需求
    enum RendererType {
        Single,             // 单一符号渲染：所有要素使用同一符号
        Categorized,        // 分类渲染：按字段值分类着色
        Graduated,          // 分级渲染：按字段数值范围分段着色
        RuleBased,          // 规则渲染：表达式匹配并可设置比例尺范围
        Heatmap,            // 热力图渲染：点要素按权重生成热度
        PointCluster,       // 点聚类渲染：相近点合并显示为聚类符号
        InvertedPolygon,    // 反向面渲染：在面之外区域进行填充
        NullRenderer        // 空符号渲染：不绘制符号，仅支持标注/图表
    };

    // 将渲染配置应用到指定图层
    static bool applyToLayer(QgsMapLayer* layer, QVariantMap spec);
    // 根据配置为矢量图层构建具体渲染器
    static QgsFeatureRenderer* buildRenderer(QgsVectorLayer* layer, QVariantMap spec);
    // 创建色带
    static std::shared_ptr<QgsGradientColorRamp> createColorRamp(const QString& colorRampName = "RdYlGn");
    /** @brief 预置色带名称列表（字段分级 / 分析样式共用） */
    static QStringList availableColorRampNames();
    /** @brief 色带中文展示名；未知名称时回退为原名 */
    static QString colorRampDisplayName(const QString& colorRampName);
};

#endif
