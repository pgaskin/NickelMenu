#ifdef __cplusplus
#include <QObject>
#include <QtPlugin>

class NMIPlugin : public QObject {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "net.pgaskin.nmi");
};
#endif
