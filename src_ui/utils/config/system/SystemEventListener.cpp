#include "SystemEventListener.h"
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

SystemEventListener::SystemEventListener(QObject *parent) : QObject(parent) {}

SystemEventListener::~SystemEventListener() {}

bool SystemEventListener::nativeEventFilter(const QByteArray &eventType,
                                            void *message, qintptr *result) {
  Q_UNUSED(eventType);
  Q_UNUSED(result);

#ifdef Q_OS_WIN
  MSG *msg = static_cast<MSG *>(message);
  if (msg->message == WM_COPYDATA) {
    COPYDATASTRUCT *cds = (COPYDATASTRUCT *)msg->lParam;
    if (cds->dwData == 1) { // Plugin Command ID
      QString pluginName = QString::fromUtf8((char *)cds->lpData);
      qDebug() << "Plugin command received: " << pluginName;
      emit showPluginRequest(pluginName);
      return true;
    }
  }
#endif
  return false;
}
