#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

class SystemEventListener : public QObject, public QAbstractNativeEventFilter {
  Q_OBJECT
public:
  explicit SystemEventListener(QObject *parent = nullptr);
  ~SystemEventListener() override;

  bool nativeEventFilter(const QByteArray &eventType, void *message,
                         qintptr *result) override;

signals:
  void showPluginRequest(const QString &pluginName);
};
