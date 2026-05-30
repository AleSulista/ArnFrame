#include "models/AppController.h"
#include "models/AssetLibrary.h"
#include "models/EditorState.h"
#include "models/FileDialogs.h"
#include "DriftImageProvider.h"
#include "preview/PreviewItem.h"

// QApplication (not QGuiApplication) is required so QFileDialog can use the
// native platform file picker, which routes through xdg-desktop-portal.
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QtQml/qqml.h>

int main(int argc, char *argv[])
{
    // The compositor renders into an FBO on its own GL context and hands the
    // texture to the scene graph without a readback. That requires both contexts
    // to share objects, and the scene graph to actually be on OpenGL. Both must
    // be set before the QApplication is constructed.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    QApplication::setApplicationName("CutWire Drift");
    QApplication::setOrganizationName("CutWire Drift");

    qmlRegisterType<PreviewItem>("Drift", 1, 0, "PreviewItem");

    static AssetLibrary assetLibrary;
    static EditorState editorState(&assetLibrary);
    static FileDialogs fileDialogs;
    qmlRegisterSingletonInstance("Drift", 1, 0, "AssetLibrary", &assetLibrary);
    qmlRegisterSingletonInstance("Drift", 1, 0, "EditorState", &editorState);
    qmlRegisterSingletonInstance("Drift", 1, 0, "AppController", &editorState);
    qmlRegisterSingletonInstance("Drift", 1, 0, "FileDialogs", &fileDialogs);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("drift"), new DriftImageProvider());
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QGuiApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Drift", "Main");

    return app.exec();
}
