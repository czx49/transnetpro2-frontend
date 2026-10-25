#ifndef TNFEATUREATTRIBUTEDIALOG_H
#define TNFEATUREATTRIBUTEDIALOG_H

#include "framelessdialog.h"
#include <QSet>
#include <QTableWidget>
#include <QString>
#include <QStringList>
#include "qgsvectorlayer.h"
#include "qgsfeature.h"

class TNFeatureAttributeDialog : public FramelessDialog
{
    Q_OBJECT

public:
    explicit TNFeatureAttributeDialog(QgsVectorLayer* layer,
                                    const QgsFeature& feature,
                                    QWidget* parent = nullptr,
                                    const QString& titleText = QString(),
                                    const QStringList& readOnlyFieldNames = {});
    ~TNFeatureAttributeDialog();

    /** @brief 获取编辑后的属性 */
    QgsAttributeMap editedAttributes() const { return mEditedAttributes; }

private slots:
    void onAccepted();

private:
    void setupUI();
    void loadFeatureAttributes();

    QgsVectorLayer* mLayer = nullptr;
    QgsFeature mFeature;
    QTableWidget* mAttributeTable = nullptr;
    QSet<QString> mReadOnlyFieldNames;
    QgsAttributeMap mEditedAttributes;
};

#endif // TNFEATUREATTRIBUTEDIALOG_H
