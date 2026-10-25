/*****************************************************************//**
 * \file   basedatamodel.h
 * \brief  基础数据模型
 * \details 管理基础数据的列表，继承自 TableModelBase
 *
 * \author zhangmg
 * \date   April 2026
 * \History 初始版本
 *********************************************************************/
#ifndef BASEDATAMODEL_H
#define BASEDATAMODEL_H

#include "../publicctrls/tablemodelbase.h"

/**
 * \brief  基础数据项
 * \details 用于存储基础数据表格的行数据
 */
struct BaseDataItem {
    int id;                          ///< 数据ID
    QString name;                    ///< 数据名称
    QString type;                    ///< 数据类型
    QString source;                  ///< 数据来源
    QString status;                  ///< 数据状态
    QDateTime createTime;             ///< 创建时间
    QVariantMap attributes;          ///< 自定义属性（可扩展）
    bool checked;                    ///< 是否选中
};

/**
 * \brief  基础数据模型
 * \details 管理基础数据的列表，支持复选框
 * \author zhangmg
 * \date   April 2026
 */
class BaseDataTableModel : public TableModelBase<BaseDataItem> {
    Q_OBJECT

public:
    explicit BaseDataTableModel(QObject* parent = nullptr);
    ~BaseDataTableModel() override;

    /**
     * \brief  获取表头
     * \return 表头字符串列表
     * \author zhangmg
     * \date   April 2026
     */
    const QStringList headers() const override;

    /**
     * \brief  获取指定列的值
     * \param  data 数据项
     * \param  col 列索引
     * \return 对应列的值
     * \author zhangmg
     * \date   April 2026
     */
    QVariant colValue(const BaseDataItem& data, int col) const override;

    /**
     * \brief  获取数据
     * \param  index 模型索引
     * \param  role 角色
     * \return 数据
     * \author zhangmg
     * \date   April 2026
     */
    QVariant data(const QModelIndex& index, int role) const override;

    /**
     * \brief  设置数据
     * \param  index 模型索引
     * \param  value 值
     * \param  role 角色
     * \return 是否设置成功
     * \author zhangmg
     * \date   April 2026
     */
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    /**
     * \brief  获取项目标志
     * \param  index 模型索引
     * \return 项目标志
     * \author zhangmg
     * \date   April 2026
     */
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    /**
     * \brief  添加基础数据
     * \param  item 基础数据项
     * \author zhangmg
     * \date   April 2026
     */
    void addBaseData(const BaseDataItem& item);

    /**
     * \brief  删除基础数据
     * \param  index 数据索引
     * \author zhangmg
     * \date   April 2026
     */
    void removeBaseData(int index);

    /**
     * \brief  更新基础数据
     * \param  index 数据索引
     * \param  item 新的基础数据
     * \author zhangmg
     * \date   April 2026
     */
    void updateBaseData(int index, const BaseDataItem& item);

    /**
     * \brief  设置选中状态
     * \param  index 数据索引
     * \param  checked 是否选中
     * \author zhangmg
     * \date   April 2026
     */
    void setChecked(int index, bool checked);

    /**
     * \brief  获取选中的数据
     * \return 选中的数据列表
     * \author zhangmg
     * \date   April 2026
     */
    QList<BaseDataItem> getCheckedItems() const;

signals:
    /**
     * \brief  复选框状态改变信号
     * \param  index 模型索引
     * \param  checked 是否选中
     * \author zhangmg
     * \date   April 2026
     */
    void checkStateChanged(const QModelIndex& index, bool checked);
};

#endif // BASEDATAMODEL_H
