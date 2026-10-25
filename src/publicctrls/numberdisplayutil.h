/*****************************************************************//**
 * \file   numberdisplayutil.h
 * \brief  表格等 UI 浮点数值展示格式化（最多 4 位小数、去尾零）
 *
 * \author zhangmg
 * \date   August 2026
 *********************************************************************/
#ifndef NUMBERDISPLAYUTIL_H
#define NUMBERDISPLAYUTIL_H

#include <QString>
#include <QVariant>

namespace NumberDisplayUtil {

/** @brief 是否为浮点 QVariant 类型（含 Float / Double） */
bool isFloatingPointType(int qVariantType);

/**
 * @brief 浮点展示：小数点后最多 4 位、四舍五入、去尾随 0 与小数点
 * @param value 待格式化数值
 * @return 展示用字符串；NaN/Inf 回退为 QString::number 默认行为
 */
QString formatFloatingPointDisplay(double value);

/**
 * @brief 按字段类型将属性值转为展示文案；非浮点或空值走 QVariant::toString
 * @param value 属性值
 * @param fieldType 字段 QVariant 类型
 */
QString formatAttributeDisplay(const QVariant& value, int fieldType);

} // namespace NumberDisplayUtil

#endif // NUMBERDISPLAYUTIL_H
