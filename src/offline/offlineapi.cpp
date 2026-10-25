#include "offlineapi.h"
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <memory>
#include <stdexcept>

namespace {
std::unique_ptr<OfflineStore> store;
struct Failure { QString message; };
QString qi(const QString& s) { return OfflineStore::quoteIdentifier(s); }
QString qualified(const QString& s) { return "tnp2_data." + qi(s); }
QString prefix(int p, int c=0) { return QString("tnp2_project%1_user1_").arg(p)+(c ? QString("case%1_").arg(c) : QString()); }
QSqlDatabase db() { return store->database(); }
QSqlQuery sql(const QString& text, const QVariantList& values={}) {
    QSqlQuery q(db());
    if(values.isEmpty()) { if(!q.exec(text))throw Failure{q.lastError().text()};return q; }
    if (!q.prepare(text)) throw Failure{q.lastError().text()};
    for (const auto& v:values) q.addBindValue(v);
    if (!q.exec()) throw Failure{q.lastError().text()};
    return q;
}
QJsonValue scalarJson(const QVariant& value) {
    QJsonDocument doc=QJsonDocument::fromJson(value.toByteArray());
    return doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
}
QJsonArray rows(const QString& query, const QVariantList& values={}) {
    QJsonArray arr; auto q=sql("SELECT row_to_json(t)::text FROM ("+query+") t",values);
    while(q.next()) arr.append(scalarJson(q.value(0))); return arr;
}
QJsonObject one(const QString& query, const QVariantList& values={}) {
    const auto arr=rows(query,values); if(arr.isEmpty()) throw Failure{u8"工程或方案不存在，请刷新列表。"}; return arr.first().toObject();
}
QJsonObject ok(const QJsonValue& value=true) { return {{"code",200},{"message",u8"操作成功"},{"data",value}}; }
struct Definition { const char* key; const char* label; const char* columns; };
const Definition definitions[]={
 {"road_point",u8"路网节点","node_id bigint,name text,longitude double precision,latitude double precision,type integer DEFAULT 1"},
 {"road_way",u8"路网路段","link_id bigint,init_node bigint,term_node bigint,from_node bigint,to_node bigint,name text,length double precision,capacity double precision,free_flow_time double precision,free_speed double precision,b double precision DEFAULT 0.15,power double precision DEFAULT 4,speed double precision,toll double precision DEFAULT 0,link_type integer DEFAULT 1,type integer DEFAULT 1,volume double precision"},
 {"road_community",u8"交通小区","name text,zone_id bigint,area_id bigint,pop_sum double precision,num_grids integer,seed_oid bigint"},
 {"bus_point",u8"公交站点","name text,node_id bigint,longitude double precision,latitude double precision"},
 {"bus_way",u8"公交线路","name text,link_id bigint,init_node bigint,term_node bigint,type integer DEFAULT 1,capacity double precision,free_flow_time double precision,route_id text,headway_min double precision,length double precision"},
 {"bus_community",u8"公交小区","name text,zone_id bigint,area_id bigint,pop_sum double precision,num_grids integer,seed_oid bigint"},
 {"slow_road_point",u8"慢行节点","name text,node_id bigint,longitude double precision,latitude double precision,type integer DEFAULT 1"},
 {"slow_road_way",u8"慢行路网","name text,link_id bigint,init_node bigint,term_node bigint,type integer DEFAULT 1,capacity double precision,free_flow_time double precision,mode text,length double precision,free_speed double precision"},
 {"slow_road_community",u8"慢行小区","name text,zone_id bigint,area_id bigint,pop_sum double precision,num_grids integer,seed_oid bigint"},
 {"other_od",u8"OD需求","f_id bigint,t_id bigint,origin bigint,destination bigint,demand double precision,mode text"},
 {"other_observation",u8"路段观测流量","link_id bigint,flow double precision,observed_flow double precision"},
 {"other_taz_socioeconomic",u8"小区社会经济数据","name text,longitude double precision,latitude double precision,pop double precision,cars double precision,eind double precision,eret double precision,eoth double precision,du double precision,hinc double precision,area double precision,etot double precision"},
 {"other_population",u8"人口数据","name text,population double precision,longitude double precision,latitude double precision"},
 {"other_toll_gate",u8"收费站数据","name text,flow double precision"},
 {"other_mobile_cdr",u8"手机信令","name text,flow double precision"},
 {"other_bus_route",u8"公交客流","name text,flow double precision"},
 {"other_bus_customized",u8"定制公交","name text,flow double precision"},
 {"other_bus_school",u8"校车数据","name text,flow double precision"},
 {"other_taxi_trip",u8"出租车出行","f_id bigint,t_id bigint,demand double precision"}
};
void ensureTables(int p) {
    for(const auto& d:definitions) {
        QString key=d.key, name=prefix(p)+key;
        auto exists=sql("SELECT 1 FROM tnp2_local.data_tables WHERE project_id=? AND case_id=0 AND logical_name=?",{p,key});
        if(exists.next()) continue;
        sql("CREATE TABLE IF NOT EXISTS "+qualified(name)+" (id bigserial PRIMARY KEY,"+QString::fromUtf8(d.columns)+",geom public.geometry(Geometry,4326))");
        sql("INSERT INTO tnp2_local.data_tables VALUES(?,0,?,?,?,?)",{p,key,name,QString::fromUtf8(d.label),key.section('_',0,0)});
        // Copy any previously supplied workbench data once; originals remain intact.
        auto source=sql("SELECT to_regclass(?) IS NOT NULL",{"tnp2_data."+key}); source.next();
        if(p==1 && source.value(0).toBool()) {
            auto cols=sql("SELECT column_name FROM information_schema.columns WHERE table_schema='tnp2_data' AND table_name=? AND column_name IN (SELECT column_name FROM information_schema.columns WHERE table_schema='tnp2_data' AND table_name=?) ORDER BY ordinal_position",{key,name});
            QStringList names; while(cols.next()) if(cols.value(0).toString()!="project_id") names<<qi(cols.value(0).toString());
            if(!names.isEmpty()) sql("INSERT INTO "+qualified(name)+" ("+names.join(',')+") SELECT "+names.join(',')+" FROM "+qualified(key));
        }
    }
}
QJsonObject connection(int p,int c=0) {
    const auto cfg=store->connectionConfig();
    return {{"host",cfg["host"]},{"port",cfg["port"]},{"dbname",cfg["database"]},
            {"user",cfg["user"]},{"password",store->databasePassword()},{"role","tnp2_data"},{"scenario_prefix",prefix(p,c)}};
}
QString tableFor(int p,int c,const QString& input) {
    const QString name=input.section('.',-1).remove('"');
    auto q=sql("SELECT physical_name FROM tnp2_local.data_tables WHERE project_id=? AND case_id=? AND (logical_name=? OR physical_name=?)",{p,c,name,name});
    if(q.next()) return q.value(0).toString();
    // Native algorithm result tables carry the same scenario prefix.
    if(name.startsWith(prefix(p,c)) && QRegularExpression("^[a-zA-Z0-9_]+$").match(name).hasMatch()) {
        auto found=sql("SELECT to_regclass(?) IS NOT NULL",{"tnp2_data."+name});found.next(); if(found.value(0).toBool())return name;
    }
    throw Failure{u8"所选工程或方案中没有这张表："+name};
}
QJsonArray menu(int p,int c=0) {
    QJsonArray result;
    for(const QString& category:QStringList{"road","bus","slow","other"}) {
        QJsonArray children;
        auto q=sql("SELECT logical_name,physical_name,display_name FROM tnp2_local.data_tables WHERE project_id=? AND case_id=? AND category=? ORDER BY logical_name",{p,c,category});
        while(q.next())children.append(QJsonObject{{"name",q.value(2).toString()},{"value",q.value(1).toString()},{"val",q.value(0).toString()},{"level",1}});
        if(!children.isEmpty())result.append(QJsonObject{{"name",category=="road"?u8"机动车":category=="bus"?u8"公交":category=="slow"?u8"慢行":u8"其他数据"},{"value",category},{"children",children}});
    }
    return result;
}
QJsonObject counts(int p,int c) {
    QJsonObject result; auto q=sql("SELECT logical_name,physical_name FROM tnp2_local.data_tables WHERE project_id=? AND case_id=?",{p,c});
    while(q.next()){auto n=sql("SELECT count(*) FROM "+qualified(q.value(1).toString()));n.next();result[q.value(0).toString()]=n.value(0).toDouble();} return result;
}
QJsonObject project(int p) {
    return one("SELECT p.id,p.name,p.province_id,p.province,p.city_id,p.city,p.description AS desc,1 AS status,0 AS sort,ARRAY[1] AS users,(SELECT count(*) FROM tnp2_local.cases c WHERE c.project_id=p.id) AS num,p.created_at,p.updated_at FROM tnp2_local.projects p WHERE p.id=?",{p});
}
QJsonObject caseDetail(int id) {
    auto item=one("SELECT *,1 AS user_id,0 AS flag FROM tnp2_local.cases WHERE id=?",{id});
    int p=item["project_id"].toInt();item["list"]=menu(p,id);item["db_conn_str"]=connection(p,id);item["menu"]=menu(p,id);
    item["case_count"]=counts(p,id);item["algorithm_params"]=QJsonObject{{"conv",0.0001},{"max_iter",100},{"cost_scalar",1}}; return item;
}
QJsonArray caseTypes(int p) {
    QJsonArray result;
    const QStringList names={u8"路网现状分析",u8"路网承载力分析",u8"OD反推分析",u8"路网提升分析"};
    const QStringList keys={"network","capacity","od","enhance"};
    const auto n=counts(p,0);
    QJsonObject road{{"point",true},{"way",true},{"community",false}},other{{"od",true},{"observation",false},{"taz_socioeconomic",false}};
    auto tips=[&](const QString& cat){ QJsonArray arr; for(const auto& d:definitions) {QString k=d.key;if(k.startsWith(cat+"_"))arr.append(QJsonObject{{"name",k.mid(cat.size()+1)},{"havedata",n[k].toDouble()>0},{"tip",n[k].toDouble()>0?u8"本地已有数据":u8"请先导入本地数据"}});} return arr;};
    for(int i=0;i<names.size();++i) result.append(QJsonObject{{"id",i+1},{"name",names[i]},{"key",keys[i]},{"status",1},
      {"road_types",road},{"bus_types",QJsonObject{}},{"other_types",other},{"other_types_must",QJsonObject{}},
      {"road_types_data",tips("road")},{"bus_types_data",tips("bus")},{"other_types_data",tips("other")}});
    return result;
}
using Dataset=std::unique_ptr<GDALDataset,void(*)(GDALDataset*)>;
Dataset source(const QString& path) {
    GDALAllRegister();
    char** options=nullptr;
    if(QFileInfo(path).suffix().compare("csv",Qt::CaseInsensitive)==0 || QFileInfo(path).suffix().compare("txt",Qt::CaseInsensitive)==0) {
        options=CSLSetNameValue(options,"AUTODETECT_TYPE","YES");
        options=CSLSetNameValue(options,"GEOM_POSSIBLE_NAMES","geom,geometry,wkt,WKT");
    }
    const QString input=QFileInfo(path).suffix().compare("txt",Qt::CaseInsensitive)==0?"CSV:"+path:path;
    auto* ds=static_cast<GDALDataset*>(GDALOpenEx(input.toUtf8().constData(),GDAL_OF_VECTOR|GDAL_OF_READONLY,nullptr,options,nullptr));
    CSLDestroy(options); if(!ds)throw Failure{u8"无法读取本地文件，请检查文件格式。"}; return Dataset(ds,[](GDALDataset* d){GDALClose(d);});
}
QStringList columns(const QString& table) {
    QStringList result;auto q=sql("SELECT column_name FROM information_schema.columns WHERE table_schema='tnp2_data' AND table_name=? ORDER BY ordinal_position",{table});
    while(q.next())result<<q.value(0).toString();return result;
}
int importLayer(OGRLayer* layer,const QString& target) {
    if(!layer)throw Failure{u8"文件中找不到所选数据表。"};
    auto* def=layer->GetLayerDefn(); QStringList names=columns(target), inserts, expressions;QList<int> fields;
    for(int i=0;i<def->GetFieldCount();++i) {
        auto* f=def->GetFieldDefn(i);QString name=QString::fromUtf8(f->GetNameRef()).trimmed().toLower();
        if(name.isEmpty()||name=="geom"||name=="geometry"||name=="wkt"||name=="project_id")continue;
        if(!names.contains(name)) {
            QString type=f->GetType()==OFTInteger||f->GetType()==OFTInteger64?"bigint":f->GetType()==OFTReal?"double precision":"text";
            sql("ALTER TABLE "+qualified(target)+" ADD COLUMN "+qi(name)+" "+type); names<<name;
        }
        fields<<i;inserts<<qi(name);expressions<<"?";
    }
    inserts<<"geom"; expressions<<"ST_GeomFromWKB(?,4326)";
    QSqlQuery put(db()); if(!put.prepare("INSERT INTO "+qualified(target)+" ("+inserts.join(',')+") VALUES ("+expressions.join(',')+")"))throw Failure{put.lastError().text()};
    OGRSpatialReference destination;destination.importFromEPSG(4326);destination.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    auto* original=layer->GetSpatialRef();std::unique_ptr<OGRSpatialReference> crs(original?original->Clone():nullptr);
    std::unique_ptr<OGRCoordinateTransformation,void(*)(OGRCoordinateTransformation*)> transform(nullptr,[](OGRCoordinateTransformation* t){OCTDestroyCoordinateTransformation(t);});
    if(crs){crs->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);transform.reset(OGRCreateCoordinateTransformation(crs.get(),&destination));if(!transform)throw Failure{u8"无法转换数据坐标系。"};}
    const int longitude=def->GetFieldIndex("longitude"),latitude=def->GetFieldIndex("latitude");
    int count=0;layer->ResetReading();
    while(auto* raw=layer->GetNextFeature()) {
        std::unique_ptr<OGRFeature,void(*)(OGRFeature*)> feature(raw,OGRFeature::DestroyFeature);
        int b=0; for(int i:fields) {auto* f=def->GetFieldDefn(i); QVariant value;
            if(feature->IsFieldSetAndNotNull(i) && !(f->GetType()==OFTString && QString::fromUtf8(feature->GetFieldAsString(i)).isEmpty())) {
                if(f->GetType()==OFTInteger||f->GetType()==OFTInteger64)value=qlonglong(feature->GetFieldAsInteger64(i));
                else if(f->GetType()==OFTReal)value=feature->GetFieldAsDouble(i);
                else value=QString::fromUtf8(feature->GetFieldAsString(i));
            }
            put.bindValue(b++,value);
        }
        std::unique_ptr<OGRGeometry> geometry(feature->GetGeometryRef()?feature->GetGeometryRef()->clone():nullptr);
        if(geometry && transform && geometry->transform(transform.get())!=OGRERR_NONE)throw Failure{u8"数据坐标转换失败，已撤销本次导入。"};
        if(!geometry&&longitude>=0&&latitude>=0&&feature->IsFieldSetAndNotNull(longitude)&&feature->IsFieldSetAndNotNull(latitude)) {
            double x=feature->GetFieldAsDouble(longitude),y=feature->GetFieldAsDouble(latitude);
            if(qAbs(x)>180||qAbs(y)>90)throw Failure{u8"经纬度超出范围，请提供正确坐标系的空间数据。"};geometry.reset(new OGRPoint(x,y));
        }
        if(geometry) {geometry->flattenTo2D();QByteArray wkb(int(geometry->WkbSize()),0);geometry->exportToWkb(wkbNDR,reinterpret_cast<unsigned char*>(wkb.data()));put.bindValue(b,wkb);}
        else put.bindValue(b,QVariant(QVariant::ByteArray));
        if(!put.exec())throw Failure{QString(u8"第 %1 条记录导入失败：%2").arg(count+1).arg(put.lastError().text())};++count;
    }
    auto seq=sql("SELECT pg_get_serial_sequence(?, 'id')",{"tnp2_data."+target});seq.next();
    if(!seq.value(0).isNull())sql("SELECT setval(?::regclass,GREATEST((SELECT coalesce(max(id),1) FROM "+qualified(target)+"),1))",{seq.value(0)});
    return count;
}
QJsonObject exportTable(int p,int c,const QString& input,const QString& extension) {
    const QString target=tableFor(p,c,input);
    QString ext=extension.toLower();if(ext=="txt")ext="csv";
    if(ext!="csv"&&ext!="xlsx")throw Failure{u8"当前离线表格导出支持 CSV 和 XLSX。"};
    const QString root=QFileInfo(qEnvironmentVariable("TNP_OFFLINE_CONFIG")).dir().absolutePath()+"/../data/exports";QDir().mkpath(root);
    const QString path=root+"/"+target+"_"+QUuid::createUuid().toString(QUuid::Id128)+"."+ext;
    QStringList selected;for(const auto& column:columns(target))selected<<(column=="geom"?"ST_AsText(geom) AS wkt":qi(column));
    auto q=sql("SELECT "+selected.join(',')+" FROM "+qualified(target)+" ORDER BY id");
    const auto record=q.record(); auto* driver=GetGDALDriverManager()->GetDriverByName(ext=="xlsx"?"XLSX":ext=="gpkg"?"GPKG":"CSV");
    if(!driver)throw Failure{u8"缺少本地文件导出驱动。"};
    Dataset output(driver->Create(path.toUtf8().constData(),0,0,0,GDT_Unknown,nullptr),[](GDALDataset* d){if(d)GDALClose(d);});
    if(!output)throw Failure{u8"无法创建本地导出文件。"};
    auto* layer=output->CreateLayer("data",nullptr,wkbNone,nullptr);if(!layer)throw Failure{u8"无法创建导出数据表。"};
    for(int i=0;i<record.count();++i) {QString n=record.fieldName(i);OGRFieldDefn f(n.toUtf8().constData(),OFTString);if(layer->CreateField(&f)!=OGRERR_NONE)throw Failure{u8"无法创建导出字段。"};}
    int count=0; while(q.next()) {OGRFeature f(layer->GetLayerDefn());for(int i=0;i<record.count();++i)if(!q.value(i).isNull())f.SetField(i,q.value(i).toString().toUtf8().constData());if(layer->CreateFeature(&f)!=OGRERR_NONE)throw Failure{u8"导出记录失败。"};++count;}
    output.reset();
    return {{"fileName",QFileInfo(path).fileName()},{"downloadUrl",QUrl::fromLocalFile(path).toString()},{"bucket","local"},{"fileSize",double(QFileInfo(path).size())},{"recordCount",count},{"exportType",ext.toUpper()},{"tableName",target}};
}
#include "phase2routes.inc"
} // namespace


bool OfflineApi::enabled() { return !qEnvironmentVariable("TNP_OFFLINE_CONFIG").isEmpty(); }
bool OfflineApi::initialize(QString& error) {
    if(store&&store->isOpen())return true;
    store.reset(new OfflineStore);if(!store->openLocal(qEnvironmentVariable("TNP_OFFLINE_CONFIG"))){error=store->error();store.reset();return false;}
    try {
        QFile migration(qEnvironmentVariable("TNP_OFFLINE_SCHEMA"));if(!migration.open(QIODevice::ReadOnly))throw Failure{u8"缺少离线兼容数据库结构文件，请重新运行构建脚本。"};
        if(!db().transaction())throw Failure{db().lastError().text()};
        const auto statements=QString::fromUtf8(migration.readAll()).split(';',Qt::SkipEmptyParts);
        for(const auto& statement:statements)if(!statement.trimmed().isEmpty())sql(statement);
        auto projects=sql("SELECT id FROM tnp2_local.projects");while(projects.next())ensureTables(projects.value(0).toInt());
        if(!db().commit())throw Failure{db().lastError().text()};
        return true;
    }catch(const Failure& failure){db().rollback();error=failure.message;store.reset();return false;}
}
void OfflineApi::shutdown() { store.reset(); }
QJsonObject OfflineApi::execute(const QString& method,const QString& url,const QVariantMap& params,const QString& file) {
    QString error;if(!initialize(error))return {{"code",503},{"message",error},{"data",QJsonValue()}};
    try {
        QVariantMap values=params;QUrl u(url);QUrlQuery query(u);for(const auto& item:query.queryItems(QUrl::FullyDecoded))values[item.first]=item.second;
        if(!db().transaction())throw Failure{db().lastError().text()};
        QJsonValue data=phase2Route(method,u.path(),values,file);
        if(!db().commit())throw Failure{db().lastError().text()};return ok(data);
    }catch(const Failure& failure){db().rollback();QString safe=failure.message;const QString pwd=store->databasePassword();if(!pwd.isEmpty())safe.replace(pwd,"***");return {{"code",400},{"message",safe},{"data",QJsonValue()}};}
}
void OfflineApi::dispatch(const QString& method,const QString& url,const QVariantMap& params,const Callback& callback,const QString& file) {
    // Queued completion preserves the original async UI behavior (including disabled buttons).
    QTimer::singleShot(0,QCoreApplication::instance(),[=](){auto response=execute(method,url,params,file);const bool ok=response["code"].toInt()==200;callback(ok,ok?QString():response["message"].toString(),QJsonDocument(response).toJson(QJsonDocument::Compact));});
}
QStringList OfflineApi::sourceLayers(const QString& file) {
    try {auto ds=source(file);QStringList names;for(int i=0;i<ds->GetLayerCount();++i)names<<QString::fromUtf8(ds->GetLayer(i)->GetName());return names;}catch(const Failure&){return {};}
}

QJsonObject OfflineApi::layerConnection(const QString& input,QString* error) {
    QString message;if(!initialize(message)){if(error)*error=message;return {};}
    try {const QString table=allowedTable(input);auto result=store->connectionConfig();result["password"]=store->databasePassword();result["table"]=table;result["schema"]="tnp2_data";
        auto q=sql("SELECT f_geometry_column FROM public.geometry_columns WHERE f_table_schema='tnp2_data' AND f_table_name=?",{table});
        if(q.next()) {
            result["geometry"]=q.value(0).toString();
            auto geometry=sql("SELECT GeometryType(geom) FROM "+qualified(table)+" WHERE geom IS NOT NULL LIMIT 1");
            if(geometry.next())result["wkb_type"]=geometry.value(0).toString();
            else if(table.endsWith("_point"))result["wkb_type"]="Point";
            else if(table.endsWith("_way")||table.endsWith("_od"))result["wkb_type"]="LineString";
            else if(table.endsWith("_community"))result["wkb_type"]="Polygon";
            else {result["geometry"]="";result["wkb_type"]="NoGeometry";}
        }
        return result;
    }catch(const Failure& failure){if(error)*error=failure.message;return {};}
}
