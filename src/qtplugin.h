#ifdef __cplusplus
#include <QImageIOHandler>
#include <QObject>
#include <QtPlugin>

// we make it a fake image plugin so we can have Qt automatically load it
// without needing extra configuration (e.g. LD_PRELOAD, -plugin arg, etc).

class NMPlugin : public QImageIOPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface")
public:
    Capabilities capabilities(QIODevice*, QByteArray const&) const { return 0; };
    QImageIOHandler *create(QIODevice*, QByteArray const& = QByteArray()) const { return 0; };
};

#endif
