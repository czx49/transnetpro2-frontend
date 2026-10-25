#ifndef CASEWFSEDITCOMMIT_H
#define CASEWFSEDITCOMMIT_H

#include <QString>

class QgsVectorLayer;

namespace CaseWfsEditCommit {

/** @brief 是否为 Phase2 侧栏加载的业务 WFS 图层 */
bool isPanelWfsLayer(QgsVectorLayer* layer);

/** @brief 通过服务端 REST JSON Transaction（POST /wfs）提交 editBuffer，绕过 QGIS WFS-T */
bool commitLayerEdits(QgsVectorLayer* layer, const QString& authToken, QString* errorOut = nullptr);

} // namespace CaseWfsEditCommit

#endif // CASEWFSEDITCOMMIT_H
