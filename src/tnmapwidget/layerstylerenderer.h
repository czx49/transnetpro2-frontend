/*****************************************************************//**
 * \file   layerstylerenderer.h
 * \brief  字段符号化规则到 QGIS 渲染器的转换
 *
 * \author zhangmg
 * \date   July 2026
 *********************************************************************/
#ifndef LAYERSTYLERENDERER_H
#define LAYERSTYLERENDERER_H

#include "layerstylepersistence.h"

class QgsSymbol;
class QgsVectorLayer;

namespace LayerStyleRenderer {

bool isNumericFieldType(int variantType);
bool applyFieldColorRule(QgsVectorLayer* layer,
                         const QString& fieldName,
                         const FieldColorRule& rule,
                         QString* errorMessage = nullptr);
void configureSymbol(QgsSymbol* symbol, double opacity, double lineWidth);

} // namespace LayerStyleRenderer

#endif // LAYERSTYLERENDERER_H
