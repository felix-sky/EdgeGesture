#pragma once

#include <QPointer>
#include <QQuickItem>
#include <dwmapi.h>
#include <qqml.h>

class QmlDwmThumbnail : public QQuickItem {
  Q_OBJECT
  QML_NAMED_ELEMENT(DwmThumbnail)
  Q_PROPERTY(quint64 sourceHwnd READ sourceHwnd WRITE setSourceHwnd NOTIFY
                 sourceHwndChanged)
  Q_PROPERTY(bool interactive READ interactive WRITE setInteractive NOTIFY
                 interactiveChanged)

public:
  explicit QmlDwmThumbnail(QQuickItem *parent = nullptr);
  ~QmlDwmThumbnail() override;

  quint64 sourceHwnd() const;
  void setSourceHwnd(quint64 newSourceHwnd);

  bool interactive() const;
  void setInteractive(bool newInteractive);

  void componentComplete() override;

signals:
  void sourceHwndChanged();
  void interactiveChanged();

protected:
  void geometryChange(const QRectF &newGeometry,
                      const QRectF &oldGeometry) override;
  void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
  void registerThumbnail();
  void unregisterThumbnail();
  void updateThumbnailRect();

  quint64 m_sourceHwnd = 0;
  bool m_interactive = false;
  HTHUMBNAIL m_hThumbnail = nullptr;
  QPointer<QWindow> m_window;

  // Cache to avoid unnecessary DWM calls
  RECT m_lastDestRect = {0, 0, 0, 0};
};
