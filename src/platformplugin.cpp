#include "connectivity.h"
#include <QQmlExtensionPlugin>
#include <qqml.h>

// Calamares runs a separate QML engine. Reuse the same radio implementation
// there rather than maintaining a second installer-only network controller.
class SovelPlatformPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *uri) override {
        qmlRegisterType<Connectivity>(uri, 1, 0, "Connectivity");
    }
};
#include "platformplugin.moc"
