#pragma once
#include "qrequest2.h"
#include "offlineapi.h"

class ApiRequest {
public:
    static ApiRequest& instance() { static ApiRequest value; return value; }
    using Callback=QRequest2::QRequest2CallBack;
    using Headers=QRequest2::KnownHeaderMap;
    template<class... Extra>
    void get(const QString& url,const Callback& cb,const QVariantMap& p={},const Headers& h={},const QVariantMap& r={},Extra&&... extra) {
        if(OfflineApi::enabled())OfflineApi::dispatch("GET",url,p,cb);
        else QRequest2::instance().get(url,cb,p,h,r,std::forward<Extra>(extra)...);
    }
    template<class... Extra>
    void post(const QString& url,const Callback& cb,const QVariantMap& p={},const Headers& h={},const QVariantMap& r={},Extra&&... extra) {
        if(OfflineApi::enabled())OfflineApi::dispatch("POST",url,p,cb);
        else QRequest2::instance().post(url,cb,p,h,r,std::forward<Extra>(extra)...);
    }
    template<class... Extra>
    void put(const QString& url,const Callback& cb,const QVariantMap& p={},const Headers& h={},const QVariantMap& r={},Extra&&... extra) {
        if(OfflineApi::enabled())OfflineApi::dispatch("PUT",url,p,cb);
        else QRequest2::instance().put(url,cb,p,h,r,std::forward<Extra>(extra)...);
    }
    template<class... Extra>
    void remove(const QString& url,const Callback& cb,const QVariantMap& p={},const Headers& h={},const QVariantMap& r={},Extra&&... extra) {
        if(OfflineApi::enabled())OfflineApi::dispatch("DELETE",url,p,cb);
        else QRequest2::instance().remove(url,cb,p,h,r,std::forward<Extra>(extra)...);
    }
    template<class... Extra>
    void postFile(const QString& file,const QString& url,const Callback& cb,const QVariantMap& p={},const QString& field={},const Headers& h={},const QVariantMap& r={},Extra&&... extra) {
        if(OfflineApi::enabled())OfflineApi::dispatch("POST",url,p,cb,file);
        else QRequest2::instance().postFile(file,url,cb,p,field,h,r,std::forward<Extra>(extra)...);
    }
};
