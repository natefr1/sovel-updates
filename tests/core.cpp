#include <QtTest>
#include <memory>
#include <algorithm>
#include <QLocalServer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QDir>
#include <QtEndian>
#include "auth.h"
#include "backend.h"

class CoreTests:public QObject {
    Q_OBJECT
    QTemporaryDir temp;
    QLocalServer server;
    QLocalSocket *peer=nullptr;
    QByteArray incoming;
    bool connectAuth(Auth &auth){
        auto name=temp.path()+"/greetd";
        QLocalServer::removeServer(name);
        if(!server.listen(name))return false;
        qputenv("GREETD_SOCK",name.toUtf8());
        auth.begin("test-user");
        QElapsedTimer timeout;timeout.start();
        while(!server.hasPendingConnections() && timeout.elapsed()<1000){QCoreApplication::processEvents();QTest::qWait(2);}
        if(!server.hasPendingConnections())return false;
        peer=server.nextPendingConnection();return true;
    }
    QJsonObject take(){
        QElapsedTimer t;t.start();
        while(t.elapsed()<1000){
            QCoreApplication::processEvents();incoming+=peer->readAll();
            if(incoming.size()>=4){
                quint32 n=qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(incoming.constData()));
                if(incoming.size()>=int(n+4)){
                    auto doc=QJsonDocument::fromJson(incoming.mid(4,n));incoming.remove(0,n+4);return doc.object();
                }
            }
            QTest::qWait(2);
        }
        return {};
    }
    void reply(QJsonObject obj,bool split=false){
        auto data=QJsonDocument(obj).toJson(QJsonDocument::Compact);
        quint32 n=qToLittleEndian(quint32(data.size()));
        QByteArray frame(reinterpret_cast<char *>(&n),4);frame+=data;
        if(split){peer->write(frame.left(2));peer->flush();QTest::qWait(20);peer->write(frame.mid(2));}
        else peer->write(frame);
        peer->flush();
    }
private slots:
    void dockPinsKeepSavedOrderAndAllowSystemApps(){
        Backend backend;const auto original=backend.pinnedApps();QStringList ids;for(const auto &value:original)ids<<value.toMap().value("id").toString();
        backend.setPinned("sovel:settings",false);QVERIFY(!backend.isPinned("sovel:settings"));backend.setPinned("sovel:settings",true);QVERIFY(backend.isPinned("sovel:settings"));
        const auto before=backend.pinnedApps();QStringList order;for(const auto &value:before)order<<value.toMap().value("id").toString();
        std::reverse(order.begin(),order.end());backend.setDockOrder(order);const auto after=backend.pinnedApps();QCOMPARE(after.first().toMap().value("id").toString(),order.first());
        Backend reader;QCOMPARE(reader.pinnedApps().first().toMap().value("id").toString(),order.first());
        backend.setPinned("sovel:library",true);QVERIFY(!backend.isPinned("sovel:library"));backend.setPinned("not-an-installed-app",true);QVERIFY(!backend.isPinned("not-an-installed-app"));
        for(const auto &value:backend.pinnedApps())backend.setPinned(value.toMap().value("id").toString(),false);
        for(const auto &id:ids)backend.setPinned(id,true);backend.setDockOrder(ids);
    }
    void initTestCase(){qputenv("XDG_CONFIG_HOME",temp.path().toUtf8());}
    void cleanup(){server.close();if(peer){peer->abort();peer->deleteLater();peer=nullptr;}incoming.clear();qunsetenv("GREETD_SOCK");}
    void previewDoesNotAuthenticate(){
        qunsetenv("GREETD_SOCK");Auth auth;QVERIFY(!auth.available());auth.begin("nate");QCOMPARE(auth.state(),QString("error"));
    }
    void connectedAuthDestructionDoesNotEmit(){
        auto auth=std::make_unique<Auth>();
        QVERIFY2(connectAuth(*auth), "IPC socket required for teardown regression test");
        QCOMPARE(take()["type"].toString(),QString("create_session"));
        int changes=0;
        connect(auth.get(),&Auth::changed,this,[&changes]{++changes;});
        auth.reset();
        QCOMPARE(changes,0);
        QTRY_COMPARE(peer->state(),QLocalSocket::UnconnectedState);
    }
    void multiplePromptsAndFixedSessionCommand(){
        Auth auth;if(!connectAuth(auth))QSKIP("Unix-domain sockets unavailable: run IPC integration on the target VM.");
        auto first=take();QCOMPARE(first["type"].toString(),QString("create_session"));QCOMPARE(first["username"].toString(),QString("test-user"));
        reply({{"type","auth_message"},{"auth_message_type","secret"},{"auth_message","Password:"}},true);
        QTRY_COMPARE(auth.state(),QString("prompt"));QVERIFY(auth.secret());
        auth.answer("not-a-real-password");auto answer=take();QCOMPARE(answer["type"].toString(),QString("post_auth_message_response"));
        reply({{"type","auth_message"},{"auth_message_type","visible"},{"auth_message","Challenge:"}});
        QTRY_COMPARE(auth.state(),QString("prompt"));QVERIFY(!auth.secret());
        auth.answer("challenge-response");take();
        reply({{"type","success"}});QTRY_COMPARE(auth.state(),QString("starting"));
        auto start=take();QCOMPARE(start["type"].toString(),QString("start_session"));
        QCOMPARE(start["cmd"].toArray(),QJsonArray{"/usr/bin/sovel-session"});
        // Do not acknowledge start here: production exits only after that ack.
    }
    void failedAuthCancelsBeforeRetry(){
        Auth auth;if(!connectAuth(auth))QSKIP("Unix-domain sockets unavailable: run IPC integration on the target VM.");take();
        reply({{"type","error"},{"error_type","auth_error"},{"description","Denied"}});
        QTRY_COMPARE(auth.state(),QString("resetting"));QCOMPARE(take()["type"].toString(),QString("cancel_session"));
        reply({{"type","success"}});QTRY_COMPARE(auth.state(),QString("error"));
        auth.begin("test-user");QCOMPARE(take()["type"].toString(),QString("create_session"));
    }
    void oversizedFrameFailsClosed(){
        Auth auth;if(!connectAuth(auth))QSKIP("Unix-domain sockets unavailable: run IPC integration on the target VM.");take();
        quint32 n=qToLittleEndian(quint32(1048577));peer->write(reinterpret_cast<char *>(&n),4);peer->flush();
        QTRY_COMPARE(auth.state(),QString("error"));
    }
    void realFilesAndInvalidNames(){
        Backend backend;QTemporaryDir dir;
        QVERIFY(backend.makeFolder(dir.path(),"Projects"));
        QVERIFY(!backend.makeFolder(dir.path(),"../outside"));
        QVERIFY(!backend.makeFolder(dir.path(),".."));
        auto files=backend.files(dir.path());QCOMPARE(files.size(),1);
        QCOMPARE(files[0].toMap()["name"].toString(),QString("Projects"));
        QVERIFY(files[0].toMap()["folder"].toBool());
        QVERIFY(backend.metrics()["ramTotal"].toDouble()>0);
    }
    void appLibraryHasNoDuplicateNamesOrTerminalHelpers(){
        Backend backend;QSet<QString> names;
        int files=0,settings=0,terminals=0;
        for(const auto &entry:backend.applications()){
            const auto app=entry.toMap();const auto key=app["name"].toString().simplified().toCaseFolded();
            QVERIFY2(!names.contains(key),qPrintable("Duplicate app: "+key));names.insert(key);
            QVERIFY(!app["id"].toString().endsWith("footclient.desktop"));
            QVERIFY(!app["id"].toString().endsWith("foot-server.desktop"));
            if(key=="files")files++;if(key=="settings")settings++;if(key=="terminal")terminals++;
        }
        QCOMPARE(files,1);QCOMPARE(settings,1);QCOMPARE(terminals,1);
    }
    void appearancePersistsAndRejectsUnknownWallpaper(){
        Backend writer;
        const auto previous=writer.wallpaper();
        const bool motion=writer.reducedMotion(),effects=writer.lowEffects();
        writer.setWallpaper("lockroom");writer.setReducedMotion(true);writer.setLowEffects(true);
        Backend reader;
        QCOMPARE(reader.wallpaper(),QString("lockroom"));
        QVERIFY(reader.reducedMotion());QVERIFY(reader.lowEffects());
        writer.setWallpaper("../../invalid");
        QCOMPARE(writer.wallpaper(),QString("lockroom"));
        writer.setWallpaper("landscape");
        QTRY_COMPARE(reader.wallpaper(),QString("landscape"));
        writer.setWallpaper(previous);writer.setReducedMotion(motion);writer.setLowEffects(effects);
    }
    void layoutIsValidatedAndPersisted(){
        Backend backend;
        backend.saveWidgets({QVariantMap{{"type","clock"},{"x",12.0},{"y",-1.0}},QVariantMap{{"type","unknown"}}});
        Backend other;auto saved=other.widgets();QCOMPARE(saved.size(),1);
        QCOMPARE(saved[0].toMap()["x"].toDouble(),1.0);QCOMPARE(saved[0].toMap()["y"].toDouble(),0.0);
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core.moc"
