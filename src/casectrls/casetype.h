/*****************************************************************//**
 * \file   casetype.h
 * \brief  方案业务类型（case_type_id）枚举与显示名
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#ifndef CASETYPE_H
#define CASETYPE_H

#include <QString>

/**
 * @brief 与接口字段 case_type_id 对应的业务类型（非基础/普通区分，后者见 isbase）
 */
enum class CaseTypeId : int
{
    Unknown = 0,
    TrafficPolicy = 1
};

inline QString caseTypeIdDisplayName(CaseTypeId id)
{
    switch (id) {
    case CaseTypeId::TrafficPolicy:
        return QString::fromUtf8(u8"交通政策");
    case CaseTypeId::Unknown:
    default:
        return QString::fromUtf8(u8"未知类型");
    }
}

inline QString caseTypeIdDisplayName(int rawId)
{
    return caseTypeIdDisplayName(static_cast<CaseTypeId>(rawId));
}

#endif // CASETYPE_H
