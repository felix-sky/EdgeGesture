#include "WinIconProvider.h"
#include <QQmlEngine>
#include <QQmlEngineExtensionPlugin>

#include "WindowThumbnailProvider.h"

// Define the custom plugin to register the ImageProvider
class StageManagerPlugin : public QQmlEngineExtensionPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)

public:
  void initializeEngine(QQmlEngine *engine, const char *uri) override {
    Q_UNUSED(uri);
    // Register the image provider
    // URI is irrelevant for image provider global id
    engine->addImageProvider("windowIcons", new WinIconProvider);
    engine->addImageProvider("windowThumbnails", new WindowThumbnailProvider);
  }
};

#include "StageManagerPlugin.moc"
