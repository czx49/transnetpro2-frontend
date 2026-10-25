/*****************************************************************//**
 * \file   tnfloatdisplayattributetablemodel.h
 * \brief  QgsAttributeTableModel 子类：DisplayRole 浮点最多 4 位小数
 *
 * \author zhangmg
 * \date   August 2026
 *********************************************************************/
#ifndef TNFLOATDISPLAYATTRIBUTETABLEMODEL_H
#define TNFLOATDISPLAYATTRIBUTETABLEMODEL_H

#include "../publicctrls/numberdisplayutil.h"

#include <qgsattributetablemodel.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsvectorlayer.h>

/**
 * \brief 仅改 DisplayRole 浮点展示；EditRole / 写回 / 导出仍用基类原始精度
 */
class TnFloatDisplayAttributeTableModel : public QgsAttributeTableModel
{
public:
    using QgsAttributeTableModel::QgsAttributeTableModel;

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::DisplayRole && index.isValid() && layer()) {
            const int fieldIndex = fieldIdx(index.column());
            if (fieldIndex >= 0 && fieldIndex < layer()->fields().count()) {
                const int fieldType = static_cast<int>(layer()->fields().at(fieldIndex).type());
                if (NumberDisplayUtil::isFloatingPointType(fieldType)) {
                    const QVariant raw = QgsAttributeTableModel::data(index, Qt::EditRole);
                    if (raw.isValid() && !raw.isNull()) {
                        return NumberDisplayUtil::formatAttributeDisplay(raw, fieldType);
                    }
                }
            }
        }
        return QgsAttributeTableModel::data(index, role);
    }
};

#endif // TNFLOATDISPLAYATTRIBUTETABLEMODEL_H
