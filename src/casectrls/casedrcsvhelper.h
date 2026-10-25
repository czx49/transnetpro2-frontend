/*****************************************************************//**
 * \file   casedrcsvhelper.h
 * \brief  `/api/files/drcsv` 与 `/api/project/export` 的 table 参数（方案树 / 基础数据图层共用）
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef CASEDRCSVHELPER_H
#define CASEDRCSVHELPER_H

#include <QString>

namespace CaseDrcsvHelper {

/** @brief 由图层元数据 val 得到 drcsv/export 的 table（仅信任 val，不做 typename 回退） */
QString tableNameFromLayerVal(const QString& valRaw);

} // namespace CaseDrcsvHelper

#endif
