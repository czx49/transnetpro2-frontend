/*****************************************************************//**
 * \file   numberdisplayutil.cpp
 * \brief  表格等 UI 浮点数值展示格式化
 *
 * \author zhangmg
 * \date   August 2026
 *********************************************************************/
#include "numberdisplayutil.h"

#include <QMetaType>
#include <QtGlobal>
#include <cmath>

namespace NumberDisplayUtil {

bool isFloatingPointType(int qVariantType)
{
    return qVariantType == static_cast<int>(QVariant::Double)
        || qVariantType == static_cast<int>(QMetaType::Float);
}

QString formatFloatingPointDisplay(double value)
{
    if (!std::isfinite(value)) {
        return QString::number(value);
    }

    QString text = QString::number(value, 'f', 4);
    if (!text.contains(QLatin1Char('.'))) {
        return text;
    }

    while (text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    return text;
}

QString formatAttributeDisplay(const QVariant& value, int fieldType)
{
    if (!value.isValid() || value.isNull()) {
        return QString();
    }
    if (!isFloatingPointType(fieldType)) {
        return value.toString();
    }
    bool ok = false;
    const double number = value.toDouble(&ok);
    if (!ok) {
        return value.toString();
    }
    return formatFloatingPointDisplay(number);
}

} // namespace NumberDisplayUtil
