#pragma once

#include "StageContainerController.h"
#include "StageManagerTypes.h"
#include <QMap>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class StageManagerService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(quint64 activeContainerId READ activeContainerId NOTIFY
                 activeContainerChanged)
  Q_PROPERTY(int containerCount READ containerCount NOTIFY containerCountChanged)

public:
  static StageManagerService *instance();
  explicit StageManagerService(QObject *parent = nullptr);
  ~StageManagerService() override;

  quint64 activeContainerId() const { return m_activeContainerId; }
  int containerCount() const { return m_containers.count(); }

  Q_INVOKABLE quint64 createContainer();
  Q_INVOKABLE void createContainerWithWindow(qint64 hwnd);
  Q_INVOKABLE bool addWindowToActiveContainer(qint64 hwnd);
  Q_INVOKABLE bool addWindowToContainer(qint64 hwnd, quint64 containerId);

  Q_INVOKABLE void activatePage(quint64 containerId, int pageIndex);
  Q_INVOKABLE void releasePage(quint64 containerId, int pageIndex);
  Q_INVOKABLE void closePage(quint64 containerId, int pageIndex);
  Q_INVOKABLE void closeContainer(quint64 containerId);

  Q_INVOKABLE void setPinned(quint64 containerId, bool pinned);
  Q_INVOKABLE void setActiveDestination(quint64 containerId);

  Q_INVOKABLE bool isManaged(qint64 hwnd) const;
  Q_INVOKABLE QString dumpState() const;
  Q_INVOKABLE StageContainerController *
  getController(quint64 containerId) const;

  Q_INVOKABLE void applyWin11RoundedCorners(QQuickWindow *window);
  Q_INVOKABLE void closeAll();

  // Internal lifecycle calls
  void onTargetDestroyed(HWND hwnd);

signals:
  void activeContainerChanged(quint64 containerId);
  void containerCreated(quint64 containerId,
                        StageContainerController *controller);
  void containerClosed(quint64 containerId);
  void containerCountChanged(int count);

private:
  static StageManagerService *s_instance;

  quint64 m_nextContainerId{1};
  quint64 m_nextPageId{1};
  quint64 m_activeContainerId{0};

  QMap<ContainerId, StageContainerController *> m_containers;
  QMap<HWND, ManagedEntry> m_managedWindows;
};
