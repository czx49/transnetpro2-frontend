/*****************************************************************//**
 * \file   phase2treeutil.cpp
 * \brief  Phase2 深色侧栏树分支箭头样式工具实现
 *
 * \author zhangmg
 * \date   June 2026
 *********************************************************************/
#include "phase2treeutil.h"

namespace Phase2Ui {

QString darkSidebarTreeBranchStyleRules(const QString& selector)
{
    return selector + QStringLiteral(
        "::branch {"
        "   background: transparent;"
        "   border: none;"
        "}"
        "%1::branch:has-children {"
        "   width: 16px;"
        "   height: 16px;"
        "}"
        "%1::branch:has-children:!has-siblings:closed,"
        "%1::branch:closed:has-children:has-siblings {"
        "   border-image: none;"
        "   image: url(:/icons/arrow_right.png);"
        "}"
        "%1::branch:has-children:!has-siblings:closed:hover,"
        "%1::branch:closed:has-children:has-siblings:hover {"
        "   border-image: none;"
        "   image: url(:/icons/arrow_right_hover.png);"
        "}"
        "%1::branch:open:has-children:!has-siblings,"
        "%1::branch:open:has-children:has-siblings {"
        "   border-image: none;"
        "   image: url(:/icons/arrow_down.png);"
        "}"
        "%1::branch:open:has-children:!has-siblings:hover,"
        "%1::branch:open:has-children:has-siblings:hover {"
        "   border-image: none;"
        "   image: url(:/icons/arrow_down_hover.png);"
        "}"
    ).arg(selector);
}

} // namespace Phase2Ui
