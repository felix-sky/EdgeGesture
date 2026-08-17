#pragma once

#include "StageManagerTypes.h"
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <windows.h>

class StageContainerController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("StageContainerController is managed by StageManagerService")

  Q_PROPERTY(quint64 containerId READ containerId CONSTANT)
  Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeIndexChanged)
  Q_PROPERTY(int pageCount READ pageCount NOTIFY pageCountChanged)
  Q_PROPERTY(bool isPinned READ isPinned WRITE setPinned NOTIFY isPinnedChanged)
  Q_PROPERTY(QVariantList pagesModel READ pagesModel NOTIFY pagesModelChanged)
  Q_PROPERTY(QString title READ title NOTIFY titleChanged)

public:
  explicit StageContainerController(ContainerId id, QObject *parent = nullptr);
  ~StageContainerController() override;

  quint64 containerId() const { return m_containerId; }
  int activeIndex() const { return m_activeIndex; }
  int pageCount() const { return m_pages.count(); }
  bool isPinned() const { return m_isPinned; }
  QVariantList pagesModel() const { return m_pageList; }
  QString title() const;

  HWND contentHostHwnd() const { return m_contentHostHwnd; }
  HWND qtHwnd() const { return m_qtHwnd; }
  const QList<PageRecord> &pages() const { return m_pages; }

  bool addPage(PageRecord page);
  bool removePage(PageId pageId, bool restore);
  void onTargetDestroyed(HWND target);
  void restoreAllPages();

  Q_INVOKABLE void bindHostWindow(QQuickWindow *window,
                                  QQuickItem *anchorItem);
  Q_INVOKABLE void syncGeometry();
  Q_INVOKABLE void activatePage(int index);
  Q_INVOKABLE void releasePage(int index);
  Q_INVOKABLE void closePage(int index);
  Q_INVOKABLE void setPinned(bool pinned);
  Q_INVOKABLE void minimizeContainer();
  Q_INVOKABLE void requestContainerClose();

signals:
  void activeIndexChanged(int index);
  void pageCountChanged(int count);
  void isPinnedChanged(bool pinned);
  void pagesModelChanged();
  void titleChanged();
  void containerCloseRequested();
  void pageAdded(quint64 pageId);
  void pageRemoved(quint64 pageId);

private:
  void createContentHostHwnd();
  void updatePageListModel();
  RECT getAnchorPhysicalRect() const;

  ContainerId m_containerId{0};
  QPointer<QQuickWindow> m_qwindow;
  QPointer<QQuickItem> m_anchorItem;
  HWND m_qtHwnd{nullptr};
  HWND m_contentHostHwnd{nullptr};

  QList<PageRecord> m_pages;
  int m_activeIndex{-1};
  bool m_isPinned{false};
  QVariantList m_pageList;
};
