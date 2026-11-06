#include "auth.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtEndian>

Auth::Auth(QObject *parent):QObject(parent){
    connect(&m_socket,&QLocalSocket::connected,this,[this]{send({{"type","create_session"},{"username",m_user}});});
    connect(&m_socket,&QLocalSocket::readyRead,this,&Auth::receive);
    connect(&m_socket,&QLocalSocket::disconnected,this,[this]{failure("Authentication service disconnected.");});
    connect(&m_socket,&QLocalSocket::errorOccurred,this,[this](QLocalSocket::LocalSocketError){failure("Authentication service is unavailable.");});
}
Auth::~Auth(){
    // Members are destroyed in reverse declaration order. Stop socket callbacks
    // before the state strings they access are destroyed.
    QObject::disconnect(&m_socket,nullptr,this,nullptr);
    m_socket.abort();
}
void Auth::failure(const QString &v){m_state="error";m_prompt=v;m_secret=false;emit changed();}
void Auth::begin(const QString &username){
    if(m_state!="idle" && m_state!="error")return;
    if(!available()){failure("Visual preview only — authentication is not available here.");return;}
    m_user=username.trimmed();if(m_user.isEmpty()){failure("Enter your username.");return;}
    m_buffer.clear();m_state="waiting";m_prompt="Connecting to authentication service…";emit changed();
    if(m_socket.state()==QLocalSocket::ConnectedState)send({{"type","create_session"},{"username",m_user}});
    else m_socket.connectToServer(qEnvironmentVariable("GREETD_SOCK"));
}
void Auth::send(const QJsonObject &obj){
    auto data=QJsonDocument(obj).toJson(QJsonDocument::Compact);
    quint32 length=qToLittleEndian(quint32(data.size()));
    m_socket.write(reinterpret_cast<const char *>(&length),4);m_socket.write(data);
    // No logging of authentication responses.
    data.fill('\0');
}
void Auth::answer(QString response){
    if(m_state!="prompt")return;
    m_state="waiting";emit changed();
    send({{"type","post_auth_message_response"},{"response",response}});
    response.fill(QChar('\0'));
}
void Auth::cancel(){
    if(m_socket.state()==QLocalSocket::ConnectedState){m_state="cancelling";send({{"type","cancel_session"}});}
    else {m_state="idle";m_prompt="Enter your username";}
    emit changed();
}
void Auth::receive(){
    m_buffer+=m_socket.readAll();
    while(m_buffer.size()>=4){
        quint32 n=qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(m_buffer.constData()));
        if(n>1024*1024){m_socket.abort();failure("Invalid authentication response.");return;}
        if(quint64(m_buffer.size())<4ULL+n)return;
        QJsonParseError err;auto doc=QJsonDocument::fromJson(m_buffer.mid(4,n),&err);m_buffer.remove(0,4+n);
        if(err.error!=QJsonParseError::NoError || !doc.isObject()){failure("Invalid authentication response.");return;}
        handle(doc.object());
    }
}
void Auth::handle(const QJsonObject &o){
    auto type=o.value("type").toString();
    if(type=="error"){
        if(m_state=="resetting" || m_state=="cancelling"){
            m_socket.abort();failure("Authentication service could not reset the session.");return;
        }
        // greetd requires cancelling a failed PAM conversation before retry.
        m_state="resetting";m_prompt="Authentication failed. Try again.";m_secret=false;
        send({{"type","cancel_session"}});emit changed();
    }else if(type=="auth_message"){
        auto kind=o.value("auth_message_type").toString();m_prompt=o.value("auth_message").toString();
        if(kind=="visible" || kind=="secret"){
            m_secret=kind=="secret";m_state="prompt";emit changed();
        }else if(kind=="info" || kind=="error"){
            emit changed();send({{"type","post_auth_message_response"},{"response",QJsonValue::Null}});
        }else failure("Unsupported authentication prompt.");
    }else if(type=="success"){
        if(m_state=="resetting"){m_state="error";emit changed();return;}
        if(m_state=="cancelling"){m_state="idle";m_prompt="Enter your username";emit changed();return;}
        if(m_state=="starting"){QCoreApplication::quit();return;}
        if(m_state!="waiting"){failure("Unexpected authentication state.");return;}
        m_state="starting";m_prompt="Opening your desktop…";emit changed();
        send({{"type","start_session"},{"cmd",QJsonArray{"/usr/bin/sovel-session"}},
              {"env",QJsonArray{"XDG_SESSION_TYPE=wayland","XDG_CURRENT_DESKTOP=Sovel","XDG_SESSION_DESKTOP=sovel"}}});
    }else failure("Unexpected authentication response.");
}
