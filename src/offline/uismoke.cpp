#include "uismoke.h"
#include "offlineapi.h"
#include "../uis/loginpanel.h"
#include "../uis/uiglobalstate.h"
#include "../uis/casepanel.h"
#include "../uis/caseworkpanel.h"
#include "../casectrls/casedetailpanel.h"
#include "../casectrls/basedatatableview.h"
#include "../casectrls/casetreemodel.h"
#include "../casectrls/casewfsmapcontroller.h"
#include "../casectrls/casewfseditcommit.h"
#include "../casectrls/newcasedialog.h"
#include "../homectrls/recentprojecthistory.h"
#include "../tnmapwidget/tnmapwidget.h"
#include <qgsvectorlayer.h>
#include <qgsmapcanvas.h>
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QTemporaryDir>
#include <functional>
#include <stdexcept>

namespace {
int projectId=0;
QJsonArray checks;
QString failure,output;
void require(bool ok,const QString& text) {
    if(!ok)throw std::runtime_error(text.toUtf8().constData());
    checks.append(text);
}
bool waitFor(const std::function<bool()>& predicate,int timeout=7000) {
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<timeout){QApplication::processEvents(QEventLoop::AllEvents,25);if(predicate())return true;QThread::msleep(15);}
    return predicate();
}
void settle(int ms=300){QElapsedTimer t;t.start();waitFor([&](){return t.elapsed()>=ms;},ms+100);}
void shot(QWidget* window,const QString& name){settle();require(window->grab().save(output+"/"+name+".png"),"screenshot "+name);}
void zoom(TNMapWidget* map,QgsVectorLayer* layer){auto extent=layer->extent();extent.scale(1.25);map->canvas()->setExtent(extent);map->canvas()->refresh();}
QJsonValue api(const QString& route,const QVariantMap& args={},const QString& file={}) {
    const auto reply=OfflineApi::execute("POST","http://offline.invalid"+route,args,file);
    if(reply["code"].toInt()!=200)throw std::runtime_error(reply["message"].toString().toUtf8().constData());return reply["data"];
}
QString physical(int caseId=0){return QString("tnp2_project%1_user1_%2road_way").arg(projectId).arg(caseId?QString("case%1_").arg(caseId):QString());}
QgsVectorLayer* loaded(TNMapWidget* map,int caseId) {
    return map?qobject_cast<QgsVectorLayer*>(map->getLayer(CaseWfsMap::buildWfsDataSourceUri(physical(caseId)))):nullptr;
}
void run(QWidget* window) {
    try {
        shot(window,"01-login");
        auto* login=window->findChild<LoginPanel*>();require(login,"original login widget");
        require(QMetaObject::invokeMethod(login,"onLoginButtonClicked"),"login action");
        require(waitFor([]{return UIG().isLogin();}),"offline login completed");settle(700);shot(window,"02-home");
        projectId=api("/api/project/add",{{"name",u8"二期界面验收临时工程"},{"province_id",1},{"city_id",1}}).toObject()["project_id"].toInt();require(projectId>0,"temporary project created");
        QTemporaryDir fixture;const QString path=fixture.filePath("roads.csv");QFile file(path);require(file.open(QIODevice::WriteOnly),"fixture file");
        file.write("id,link_id,name,type,type1,capacity,length,free_flow_time,wkt\n1,10,A,1,1,1200,1.5,0.1,\"LINESTRING(104 30,104.04 30.04,104.1 30.1)\"\n2,20,B,1,1,900,2,0.2,\"LINESTRING(104.04 30.04,104.1 30.01)\"\n3,30,C,1,1,800,2,0.3,\"LINESTRING(104.04 30.04,104.01 30.08)\"\n");file.close();
        for(const char* table:{"road_way","bus_way","slow_road_way"})api("/api/files/drcsv",{{"project_id",projectId},{"table",table}},path);
        const int baseId=api("/api/case/add",{{"project_id",projectId},{"name",u8"基础方案"}}).toObject()["id"].toInt();
        const int copyId=api("/api/case/copy",{{"case_id",baseId},{"name",u8"路网改造方案"}}).toObject()["id"].toInt();
        UIG().openProject(projectId);
        require(waitFor([&]{return window->findChild<CasePanel*>()!=nullptr;}),"original project tab opened");
        auto* panel=window->findChild<CasePanel*>();settle(500);shot(window,"03-schemes");
        QMetaObject::invokeMethod(panel,"onBaseDataTabClicked");
        auto* tree=panel->findChild<BaseDataTableView*>();require(tree,"original base data tree");
        {QgsVectorLayer attributes(CaseWfsMap::buildWfsDataSourceUri(QString("tnp2_project%1_user1_other_observation").arg(projectId)),"attributes","postgres");require(attributes.isValid()&&attributes.geometryType()==Qgis::GeometryType::Null,"empty attribute-only table recognized");}
        require(tree->setLayerCheckedByVal("road_way",true),"base tree check action");
        QgsVectorLayer* baseLayer=nullptr;TNMapWidget* baseMap=nullptr;
        require(waitFor([&]{for(auto* map:panel->findChildren<TNMapWidget*>()){auto* layer=loaded(map,0);if(layer&&layer->isValid()){baseLayer=layer;baseMap=map;return true;}}return false;}),"base map loaded local PostGIS layer");
        require(baseLayer->providerType()=="postgres"&&baseLayer->featureCount()==3,"base map provider and features");zoom(baseMap,baseLayer);shot(window,"04-base-map");
        for(const QString& logical:QStringList{"bus_way","slow_road_way"}){
            require(tree->setLayerCheckedByVal(logical,true),"transport layer checked: "+logical);
            const QString key=QString("tnp2_project%1_user1_%2").arg(projectId).arg(logical);
            require(waitFor([&]{auto* transport=qobject_cast<QgsVectorLayer*>(baseMap->getLayer(CaseWfsMap::buildWfsDataSourceUri(key)));return transport&&transport->isValid()&&transport->providerType()=="postgres"&&transport->featureCount()==3;}),"transport map loaded: "+logical);
            tree->setLayerCheckedByVal(logical,false);
        }
        QMetaObject::invokeMethod(panel,"onBaseDataClicked");shot(window,"05-base-attributes");
        QMetaObject::invokeMethod(panel,"onSchemeTabClicked");
        QMetaObject::invokeMethod(panel,"onCaseOpened",Q_ARG(int,copyId));
        auto* detail=panel->findChild<CaseDetailPanel*>();require(detail,"original scheme workbench");
        auto* model=panel->findChild<CaseTreeModel*>();require(model,"original scheme tree model");
        require(model->setLayerCheckedByVal(copyId,"road_way",true),"scheme tree check action");
        require(waitFor([&]{return loaded(detail->standardMapWidget(),copyId)!=nullptr;}),"scheme map loaded");
        auto* layer=loaded(detail->standardMapWidget(),copyId);require(layer->isValid()&&layer->featureCount()==3,"scheme layer valid");
        require(layer->geometryType()==Qgis::GeometryType::Line,"scheme line geometry recognized");
        zoom(detail->standardMapWidget(),layer);shot(window,"06-scheme-map");
        QgsFeature feature;require(layer->getFeatures().nextFeature(feature),"read feature for edit");
        require(layer->startEditing(),"PostGIS edit session");require(layer->changeAttributeValue(feature.id(),layer->fields().indexFromName("capacity"),1800),"change capacity");
        QString error;require(CaseWfsEditCommit::commitLayerEdits(layer,QString(),&error),"native edit commit: "+error);
        const auto values=api("/api/project/select_data_page",{{"tablefullname",physical(copyId)}}).toObject()["data"].toArray();bool saved=false;for(const auto& v:values)if(v.toObject()["id"].toInt()==feature.id())saved=v.toObject()["capacity"].toDouble()==1800;
        require(saved,"map edit persisted in PostgreSQL");
        QgsFeature added(layer->fields());added.setAttribute("name","UI added");added.setGeometry(QgsGeometry::fromWkt("LINESTRING(104.02 30.02,104.05 30.07)"));
        require(layer->startEditing()&&layer->addFeature(added),"add line feature");require(CaseWfsEditCommit::commitLayerEdits(layer,QString(),&error),"new feature persisted");
        require(layer->featureCount()==4,"new feature count");
        QgsFeature inserted;bool found=false;auto it=layer->getFeatures();while(it.nextFeature(inserted))if(inserted.attribute("name").toString()=="UI added"){found=true;break;}
        require(found&&layer->startEditing()&&layer->deleteFeature(inserted.id()),"delete added feature");require(CaseWfsEditCommit::commitLayerEdits(layer,QString(),&error)&&layer->featureCount()==3,"feature deletion persisted");
        const auto baseValues=api("/api/project/select_data_page",{{"tablefullname",physical(baseId)}}).toObject()["data"].toArray();bool isolated=true;for(const auto& v:baseValues)if(v.toObject()["capacity"].toDouble()==1800)isolated=false;require(isolated,"base scheme isolated from edit");
        detail->switchToStandardDataView();detail->refreshStandardAttributeFromMap();shot(window,"07-scheme-attributes");
        QMetaObject::invokeMethod(panel,"onCaseOpened",Q_ARG(int,copyId));
        QMetaObject::invokeMethod(detail,"onStdRenderClicked");
        detail->workPanel()->setAnalysisModule(CaseWorkPanel::AnalysisModule::MotorVehicle);detail->workPanel()->setExpanded(true);settle(700);shot(window,"08-workflow");detail->workPanel()->setExpanded(false);
        require(model->setLayerCheckedByVal(baseId,"road_way",true),"base scheme selected for comparison");
        CompareCaseItem left;left.id=baseId;left.name=u8"基础方案";left.projectName=u8"二期界面验收临时工程";
        CompareCaseItem right;right.id=copyId;right.name=u8"路网改造方案";right.projectName=left.projectName;
        detail->setCompareCases(left,right);detail->setToolbarViewMode(CaseMapToolbarWidget::ViewMode::Compare);
        QMetaObject::invokeMethod(panel,"onDetailCompareCasesChanged",Q_ARG(int,baseId),Q_ARG(int,copyId));
        require(waitFor([&]{return loaded(detail->leftCompareMapWidget(),baseId)&&loaded(detail->rightCompareMapWidget(),copyId);}),"both comparison maps loaded local layers");
        zoom(detail->leftCompareMapWidget(),loaded(detail->leftCompareMapWidget(),baseId));zoom(detail->rightCompareMapWidget(),loaded(detail->rightCompareMapWidget(),copyId));shot(window,"09-compare");
        {NewCaseDialog dialog(window);dialog.applyNewCaseMode(false);dialog.show();shot(&dialog,"10-new-scheme");dialog.close();}
    }catch(const std::exception& e){failure=QString::fromUtf8(e.what());}
    CaseWfsMap::prepareForShutdown();
    QApplication::exit(failure.isEmpty()?0:1);
}
}
void OfflineUiSmoke::start(QWidget* window){output=qEnvironmentVariable("TNP2_UI_TEST_OUTPUT");QDir().mkpath(output);QTimer::singleShot(500,window,[window]{run(window);});}
bool OfflineUiSmoke::finish(){
    try{if(projectId){api("/api/project/del",{{"id",projectId}});RecentProjectHistory history;history.load();history.removeByProjectId(projectId);history.save();checks.append("temporary project and recent history removed");}}catch(const std::exception& e){failure+=QString::fromUtf8(e.what());}
    QFile report(output+"/result.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(QJsonObject{{"passed",failure.isEmpty()},{"failure",failure},{"checks",checks}}).toJson());
    return failure.isEmpty();
}
