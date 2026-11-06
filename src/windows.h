#pragma once
#include <QObject>
#include <QVariantList>
#include <memory>

class Windows : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString activeName READ activeName NOTIFY changed)
    Q_PROPERTY(QString activeAppId READ activeAppId NOTIFY changed)
    Q_PROPERTY(bool hasWindows READ hasWindows NOTIFY changed)
    Q_PROPERTY(bool activeMaximized READ activeMaximized NOTIFY changed)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
public:
    explicit Windows(QObject *parent=nullptr,bool manageNewApps=false);
    ~Windows() override;
    QString activeName() const;
    QString activeAppId() const;
    bool hasWindows() const;
    bool activeMaximized() const;
    QVariantList entries() const;
    Q_INVOKABLE void activate(int id);
    Q_INVOKABLE bool activateApp(const QStringList &appIds,bool toggleActive=false);
    Q_INVOKABLE void closeActive();
    Q_INVOKABLE void minimizeActive();
    Q_INVOKABLE void toggleMaximized();
signals:
    void changed();
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
