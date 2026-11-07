#pragma once
// Normal Mode and Private Mode are separate Linux accounts. A process is in
// Private Mode only when it really belongs to the sovel-private group; no
// environment variable or preference can switch a Normal account into it.
#include <QFile>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <grp.h>
#include <unistd.h>
#include <vector>

namespace SovelMode {
inline constexpr auto group = "sovel-private";
inline constexpr auto configPath = "/etc/sovel/private.conf";

// Tests may pin the mode. Nothing reachable from QML or the command line calls this.
inline int &testOverride(){static int value=-1;return value;}
inline void setForTesting(bool isPrivate){testOverride()=isPrivate?1:0;}

inline bool isPrivate(){
    if(testOverride()>=0)return testOverride()==1;
    static const bool member=[]{
        const auto *entry=getgrnam(group);
        if(!entry)return false;
        if(getgid()==entry->gr_gid)return true;
        const int count=getgroups(0,nullptr);
        if(count<=0)return false;
        std::vector<gid_t> groups(static_cast<size_t>(count));
        const int filled=getgroups(count,groups.data());
        for(int i=0;i<filled;++i)if(groups[static_cast<size_t>(i)]==entry->gr_gid)return true;
        return false;
    }();
    return member;
}
inline QString name(){return isPrivate()?QStringLiteral("private"):QStringLiteral("normal");}
inline bool configured(){return QFile::exists(QString::fromLatin1(configPath));}
// JARED lives in Private Mode. Until Private Mode is set up on this computer it
// stays in Normal Mode, so upgrading never leaves the person without JARED.
// Decided once per process: setup takes effect at the next sign-in.
inline int &assistantOverride(){static int value=-1;return value;}
inline void setAssistantForTesting(bool available){assistantOverride()=available?1:0;}
inline bool assistantAvailable(){
    if(assistantOverride()>=0)return assistantOverride()==1;
    if(isPrivate())return true;
    static const bool beforeSetup=!configured();
    return beforeSetup;
}
// Entries tagged "modes" exist only in those modes; "assistant" means wherever JARED is.
inline bool allowedIn(const QStringList &modes){
    return modes.isEmpty()||modes.contains(name())||(modes.contains(QStringLiteral("assistant"))&&assistantAvailable());
}
inline int terminal(const char *key,int fallback){
    QSettings settings(QString::fromLatin1(configPath),QSettings::IniFormat);
    bool ok=false;const int value=settings.value(QStringLiteral("private/")+QLatin1String(key),fallback).toInt(&ok);
    return ok&&value>=1&&value<=63?value:fallback;
}
inline int normalTerminal(){return terminal("normal_vt",7);}
inline int privateTerminal(){return terminal("private_vt",8);}
inline QString privateUser(){
    QSettings settings(QString::fromLatin1(configPath),QSettings::IniFormat);
    const auto user=settings.value(QStringLiteral("private/user")).toString();
    for(const auto c:user)if(!(c.isLower()||c.isDigit()||c=='-'||c=='_'))return {};
    return user.size()<=31?user:QString();
}
}
