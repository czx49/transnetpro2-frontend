#include "offlineimportdialog.h"
#include "offlineapi.h"
#include "framelessdialog.h"
#include "phase2dialogstyle.h"
#include "phase2comboboxutil.h"
#include "messagedialog.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

QString selectOfflineImportSource(QWidget* parent,const QString& layerTitle,const QString& logicalName)
{
    QString path=QFileDialog::getOpenFileName(parent,QString::fromUtf8(u8"导入本地数据：")+layerTitle,QString(),
        QString::fromUtf8(u8"数据库/空间文件 (*.db *.sqlite *.sqlite3 *.gpkg *.shp *.geojson *.json);;表格 (*.csv *.txt *.xlsx *.xls);;所有文件 (*)"));
    if(path.isEmpty())return {};
    const auto layers=OfflineApi::sourceLayers(path);
    if(layers.isEmpty()){TnUi::showWarning(u8"读取失败",u8"文件中没有可读取的数据表或空间图层。请检查文件格式。");return {};}
    FramelessDialog dialog(parent);dialog.setTitleText(u8"导入本地数据");dialog.setStyleSheet(Phase2Ui::dialogShellStyleSheet());dialog.resize(520,240);
    dialog.contentLayout()->addWidget(new QLabel(QString::fromUtf8(u8"文件：")+QFileInfo(path).fileName(),&dialog));
    dialog.contentLayout()->addWidget(new QLabel(QString::fromUtf8(u8"目标图层：")+layerTitle,&dialog));
    auto* combo=new QComboBox(&dialog);combo->addItems(layers);Phase2Ui::setupThemedCombo(combo,Phase2Ui::InputFieldTheme::LightCompact);
    const QString suffix=logicalName.section(':',-1).section('.',-1);for(int i=0;i<layers.size();++i)if(layers[i]==suffix||suffix.endsWith("_"+layers[i])){combo->setCurrentIndex(i);break;}
    dialog.contentLayout()->addWidget(combo);
    auto* note=new QLabel(u8"导入会替换当前图层的记录，源文件保持不变。",&dialog);note->setWordWrap(true);dialog.contentLayout()->addWidget(note);
    auto* actions=new QHBoxLayout;actions->addStretch();auto* cancel=new QPushButton(u8"取消",&dialog);cancel->setStyleSheet(Phase2Ui::dialogGhostButtonStyleSheet());auto* ok=new QPushButton(u8"导入",&dialog);ok->setStyleSheet(Phase2Ui::dialogPrimaryButtonStyleSheet());
    actions->addWidget(cancel);actions->addWidget(ok);dialog.contentLayout()->addLayout(actions);
    QObject::connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject);QObject::connect(ok,&QPushButton::clicked,&dialog,&QDialog::accept);
    if(dialog.exec()!=QDialog::Accepted)return {};
    return path+"|layername="+combo->currentText();
}
