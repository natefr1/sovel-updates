#include "windows.h"
#include <QSocketNotifier>
#include <QHash>
#include <QSet>
#include <algorithm>
#include <vector>
#ifdef SOVEL_LAYER_SHELL
#include <wayland-client.h>
#include "foreign-toplevel.h"
#endif

struct Windows::Impl {
    Windows *owner;
    bool initial=true,manageNewApps=false;
    struct Entry {
        Impl *impl; int id; QString title, app;
        bool minimized=false, maximized=false, active=false, fullscreen=false;
        bool configured=false,hasParent=false;
        quint64 lastActivated=0;
#ifdef SOVEL_LAYER_SHELL
        zwlr_foreign_toplevel_handle_v1 *handle=nullptr;
        QSet<wl_output*> outputs;
#endif
        QStringList outputNames;
        QString name() const {
            if(app.startsWith("sovel-")) {
                auto value=app.mid(6); if(!value.isEmpty())value[0]=value[0].toUpper();
                return value=="Library" ? "App Library" : value;
            }
            if(app.contains("brave",Qt::CaseInsensitive))return "Brave";
            auto value=app.section('.',-1); if(value.isEmpty())value=title;
            if(!value.isEmpty())value[0]=value[0].toUpper();
            return value;
        }
    };
    std::vector<std::unique_ptr<Entry>> items;
    int nextId=1,lastActive=0;
    quint64 activationSerial=0;
    Entry *active() const {
        for(auto &e:items)if(e->active)return e.get();
        for(auto &e:items)if(e->id==lastActive && !e->minimized)return e.get();
        return nullptr;
    }
#ifdef SOVEL_LAYER_SHELL
    wl_display *display=nullptr; wl_registry *registry=nullptr; wl_seat *seat=nullptr;
    zwlr_foreign_toplevel_manager_v1 *manager=nullptr;
    QHash<uint32_t,wl_output*> outputGlobals;
    QHash<wl_output*,QString> outputNames;
    std::unique_ptr<QSocketNotifier> notifier;
    static void title(void *p,zwlr_foreign_toplevel_handle_v1*,const char *s){static_cast<Entry*>(p)->title=QString::fromUtf8(s);}
    static void appId(void *p,zwlr_foreign_toplevel_handle_v1*,const char *s){static_cast<Entry*>(p)->app=QString::fromUtf8(s);}
    static void enterOutput(void *p,zwlr_foreign_toplevel_handle_v1*,wl_output *output){static_cast<Entry*>(p)->outputs.insert(output);}
    static void leaveOutput(void *p,zwlr_foreign_toplevel_handle_v1*,wl_output *output){static_cast<Entry*>(p)->outputs.remove(output);}
    void updateOutputs(Entry &entry){
        QStringList names;
        for(auto *output:entry.outputs){
            const auto name=outputNames.value(output);
            if(!name.isEmpty())names.append(name);
        }
        if(!names.isEmpty()||!entry.minimized)entry.outputNames=names;
    }
    static void state(void *p,zwlr_foreign_toplevel_handle_v1*,wl_array *a){
        auto *e=static_cast<Entry*>(p);e->minimized=e->maximized=e->active=e->fullscreen=false;
        auto *values=static_cast<uint32_t*>(a->data);
        for(size_t i=0;i<a->size/sizeof(uint32_t);++i){
            switch(values[i]){case 0:e->maximized=true;break;case 1:e->minimized=true;break;case 2:e->active=true;break;case 3:e->fullscreen=true;break;}
        }
        if(e->active){e->impl->lastActive=e->id;e->lastActivated=++e->impl->activationSerial;}
    }
    static void done(void *p,zwlr_foreign_toplevel_handle_v1*){
        auto *e=static_cast<Entry*>(p);auto *impl=e->impl;
        impl->updateOutputs(*e);
        if(!e->configured && !e->app.isEmpty()){
            e->configured=true;
            if(impl->manageNewApps && !impl->initial && !e->hasParent && !e->app.startsWith("sovel-") && e->app!="foot"){
                zwlr_foreign_toplevel_handle_v1_set_maximized(e->handle);impl->flush();
            }
        }
        emit impl->owner->changed();
    }
    static void closed(void *p,zwlr_foreign_toplevel_handle_v1 *handle){
        auto *e=static_cast<Entry*>(p);auto *impl=e->impl;
        zwlr_foreign_toplevel_handle_v1_destroy(handle);
        impl->items.erase(std::remove_if(impl->items.begin(),impl->items.end(),[e](auto &item){return item.get()==e;}),impl->items.end());
        emit impl->owner->changed();
    }
    static void parent(void *p,zwlr_foreign_toplevel_handle_v1*,zwlr_foreign_toplevel_handle_v1 *parent){static_cast<Entry*>(p)->hasParent=parent!=nullptr;}
    static void created(void *p,zwlr_foreign_toplevel_manager_v1*,zwlr_foreign_toplevel_handle_v1 *handle){
        auto *impl=static_cast<Impl*>(p);auto e=std::make_unique<Entry>();
        e->impl=impl;e->id=impl->nextId++;e->handle=handle;
        static const zwlr_foreign_toplevel_handle_v1_listener listener={title,appId,enterOutput,leaveOutput,state,done,closed,parent};
        zwlr_foreign_toplevel_handle_v1_add_listener(handle,&listener,e.get());
        impl->items.push_back(std::move(e));
    }
    static void finished(void *p,zwlr_foreign_toplevel_manager_v1 *manager){
        auto *impl=static_cast<Impl*>(p);zwlr_foreign_toplevel_manager_v1_destroy(manager);impl->manager=nullptr;
    }
    static void geometry(void*,wl_output*,int32_t,int32_t,int32_t,int32_t,int32_t,const char*,const char*,int32_t){}
    static void mode(void*,wl_output*,uint32_t,int32_t,int32_t,int32_t){}
    static void outputDone(void*,wl_output*){}
    static void scale(void*,wl_output*,int32_t){}
    static void outputName(void *p,wl_output *output,const char *name){
        auto *impl=static_cast<Impl*>(p);
        impl->outputNames[output]=QString::fromUtf8(name);
        for(auto &entry:impl->items)impl->updateOutputs(*entry);
        emit impl->owner->changed();
    }
    static void description(void*,wl_output*,const char*){}
    static void global(void *p,wl_registry *registry,uint32_t name,const char *interface,uint32_t version){
        auto *impl=static_cast<Impl*>(p);
        if(QString::fromLatin1(interface)=="zwlr_foreign_toplevel_manager_v1"){
            impl->manager=static_cast<zwlr_foreign_toplevel_manager_v1*>(wl_registry_bind(registry,name,&zwlr_foreign_toplevel_manager_v1_interface,std::min(version,3u)));
            static const zwlr_foreign_toplevel_manager_v1_listener listener={created,finished};
            zwlr_foreign_toplevel_manager_v1_add_listener(impl->manager,&listener,impl);
        }else if(QString::fromLatin1(interface)=="wl_seat" && !impl->seat){
            impl->seat=static_cast<wl_seat*>(wl_registry_bind(registry,name,&wl_seat_interface,1));
        }else if(QString::fromLatin1(interface)=="wl_output"){
            auto *output=static_cast<wl_output*>(wl_registry_bind(registry,name,&wl_output_interface,std::min(version,4u)));
            static const wl_output_listener listener={geometry,mode,outputDone,scale,outputName,description};
            wl_output_add_listener(output,&listener,impl);
            impl->outputGlobals.insert(name,output);
        }
    }
    static void removed(void *p,wl_registry*,uint32_t name){
        auto *impl=static_cast<Impl*>(p);
        auto *output=impl->outputGlobals.take(name);
        if(!output)return;
        const auto outputName=impl->outputNames.take(output);
        for(auto &entry:impl->items){
            entry->outputs.remove(output);
            entry->outputNames.removeAll(outputName);
            impl->updateOutputs(*entry);
        }
        wl_output_destroy(output);
        emit impl->owner->changed();
    }
    void flush(){if(display)wl_display_flush(display);}
#endif
    explicit Impl(Windows *w,bool manage):owner(w),manageNewApps(manage){
#ifdef SOVEL_LAYER_SHELL
        if(qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))return;
        display=wl_display_connect(nullptr);if(!display)return;
        registry=wl_display_get_registry(display);
        static const wl_registry_listener listener={global,removed};
        wl_registry_add_listener(registry,&listener,this);
        if(wl_display_roundtrip(display)<0)return;
        if(wl_display_roundtrip(display)<0)return;
        initial=false;
        notifier=std::make_unique<QSocketNotifier>(wl_display_get_fd(display),QSocketNotifier::Read);
        QObject::connect(notifier.get(),&QSocketNotifier::activated,owner,[this]{
            if(wl_display_dispatch(display)<0){notifier->setEnabled(false);items.clear();emit owner->changed();}
        });
#endif
    }
    ~Impl(){
#ifdef SOVEL_LAYER_SHELL
        notifier.reset();
        for(auto &e:items)zwlr_foreign_toplevel_handle_v1_destroy(e->handle);
        if(manager)zwlr_foreign_toplevel_manager_v1_destroy(manager);
        if(seat)wl_seat_destroy(seat);
        for(auto *output:outputGlobals)wl_output_destroy(output);
        if(registry)wl_registry_destroy(registry);
        if(display)wl_display_disconnect(display);
#endif
    }
};
Windows::Windows(QObject *parent,bool manage):QObject(parent),d(std::make_unique<Impl>(this,manage)){}
Windows::~Windows()=default;
QString Windows::activeName()const{auto *e=d->active();return e?e->name():"Desktop";}
QString Windows::activeAppId()const{auto *e=d->active();return e?e->app:QString();}
bool Windows::hasWindows()const{for(auto &e:d->items)if(!e->minimized)return true;return false;}
bool Windows::activeMaximized()const{auto *e=d->active();return e && (e->maximized||e->fullscreen);}
QVariantList Windows::entries()const{
    QVariantList result;for(auto &e:d->items)result.append(QVariantMap{{"id",e->id},{"name",e->name()},{"title",e->title},{"appId",e->app},{"active",e->active},{"recent",e->lastActivated},{"maximized",e->maximized||e->fullscreen},{"minimized",e->minimized},{"outputs",e->outputNames}});return result;
}
bool Windows::activateApp(const QStringList &appIds,bool toggleActive){
#ifdef SOVEL_LAYER_SHELL
    auto key=[](QString value){if(value.endsWith(".desktop",Qt::CaseInsensitive))value.chop(8);return value.toCaseFolded();};
    Impl::Entry *match=nullptr;
    for(auto &entry:d->items){
        bool matches=false;for(const auto &id:appIds)if(!id.isEmpty()&&key(id)==key(entry->app)){matches=true;break;}
        if(matches&&(!match||(entry->active&&!match->active)||(entry->active==match->active&&(entry->lastActivated>match->lastActivated||(entry->lastActivated==match->lastActivated&&entry->id>match->id)))))match=entry.get();
    }
    if(!match||!d->seat)return false;
    if(toggleActive&&(match->active||match->id==d->lastActive)&&!match->minimized)zwlr_foreign_toplevel_handle_v1_set_minimized(match->handle);
    else {zwlr_foreign_toplevel_handle_v1_unset_minimized(match->handle);zwlr_foreign_toplevel_handle_v1_activate(match->handle,d->seat);}
    d->flush();return true;
#else
    Q_UNUSED(appIds) Q_UNUSED(toggleActive) return false;
#endif
}
void Windows::activate(int id){
#ifdef SOVEL_LAYER_SHELL
    for(auto &e:d->items)if(e->id==id && d->seat){zwlr_foreign_toplevel_handle_v1_unset_minimized(e->handle);zwlr_foreign_toplevel_handle_v1_activate(e->handle,d->seat);d->flush();break;}
#else
    Q_UNUSED(id)
#endif
}
void Windows::closeActive(){
#ifdef SOVEL_LAYER_SHELL
    if(auto *e=d->active()){zwlr_foreign_toplevel_handle_v1_close(e->handle);d->flush();}
#endif
}
int Windows::closeAll(){
#ifdef SOVEL_LAYER_SHELL
    int asked=0;
    for(auto &e:d->items){zwlr_foreign_toplevel_handle_v1_close(e->handle);++asked;}
    d->flush();return asked;
#else
    return 0;
#endif
}
int Windows::count()const{return int(d->items.size());}
void Windows::minimizeActive(){
#ifdef SOVEL_LAYER_SHELL
    if(auto *e=d->active()){zwlr_foreign_toplevel_handle_v1_set_minimized(e->handle);d->flush();}
#endif
}
void Windows::toggleMaximized(){
#ifdef SOVEL_LAYER_SHELL
    if(auto *e=d->active()){
        if(e->fullscreen)zwlr_foreign_toplevel_handle_v1_unset_fullscreen(e->handle);
        else if(e->maximized)zwlr_foreign_toplevel_handle_v1_unset_maximized(e->handle);
        else zwlr_foreign_toplevel_handle_v1_set_maximized(e->handle);
        d->flush();
    }
#endif
}
