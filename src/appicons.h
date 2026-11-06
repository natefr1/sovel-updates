#pragma once
#include <QQuickImageProvider>
#include <QIcon>
#include <QUrl>
#include <QPainter>
#include <QDirIterator>
#include <QSettings>
#include <QStandardPaths>
#include <QFileInfo>
#include <QCache>
class AppIcons:public QQuickImageProvider {
    static QIcon resolve(const QString &token){
        QString name=token;QStringList alternatives;
        if(name.startsWith("desktop:")){
            const auto file=name.mid(8);QSettings entry(file,QSettings::IniFormat);entry.beginGroup("Desktop Entry");
            alternatives<<entry.value("X-Flatpak").toString()<<entry.value("StartupWMClass").toString()<<QFileInfo(file).completeBaseName();
            name=entry.value("Icon").toString();
            if(name.startsWith("file:"))name=QUrl(name).toLocalFile();
            if(QFileInfo(name).isRelative()&&name.contains('/'))name=QFileInfo(file).dir().absoluteFilePath(name);
        }
        alternatives.prepend(name);alternatives.removeAll("");alternatives.removeDuplicates();
        QStringList roots=QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
        roots<<QDir::homePath()+"/.local/share/flatpak/exports/share"<<"/var/lib/flatpak/exports/share"<<"/var/lib/snapd/desktop";
        for(const auto &candidate:alternatives){
            if(QFileInfo(candidate).isFile())return QIcon(candidate);
            auto icon=QIcon::fromTheme(candidate);if(!icon.isNull())return icon;
            const auto base=QFileInfo(candidate).completeBaseName();
            for(const auto &root:roots)for(const auto &sub:QStringList{"icons/hicolor","icons/Adwaita","pixmaps","icons"}){
                QDirIterator files(root+"/"+sub,{candidate,candidate+".png",candidate+".svg",candidate+".xpm",base+".png",base+".svg",base+".xpm"},QDir::Files,QDirIterator::Subdirectories);
                if(files.hasNext())return QIcon(files.next());
            }
            const auto shortName=candidate.contains("spotify",Qt::CaseInsensitive)?"spotify":candidate.contains("steam",Qt::CaseInsensitive)?"steam":"";
            if(*shortName){icon=QIcon::fromTheme(shortName);if(!icon.isNull())return icon;}
        }
        return {};
    }
    QCache<QString,QIcon> m_cache{256};
public:
    AppIcons():QQuickImageProvider(QQuickImageProvider::Pixmap){}
    QPixmap requestPixmap(const QString &id,QSize *size,const QSize &requested)override{
        const auto name=QUrl::fromPercentEncoding(id.toUtf8());
        if(!m_cache.contains(name))m_cache.insert(name,new QIcon(resolve(name)));
        auto icon=*m_cache.object(name);
        QSize target=requested.isValid()?requested:QSize(64,64);
        target=target.boundedTo(QSize(256,256));
        QPixmap pixmap=icon.pixmap(target);
        if(pixmap.isNull()&&(name=="application-x-deb"||name=="application-vnd.debian.binary-package"||name=="system-software-install"))pixmap=QIcon(":/assets/icons/package.svg").pixmap(target);
        if(pixmap.isNull()&&(name.startsWith("text-")||name.startsWith("application-")||name.startsWith("image-")||name.startsWith("audio-")||name.startsWith("video-")))pixmap=QIcon(":/assets/icons/document.svg").pixmap(target);
        if(pixmap.isNull()&&(name.startsWith("folder")||name=="user-home"))pixmap=QIcon(":/assets/icons/files.svg").pixmap(target);
        if(pixmap.isNull())pixmap=QIcon(":/assets/icons/generic-app.svg").pixmap(target);
        if(size)*size=pixmap.size();return pixmap;
    }
};
