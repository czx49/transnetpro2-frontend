#include "offlineapi.h"
#include <QtTest>

class Phase2OfflineTests:public QObject {
    Q_OBJECT
    QTemporaryDir files;
    int projectId=0,baseId=0,copyId=0;
    QJsonObject call(const QString& route,const QVariantMap& args={},const QString& file={}) {
        return OfflineApi::execute("POST","http://offline.invalid"+route,args,file);
    }
    QString write(const QString& name,const QByteArray& contents) {
        const QString path=files.filePath(name);QFile f(path);if(!f.open(QIODevice::WriteOnly))return {};f.write(contents);return path;
    }
    QJsonObject payload(const QJsonObject& reply) {return reply["data"].toObject();}
    void success(const QJsonObject& reply){QVERIFY2(reply["code"].toInt()==200,qPrintable(reply["message"].toString()));}
private slots:
    void initTestCase() {
        QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
        for(const char* name:{"GDAL_DATA","PROJ_LIB","PROJ_DATA"}){const auto value=qEnvironmentVariable(name);if(!value.isEmpty())qputenv(name,value.toUtf8());}
        QString error;QVERIFY2(OfflineApi::initialize(error),qPrintable(error));
        auto reply=call("/api/project/add",{{"name",u8"二期数据验收临时工程"},{"province_id",1},{"city_id",1}});success(reply);projectId=payload(reply)["project_id"].toInt();QVERIFY(projectId>0);
    }
    void configurationAndProjectContract() {
        auto reply=call("/api/config/getversion");success(reply);QCOMPARE(payload(reply)["version"].toString(),QString("2.0.0"));
        success(call("/api/user/login"));success(call("/api/config/base"));
        reply=call("/api/project/detail",{{"id",projectId}});success(reply);QCOMPARE(payload(reply)["menu"].toArray().size(),4);
        QCOMPARE(payload(reply)["db_conn_str"].toObject()["host"].toString(),QString("127.0.0.1"));
        success(call("/api/project/edit",{{"id",projectId},{"name",QString("O'Brien_%!_%1").arg(projectId)}}));
        reply=call("/api/project/page",{{"name",QString("O'Brien_%!_%1").arg(projectId)},{"page",1},{"limit",1}});success(reply);QCOMPARE(payload(reply)["total"].toInt(),1);
    }
    void nativeFileImportAndRollback() {
        const QString road=write(u8"道路.csv","id,link_id,name,type,capacity,length,free_flow_time,wkt\n1,10,A,1,1200,1.5,0.1,\"LINESTRING(104 30,104.1 30.1)\"\n2,20,B,1,900,2,0.2,\"LINESTRING(104.1 30.1,104.2 30.2)\"\n");
        QFile f(road);QVERIFY(f.open(QIODevice::ReadOnly));const auto hash=QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256);f.close();
        auto reply=call("/api/files/drcsv",{{"project_id",projectId},{"case_id",0},{"table","road_way"}},road);success(reply);QCOMPARE(payload(reply)["imported"].toInt(),2);
        const QString invalid=write("invalid.csv","id,capacity\n5,invalid-number\n");
        reply=call("/api/files/drcsv",{{"project_id",projectId},{"table","road_way"}},invalid);QVERIFY(reply["code"].toInt()!=200);
        auto cfg=OfflineApi::layerConnection(QString("tnp2_project%1_user1_road_way").arg(projectId));QCOMPARE(cfg["wkb_type"].toString(),QString("LINESTRING"));
        reply=call("/api/project/select_data_page",{{"tablefullname",cfg["table"].toString()},{"page",1},{"limit",1}});success(reply);QCOMPARE(payload(reply)["total"].toInt(),2);QCOMPARE(payload(reply)["data"].toArray().size(),1);
        QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256),hash);
    }
    void sqliteReadOnlyAndCoordinates() {
        const QString file=files.filePath(u8"公交.sqlite"),name="phase2_fixture";
        {auto source=QSqlDatabase::addDatabase("QSQLITE",name);source.setDatabaseName(file);QVERIFY(source.open());QSqlQuery query(source);QVERIFY(query.exec("CREATE TABLE bus_nodes(id INTEGER,node_id INTEGER,name TEXT,longitude REAL,latitude REAL)"));QVERIFY(query.exec("INSERT INTO bus_nodes VALUES(1,100,'station',104,30)"));source.close();}
        QSqlDatabase::removeDatabase(name);QFile source(file);QVERIFY(source.open(QIODevice::ReadOnly));const auto hash=QCryptographicHash::hash(source.readAll(),QCryptographicHash::Sha256);source.close();
        QVERIFY(OfflineApi::sourceLayers(file).contains("bus_nodes"));
        auto reply=call("/api/files/drcsv",{{"project_id",projectId},{"table","bus_point"}},file+"|layername=bus_nodes");success(reply);QCOMPARE(payload(reply)["imported"].toInt(),1);
        QVERIFY(source.open(QIODevice::ReadOnly));QCOMPARE(QCryptographicHash::hash(source.readAll(),QCryptographicHash::Sha256),hash);
        OfflineStore store;QVERIFY(store.openLocal(qEnvironmentVariable("TNP_OFFLINE_CONFIG")));QSqlQuery q(store.database());QVERIFY(q.exec(QString("SELECT ST_X(geom),ST_Y(geom) FROM tnp2_data.tnp2_project%1_user1_bus_point").arg(projectId)));QVERIFY(q.next());QCOMPARE(q.value(0).toDouble(),104.0);QCOMPARE(q.value(1).toDouble(),30.0);
    }
    void caseSnapshotCloneAndIsolation() {
        auto reply=call("/api/case/add",{{"project_id",projectId},{"name",u8"基础方案-验收"}});success(reply);baseId=payload(reply)["id"].toInt();QVERIFY(baseId>0);QCOMPARE(payload(reply)["isbase"].toInt(),1);QCOMPARE(payload(reply)["list"].toArray().size(),4);
        reply=call("/api/case/copy",{{"case_id",baseId},{"name",u8"改造方案-验收"}});success(reply);copyId=payload(reply)["id"].toInt();QVERIFY(copyId>0);QCOMPARE(payload(reply)["isbase"].toInt(),0);
        success(call("/api/project/clrtable",{{"project_id",projectId},{"table","road_way"}}));
        reply=call("/api/case/detail/"+QString::number(copyId));success(reply);QCOMPARE(payload(reply)["case_count"].toObject()["road_way"].toInt(),2);QCOMPARE(payload(reply)["status"].toInt(),0);
        success(call("/api/case/fieldbatch",{{"project_id",projectId},{"case_id",copyId},{"tablefullname","road_way"},{"field","capacity"},{"type","multiply"},{"num",2}}));
        OfflineStore store;QVERIFY(store.openLocal(qEnvironmentVariable("TNP_OFFLINE_CONFIG")));QSqlQuery query(store.database());
        QVERIFY(query.exec(QString("SELECT capacity FROM tnp2_data.tnp2_project%1_user1_case%2_road_way WHERE id=1").arg(projectId).arg(copyId)));QVERIFY(query.next());QCOMPARE(query.value(0).toDouble(),2400.0);
        QVERIFY(query.exec(QString("SELECT capacity FROM tnp2_data.tnp2_project%1_user1_case%2_road_way WHERE id=1").arg(projectId).arg(baseId)));QVERIFY(query.next());QCOMPARE(query.value(0).toDouble(),1200.0);
    }
    void exportsAndUnsupportedEngines() {
        for(const QString& ext:QStringList{"csv","xlsx"}) {
            auto reply=call("/api/project/export",{{"project_id",projectId},{"case_id",copyId},{"table","road_way"},{"type",ext}});success(reply);QCOMPARE(payload(reply)["recordCount"].toInt(),2);
            const QString path=QUrl(payload(reply)["downloadUrl"].toString()).toLocalFile();QVERIFY(QFileInfo::exists(path));QVERIFY(!OfflineApi::sourceLayers(path).isEmpty());QFile::remove(path);
        }
        auto reply=call("/api/grpc/runalg",{{"project_id",projectId},{"case_id",copyId}});QVERIFY(reply["code"].toInt()!=200);
        QVERIFY(reply["message"].toString().contains(u8"离线"));
        reply=call("/api/case/edit-status",{{"id",copyId},{"status",1}});QVERIFY(reply["code"].toInt()!=200);
        reply=call("/api/project/clrtable",{{"project_id",projectId},{"table","road_way;DROP SCHEMA tnp2_data"}});QVERIFY(reply["code"].toInt()!=200);
    }
    void numericBatchContractAndUntruncatedRead() {
        const QVariantMap common{{"project_id",projectId},{"case_id",copyId},{"tablefullname","road_way"},{"field","capacity"},{"num",100}};
        auto args=common;args["type"]=1;success(call("/api/case/fieldbatch",args));
        args["type"]=2;args["num"]=50;success(call("/api/case/fieldbatch",args));
        args["type"]=3;args["num"]=2;success(call("/api/case/fieldbatch",args));
        const QString table=QString("tnp2_project%1_user1_case%2_road_way").arg(projectId).arg(copyId);
        auto reply=call("/api/project/select_data_all",{{"tablefullname",table}});success(reply);QCOMPARE(reply["data"].toArray()[0].toObject()["capacity"].toDouble(),4900.0);
        OfflineStore store;QVERIFY(store.openLocal(qEnvironmentVariable("TNP_OFFLINE_CONFIG")));QSqlQuery query(store.database());
        QVERIFY(query.exec(QString("INSERT INTO tnp2_data.%1 (id) SELECT generate_series(100,1200)").arg(table)));
        reply=call("/api/project/select_data_all",{{"tablefullname",table}});success(reply);QCOMPARE(reply["data"].toArray().size(),1103);
    }
    void cleanupTestCase(){if(projectId)success(call("/api/project/del",{{"id",projectId}}));OfflineApi::shutdown();}
};
QTEST_GUILESS_MAIN(Phase2OfflineTests)
#include "tests.moc"
