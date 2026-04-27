#include "models/AppController.h"
#include "models/AssetLibrary.h"
#include "models/EditorState.h"
#include "DriftImageProvider.h"
#include "preview/PreviewItem.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml/qqml.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("CutWire Drift");
    QGuiApplication::setOrganizationName("CutWire Drift");

    qmlRegisterType<PreviewItem>("Drift", 1, 0, "PreviewItem");

    static AssetLibrary assetLibrary;
    static EditorState editorState(&assetLibrary);
    qmlRegisterSingletonInstance("Drift", 1, 0, "AssetLibrary", &assetLibrary);
    qmlRegisterSingletonInstance("Drift", 1, 0, "EditorState", &editorState);
    qmlRegisterSingletonInstance("Drift", 1, 0, "AppController", &editorState);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("drift"), new DriftImageProvider());
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QGuiApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Drift", "Main");

    return app.exec();
}
