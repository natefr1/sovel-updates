#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QJsonObject>

// Greeter only. Never used as a substitute for an ext-session-lock client.
class Auth : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString prompt READ prompt NOTIFY changed)
    Q_PROPERTY(bool secret READ secret NOTIFY changed)
    Q_PROPERTY(bool available READ available CONSTANT)
public:
    explicit Auth(QObject *parent=nullptr);
    ~Auth() override;
    QString state()const{return m_state;}
    QString prompt()const{return m_prompt;}
    bool secret()const{return m_secret;}
    bool available()const{return !qEnvironmentVariableIsEmpty("GREETD_SOCK");}
    Q_INVOKABLE void begin(const QString &username);
    Q_INVOKABLE void answer(QString response);
    Q_INVOKABLE void cancel();
signals:void changed();
private:
    void send(const QJsonObject &object);
    void receive();
    void handle(const QJsonObject &object);
    void failure(const QString &text);
    QLocalSocket m_socket;
    QByteArray m_buffer;
    QString m_state="idle",m_prompt="Enter your username",m_user;
    bool m_secret=false;
};
