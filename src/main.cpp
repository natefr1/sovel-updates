#include "backend.h"
#include "audio.h"
#include "jared.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include "auth.h"
#include "windows.h"
#include "chrome.h"
#include "appicons.h"
#include "store.h"
#include "connectivity.h"
#include "updates.h"
#include "filebrowser.h"
#include "displays.h"
#include "packageinstaller.h"
#include <QQmlEngine>
#include <QPalette>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickItem>
#include "settingsservice.h"
#include "sovelmode.h"
#include <QCoreApplication>
#include <cstdio>
#include "workspace.h"
#include "filechooser.h"
#include "downloads.h"
#include <QScreen>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <memory>
#include <vector>
#include <algorithm>
#include <QHash>
#include <QProcess>
#include <QFileInfo>
#include <QLockFile>
#include <QThread>
#include <QElapsedTimer>
#ifdef SOVEL_LAYER_SHELL
#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>
#endif

// Used by sovel-mode before exiting or sealing the private workspace. Windows
// that do not close (for example, an editor asking to save) are left open.
static int closeAllWindows(int argc,char **argv,int seconds){
    QCoreApplication app(argc,argv);
    Windows windows(nullptr,false);
    QTimer::singleShot(250,&app,[&]{windows.closeAll();});
    QElapsedTimer elapsed;elapsed.start();
    QTimer poll;poll.setInterval(200);
    QObject::connect(&poll,&QTimer::timeout,&app,[&]{
        if(elapsed.elapsed()>500&&windows.count()==0)app.exit(0);
        else if(elapsed.elapsed()>qint64(seconds)*1000)app.exit(3);
    });
    poll.start();
    const int result=app.exec();
    std::printf("{\"remaining\":%d}\n",windows.count());std::fflush(stdout);
    return result;
}

int main(int argc,char **argv){
    for(int i=1;i+1<argc;i++)if(QString(argv[i])=="--close-windows"){
        bool valid=false;const int seconds=QString(argv[i+1]).toInt(&valid);
        return closeAllWindows(argc,argv,valid?qBound(1,seconds,120):20);
    }
    bool desktop=false,releaseInput=false,downloads=false,jaredService=false,privateGreeter=false;QString mode="preview",capture,settingsPage,packageFile,folderPath,jaredPrompt,previewFile,displayOutput;
    for(int i=1;i<argc;i++){
        const QString arg=argv[i];
        if(arg=="--desktop"){desktop=true;mode="desktop";}
        else if(arg=="--greeter")mode="greeter";
        else if(arg=="--private")privateGreeter=true;
        else if(arg=="--release-shell-input")releaseInput=true;
        else if(arg=="--app" && i+1<argc)mode=argv[++i];
        else if(arg=="--jared-prompt" && i+1<argc)jaredPrompt=argv[++i];
        else if(arg=="--jared-service"){mode="jared";jaredService=true;}
        else if(arg=="--capture" && i+1<argc)capture=argv[++i];
        else if(arg=="--settings-page" && i+1<argc)settingsPage=argv[++i];
        else if(arg=="--install-package"&&i+1<argc){mode="package";packageFile=argv[++i];}
        else if(arg=="--downloads"){mode="files";downloads=true;}
        else if(arg=="--preview-file"&&i+1<argc){mode="file-preview";previewFile=argv[++i];}
        else if(arg=="--display-output"&&i+1<argc)displayOutput=argv[++i];
        else if(arg=="--file-chooser-portal")mode="file-chooser";
        else if(arg=="--file-picker-preview"){mode="file-chooser";if(i+1<argc&&!QString(argv[i+1]).startsWith("--"))previewFile=argv[++i];else previewFile="open";}
        else if(arg=="--open-folder"){mode="files";if(i+1<argc&&!QString(argv[i+1]).startsWith("--"))folderPath=argv[++i];}
    }
    // JARED and the Vault exist only in the private account. This is checked
    // against real group membership, so no flag or variable can bypass it.
    // Before Private Mode is set up, JARED stays available in Normal Mode.
    if(((mode=="jared"||jaredService)&&!SovelMode::assistantAvailable())||(mode=="vault"&&!SovelMode::isPrivate())){
        std::fprintf(stderr,"Sovel: this app is available only in the private workspace.\n");
        return 3;
    }
#ifndef SOVEL_LAYER_SHELL
    if(desktop){qFatal("This build has no layer-shell support. Use --preview, or rebuild with SOVEL_LAYER_SHELL=ON.");}
#endif
    // Child apps inherit the shell process environment. They must create normal
    // managed windows, not layer surfaces without close/minimize decorations.
    // Modern LayerShellQt assigns the role per window. Keep ordinary popups on
    // xdg-shell; the obsolete global layer-shell integration breaks QML menus.
    qputenv("QT_WAYLAND_SHELL_INTEGRATION","xdg-shell");
    QGuiApplication app(argc,argv);
    auto iconPaths=QIcon::themeSearchPaths();
    for(const auto &root:QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))iconPaths<<root+"/icons";
    iconPaths<<QDir::homePath()+"/.local/share/flatpak/exports/share/icons"<<"/var/lib/flatpak/exports/share/icons";
    iconPaths.removeDuplicates();QIcon::setThemeSearchPaths(iconPaths);
    if(QIcon::themeName().isEmpty())QIcon::setThemeName("Adwaita");
    QIcon::setFallbackThemeName("hicolor");
    app.setOrganizationName("Sovel");app.setApplicationName("Desktop");
    QPalette palette;
    palette.setColor(QPalette::Window,QColor("#241A32"));
    palette.setColor(QPalette::Base,QColor("#1A1325"));
    palette.setColor(QPalette::Button,QColor("#342542"));
    palette.setColor(QPalette::Text,QColor("#F3ECFF"));
    palette.setColor(QPalette::WindowText,QColor("#F3ECFF"));
    palette.setColor(QPalette::ButtonText,QColor("#F3ECFF"));
    palette.setColor(QPalette::Highlight,QColor("#8C45DF"));
    palette.setColor(QPalette::HighlightedText,Qt::white);
    app.setPalette(palette);
    app.setFont(QFont("Noto Sans"));
    app.setDesktopFileName("sovel-"+mode);
    if(mode=="file-chooser"){
        // D-Bus activated by xdg-desktop-portal: only the picker's services.
        Backend pickerBackend;SettingsService pickerSettings;
        if(!previewFile.isEmpty())return previewFilePicker(&pickerBackend,&pickerSettings,previewFile,capture);
        return runFileChooserPortal(&pickerBackend,&pickerSettings);
    }
    Backend backend;Auth auth;Windows windows(nullptr,desktop);Store store;Connectivity radio;Updates updates;Audio audio;
    if(releaseInput){backend.releaseShellInput();return 0;}
    const auto jaredSocket=QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)+"/sovel-"+mode+".sock";
    std::unique_ptr<QLockFile> jaredOwner;
    if((mode=="jared"||mode=="settings")&&capture.isEmpty()){
        auto forward=[&]{
            QLocalSocket existing;existing.connectToServer(jaredSocket);
            if(!existing.waitForConnected(180))return false;
            existing.write(QJsonDocument(QJsonObject{{"prompt",jaredPrompt.left(2000)},{"page",settingsPage},{"action",jaredService?"connect":""}}).toJson(QJsonDocument::Compact)+"\n");
            return existing.waitForBytesWritten(500)||existing.bytesToWrite()==0;
        };
        if(forward())return 0;
        if(mode=="jared"){
            jaredOwner=std::make_unique<QLockFile>(jaredSocket+".owner");
            jaredOwner->setStaleLockTime(0);
            if(!jaredOwner->tryLock()){
                for(int attempt=0;attempt<16;++attempt){
                    if(forward())return 0;
                    QThread::msleep(75);
                }
                return 2;
            }
        }
    }
    std::unique_ptr<SettingsService> settingsService;if(mode!="greeter")settingsService=std::make_unique<SettingsService>();
    std::unique_ptr<Displays> displays;if(desktop||mode=="settings")displays=std::make_unique<Displays>();
    // Only the desktop process announces finished downloads, so Settings never doubles them.
    std::unique_ptr<Downloads> downloadsHub;if(desktop||mode=="settings")downloadsHub=std::make_unique<Downloads>(settingsService.get(),desktop);
    std::unique_ptr<Workspace> workspace;if(settingsService)workspace=std::make_unique<Workspace>(&backend,settingsService.get(),desktop);
    std::unique_ptr<Jared> jared;
    if(mode=="jared"){
        jared=std::make_unique<Jared>(&backend,&audio,&radio);
        jared->setAppearanceHandler([&](const QString &appearance){return settingsService->setPreference("appearance.mode",appearance);});
        QObject::connect(&app,&QGuiApplication::screenAdded,jared.get(),&Jared::changed);
        QObject::connect(&app,&QGuiApplication::screenRemoved,jared.get(),&Jared::changed);
    }
    if(mode=="jared")app.setQuitOnLastWindowClosed(false);
    if(!desktop&&capture.isEmpty()&&folderPath.isEmpty()&&(mode=="files"||mode=="settings"||mode=="library"||mode=="store"||mode=="vault")&&windows.activateApp({"sovel-"+mode},false))return 0;
    std::unique_ptr<FileBrowser> browser;if(mode=="files"){browser=std::make_unique<FileBrowser>();if(downloads)browser->navigate(browser->downloadsPath());else if(!folderPath.isEmpty())browser->navigate(folderPath);}
    PackageInstaller packages;if(!packageFile.isEmpty())packages.load(packageFile);
    if(mode=="motion"||mode=="wallpapers"){
        if(!windows.activateApp({"waypaper","Waypaper","waypaper.py"},false))backend.openWallpaperPicker();
        return 0;
    }
    if(browser)QObject::connect(browser.get(),&FileBrowser::packageRequested,&app,[&](const QString &path){QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--install-package",path});});
    if(desktop){updates.startAutomaticChecks();backend.restoreExternalWallpaper();audio.restorePreferences();}
    if(!capture.isEmpty() && (desktop || auth.available()))qFatal("Capture is restricted to non-authenticating windowed tests.");
    std::vector<std::unique_ptr<QQuickView>> views;
    QHash<QScreen*,QList<QQuickView*>> outputViews;
    std::function<void()> showJared;
    QLocalServer jaredInstance;
    if((mode=="jared"||mode=="settings"||mode=="desktop")&&capture.isEmpty()){
        QLocalServer::removeServer(jaredSocket);jaredInstance.setSocketOptions(QLocalServer::UserAccessOption);
        if(!jaredInstance.listen(jaredSocket))return 2;
        QObject::connect(&jaredInstance,&QLocalServer::newConnection,&app,[&]{
            while(auto socket=jaredInstance.nextPendingConnection()){
                QObject::connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
                QObject::connect(socket,&QLocalSocket::readyRead,&app,[&,socket]{
                    auto bytes=socket->property("message").toByteArray()+socket->readAll();if(bytes.size()>8192){socket->abort();return;}socket->setProperty("message",bytes);if(!bytes.contains('\n'))return;
                    const auto request=QJsonDocument::fromJson(bytes.left(bytes.indexOf('\n'))).object();
                    if(mode=="desktop"){
                        if(request.value("releaseInput").toBool())for(auto &view:views)if(auto chrome=view->findChild<Chrome*>())chrome->releasePopups();
                        socket->disconnectFromServer();return;
                    }
                    const auto action=request.value("action").toString();
                    if(mode=="jared"&&!action.isEmpty()){
                        const auto message=request.value("text").toString().left(2000);
                        if(action=="ask")jared->ask(message);
                        else if(action=="voice-start")jared->recordVoice();
                        else if(action=="voice-stop")jared->stopVoice();
                        else if(action=="clear-transcript")jared->clearTranscript();
                        else if(action=="approve")jared->approveAction();
                        else if(action=="reject")jared->rejectAction();
                        else if(action=="choose-output"){
                            bool valid=false;const int index=message.toInt(&valid);if(valid)jared->chooseOutput(index);
                        }
                        else if(action=="cancel")jared->cancel();
                        else if(action=="unload"&&!jared->busy())jared->unload();
                        else if(action=="load-workspaces")jared->work("list",{});
                        else if(action=="project-open-file")jared->work("open",{{"path",message}});
                        else if(action=="project-summary"){
                            bool valid=false;const int id=message.toInt(&valid);
                            if(valid&&id>0)jared->work("project-summary",{{"project",id}});
                        }
                        else if(action=="project-task"){
                            bool valid=false;const int id=message.section(':',0,0).toInt(&valid);
                            const auto state=message.section(':',1,1);
                            if(valid&&id>0&&(state=="0"||state=="1"))jared->work("task-set",{{"id",id},{"done",state=="1"}});
                        }
                        else if(action=="show"||action=="preferences"){
                            showJared();
                            if(action=="preferences")if(auto panel=views.front()->rootObject()->findChild<QObject*>("jared-root"))
                                QMetaObject::invokeMethod(panel,"showPreferences");
                        }
                        socket->disconnectFromServer();
                        return;
                    }
                    if(mode=="jared"){
                        showJared();
                        if(!request.value("prompt").toString().isEmpty()){
                            if(auto panel=views.front()->rootObject()->findChild<QObject*>("jared-root"))
                                QMetaObject::invokeMethod(panel,"navigate",Q_ARG(QVariant,QString("chat")));
                            jared->ask(request.value("prompt").toString());
                        }
                    }else if(!views.empty()){
                        auto window=views.front().get();if(!windows.activateApp({"sovel-"+mode},false)){window->show();window->raise();window->requestActivate();}
                    }
                    if(mode=="settings"&&!views.empty())if(auto settings=views.front()->rootObject()->findChild<QObject*>("settings-root"))QMetaObject::invokeMethod(settings,"navigateNamed",Q_ARG(QVariant,request.value("page").toString()));
                    socket->disconnectFromServer();
                });
            }
        });
    }
    auto create=[&](const QString &file,QScreen *screen,const QString &surface){
        auto view=std::make_unique<QQuickView>();
        view->setScreen(screen);view->setColor(Qt::transparent);
        if(mode=="file-preview"&&screen)view->setPosition(screen->geometry().topLeft());
        view->setTitle(mode=="settings"?"Settings":mode=="files"?"Files":mode=="library"?"App Library":mode=="store"?"Store":mode=="jared"?"JARED":mode=="vault"?"Vault":"Sovel");view->setResizeMode(QQuickView::SizeRootObjectToView);
        const bool nativeApp=mode=="files" || mode=="settings" || mode=="library" || mode=="store" || mode=="package" || mode=="jared" || mode=="file-preview" || mode=="vault";
        if(mode=="package")view->setTitle("Package Installer");
        if(mode=="file-preview")view->setTitle(QFileInfo(previewFile).fileName()+" — Preview");
        if(mode=="greeter" || nativeApp)view->setFlags(Qt::Window | Qt::FramelessWindowHint);
        view->rootContext()->setContextProperty("System",&backend);
        view->rootContext()->setContextProperty("SettingsService",settingsService.get());
        view->rootContext()->setContextProperty("Workspace",workspace.get());
        view->rootContext()->setContextProperty("Browser",browser.get());
        if(mode=="file-preview")view->rootContext()->setContextProperty("previewDetails",FileBrowser::inspectFile(previewFile));
        view->rootContext()->setContextProperty("Displays",displays.get());
        view->rootContext()->setContextProperty("DownloadsHub",downloadsHub.get());
        view->rootContext()->setContextProperty("outputName",screen?screen->name():QString());
        view->rootContext()->setContextProperty("Packages",&packages);
        view->rootContext()->setContextProperty("Radio",&radio);
        view->rootContext()->setContextProperty("Audio",&audio);
        view->rootContext()->setContextProperty("Jared",jared.get());
        view->rootContext()->setContextProperty("Updates",&updates);
        view->rootContext()->setContextProperty("initialSettingsPage",settingsPage);
        if(mode=="jared")view->rootContext()->setContextProperty("initialJaredPage",jaredPrompt.isEmpty()?"overview":"chat");
        view->rootContext()->setContextProperty("Auth",&auth);
        view->rootContext()->setContextProperty("greeterPrivate",mode=="greeter"&&privateGreeter);
        view->rootContext()->setContextProperty("privateAccount",mode=="greeter"?SovelMode::privateUser():QString());
        view->rootContext()->setContextProperty("Windows",&windows);
        view->rootContext()->setContextProperty("StoreService",&store);
        view->engine()->addImageProvider("apps",new AppIcons);
        auto *chrome=new Chrome(view.get(),surface,desktop);
        if(desktop&&surface=="dock")for(auto peer:outputViews.value(screen))if(auto bar=peer->findChild<Chrome*>())chrome->shareTopEdgeWith(bar);
        view->rootContext()->setContextProperty("Chrome",chrome);
        view->rootContext()->setContextProperty("surfaceKind",surface);
        view->rootContext()->setContextProperty("desktopMode",desktop);
        view->rootContext()->setContextProperty("sovelWindow",view.get());
        auto size=screen->size();
        if(!desktop)view->resize(qMin(mode=="settings"?880:1000,size.width()-120),qMin(720,size.height()-140));
        else if(surface=="bar"||surface=="dock")view->resize(size);
        else view->resize(size);
#ifdef SOVEL_LAYER_SHELL
        if(desktop){
            auto *layer=LayerShellQt::Window::get(view.get());
            using L=LayerShellQt::Window;
            // QWindow's platform screen can change while Qt places an
            // unmapped window. Bind each layer surface to its intended output.
            layer->setScreen(screen);
            layer->setScope("sovel-"+surface);
            layer->setKeyboardInteractivity(L::KeyboardInteractivityOnDemand);
            if(surface=="background"){
                layer->setLayer(L::LayerBottom);layer->setAnchors(L::Anchors(L::AnchorTop)|L::AnchorBottom|L::AnchorLeft|L::AnchorRight);layer->setExclusiveZone(-1);
            }else if(surface=="bar"){
                layer->setLayer(L::LayerTop);layer->setAnchors(L::Anchors(L::AnchorTop)|L::AnchorLeft|L::AnchorRight);layer->setExclusiveZone(30);
            }else {
                layer->setLayer(L::LayerTop);layer->setAnchors(L::AnchorBottom);layer->setExclusiveZone(0);
            }
        }
#endif
        if(nativeApp){
            view->setMinimumSize(QSize(mode=="jared"||mode=="vault"?520:640,mode=="vault"?420:480));
            if(mode=="jared")QObject::connect(view.get(),&QWindow::visibilityChanged,&app,[&](QWindow::Visibility visibility){if(visibility==QWindow::Hidden&&!jaredService&&!jared->keepBackground())QTimer::singleShot(0,&app,&QCoreApplication::quit);});
            if(browser){
                auto *window=view.get();
                auto title=[window,&browser,&backend]{const auto folder=browser->inTrash()?QString("Trash"):browser->path()==backend.home()?QString("Home"):QFileInfo(browser->path()).fileName();window->setTitle((folder.isEmpty()?QString("Computer"):folder)+" — Files");window->rootContext()->setContextProperty("appTitle",window->title());};
                QObject::connect(browser.get(),&FileBrowser::pathChanged,window,title);title();
            }
            view->rootContext()->setContextProperty("appTitle",view->title());
            view->rootContext()->setContextProperty("appPage",QUrl("qrc:/qml/"+file));
        }
        view->setSource(QUrl("qrc:/qml/"+(nativeApp?QString("AppFrame.qml"):file)));
        if(view->status()==QQuickView::Error)exit(2);
        // Cage sizes its sole client to the output. Requesting fullscreen before
        // the first xdg configure triggers an assertion in Cage 0.2.1/wlroots.
        if((mode=="greeter"&&qEnvironmentVariableIsEmpty("SOVEL_GREETER_KIOSK"))||(mode=="file-preview"&&!displayOutput.isEmpty()))view->showFullScreen();else view->show();
        if(desktop){
            auto *window=view.get();outputViews[screen].append(window);
            QObject::connect(screen,&QScreen::geometryChanged,window,[window,screen,surface]{
                const auto size=screen->size();
                window->resize(size);
                window->requestUpdate();
            });
        }
        views.push_back(std::move(view));
    };
    if(jared){
        showJared=[&]{
            if(views.empty())create("JaredPanel.qml",app.primaryScreen(),"preview");
            else if(!windows.activateApp({"sovel-jared"},false)){
                auto *window=views.front().get();
                if(window->visibility()==QWindow::Minimized)window->showNormal();
                else window->show();
                window->raise();
                window->requestActivate();
            }
        };
        if(!jaredService)showJared();
        if(capture.isEmpty()){
            const auto statePath=Workspace::assistantStatePath();
            QDir().mkpath(QFileInfo(statePath).absolutePath());
            const auto publish=[&app,assistant=jared.get(),statePath]{
                auto state=QJsonObject::fromVariantMap(assistant->sessionState());
                state["pid"]=qint64(app.applicationPid());
                QSaveFile file(statePath);
                if(file.open(QIODevice::WriteOnly)){
                    file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
                    const auto bytes=QJsonDocument(state).toJson(QJsonDocument::Compact);
                    if(file.write(bytes)==bytes.size())file.commit();
                }
            };
            auto *publishTimer=new QTimer(&app);
            publishTimer->setSingleShot(true);
            publishTimer->setInterval(75);
            QObject::connect(publishTimer,&QTimer::timeout,&app,publish);
            QObject::connect(jared.get(),&Jared::changed,publishTimer,[publishTimer]{
                if(!publishTimer->isActive())publishTimer->start();
            });
            QObject::connect(&app,&QCoreApplication::aboutToQuit,&app,[statePath]{QFile::remove(statePath);});
            publish();
        }
    }else if(desktop){
        app.setQuitOnLastWindowClosed(false);
        auto addOutput=[&](QScreen *screen){
            if(!screen||outputViews.contains(screen))return;
            create("Desktop.qml",screen,"background");create("Desktop.qml",screen,"bar");create("Desktop.qml",screen,"dock");
        };
        for(auto *screen:app.screens())addOutput(screen);
        auto *restoreDisplays=new QTimer(&app);
        restoreDisplays->setSingleShot(true);
        restoreDisplays->setInterval(600);
        QObject::connect(restoreDisplays,&QTimer::timeout,displays.get(),&Displays::restore);
        QObject::connect(&app,&QGuiApplication::screenAdded,restoreDisplays,[restoreDisplays]{restoreDisplays->start();});
        // A pen display plugged in later gets its mapping once its screen appears.
        auto *remapTablets=new QTimer(&app);
        remapTablets->setSingleShot(true);
        remapTablets->setInterval(1500);
        QObject::connect(remapTablets,&QTimer::timeout,settingsService.get(),&SettingsService::applyPreferences);
        QObject::connect(&app,&QGuiApplication::screenAdded,remapTablets,[remapTablets]{remapTablets->start();});
        restoreDisplays->start();
        QObject::connect(&app,&QGuiApplication::screenAdded,&app,addOutput);
        QObject::connect(&app,&QGuiApplication::screenRemoved,&app,[&](QScreen *screen){
            const auto removed=outputViews.take(screen);
            views.erase(std::remove_if(views.begin(),views.end(),[&](const auto &view){return removed.contains(view.get());}),views.end());
        });
    }else{
        QString file=mode=="greeter"?"Greeter.qml":mode=="vault"?"Vault.qml":mode=="files"?"Files.qml":mode=="settings"?"Settings.qml":mode=="library"?"AppLibrary.qml":mode=="store"?"Store.qml":mode=="package"?"PackageInstaller.qml":mode=="jared"?"JaredPanel.qml":mode=="file-preview"?"StandalonePreview.qml":"Desktop.qml";
        auto *screen=app.primaryScreen();
        if(mode=="file-preview")for(auto *candidate:app.screens())if(candidate->name()==displayOutput){screen=candidate;break;}
        create(file,screen,"preview");
    }
    if(!capture.isEmpty())QTimer::singleShot(1200,&app,[&]{
        if(views.empty() || !views.front()->grabWindow().save(capture))app.exit(3);
        else app.quit();
    });
    if(jared&&!jaredPrompt.isEmpty())QTimer::singleShot(0,jared.get(),[&]{jared->ask(jaredPrompt);});
    const int result=app.exec();
    // Output callbacks capture local view collections. Disconnect before those
    // collections are destroyed and QGuiApplication tears down its screens.
    QObject::disconnect(&app,nullptr,&app,nullptr);
    return result;
}
