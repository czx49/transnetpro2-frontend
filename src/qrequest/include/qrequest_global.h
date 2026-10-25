#ifndef QREQUEST_GLOBAL_H
#define QREQUEST_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(QREQUEST_STATIC_LIBRARY)
#define QREQUEST_EXPORT
#else
#if defined(QREQUEST_EXPORTS)
#define QREQUEST_EXPORT Q_DECL_EXPORT
#else
#define QREQUEST_EXPORT Q_DECL_IMPORT
#endif
#endif
#endif // QREQUEST_GLOBAL_H
