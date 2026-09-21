#pragma once
#include <QImageIOPlugin>
class NickelMenuQt6 final : public QImageIOPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE "plugin.json")
public:
    Capabilities capabilities(QIODevice *, const QByteArray &) const override { return {}; }
    QImageIOHandler *create(QIODevice *, const QByteArray & = {}) const override { return nullptr; }
};
