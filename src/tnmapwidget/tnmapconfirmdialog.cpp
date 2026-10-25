/*****************************************************************//**
 * \file   tnmapconfirmdialog.cpp
 *
 * \author zhangmg
 * \date   May 2026
 *********************************************************************/
#include "tnmapconfirmdialog.h"

#include "../publicctrls/messagedialog.h"

namespace TnMapUi {

bool confirmYesNo(const QString& title, const QString& message)
{
    return TnUi::confirmYesNo(title, message);
}

} // namespace TnMapUi
