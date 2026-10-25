/*****************************************************************//**
 * \file   phase2treeutil.h
 * \brief  Phase2 深色侧栏树分支箭头样式工具
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#ifndef PHASE2TREEUTIL_H
#define PHASE2TREEUTIL_H

#include <QString>

namespace Phase2Ui {

/** @brief 深色侧栏树 ::branch 图标 QSS（selector 为 QTreeView 或 QTreeWidget） */
QString darkSidebarTreeBranchStyleRules(const QString& selector);

} // namespace Phase2Ui

#endif // PHASE2TREEUTIL_H
