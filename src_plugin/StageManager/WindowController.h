#pragma once

#include "SafeWindowReparenter.h"
#include <QObject>
#include <QQuickItem>
#include <windows.h>

class WindowController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("WindowController is managed by CapsuleManager")
  Q_PROPERTY(QString title READ title NOTIFY titleChanged)
  Q_PROPERTY(qint64 hwnd READ hwndConstant CONSTANT)
  Q_PROPERTY(QQuickItem *capsuleItem READ capsuleItem WRITE setCapsuleItem
                 NOTIFY capsuleItemChanged)

public:
  explicit WindowController(HWND hwnd, QObject *parent = nullptr);
  ~WindowController();

  QString title() const;
  qint64 hwndConstant() const;
  QQuickItem *capsuleItem() const;
  void setCapsuleItem(QQuickItem *item);

  Q_INVOKABLE void requestMove(int x, int y);
  Q_INVOKABLE void requestResize(int w, int h);
  Q_INVOKABLE void requestFocus();
  Q_INVOKABLE void requestClose(); // Kill window with WM_CLOSE
  Q_INVOKABLE void
  requestRelease(); // Release and restore window without killing

  Q_INVOKABLE void syncGeometry();

public slots:
  void onCapsuleXChanged();
  void onCapsuleYChanged();
  void onCapsuleWidthChanged();
  void onCapsuleHeightChanged();

signals:
  void titleChanged();
  void capsuleItemChanged();
  void closed(); // Signal to Manager to remove this controller
  void activeChanged(bool active);

private slots:
  void onWindowActiveStateChanged(qint64 hwnd, bool active);
  void onWindowDestroyed(qint64 hwnd);

private:
  HWND m_hwnd;
  QQuickItem *m_capsuleItem = nullptr;
  SafeWindowReparenter *m_reparenter;

  // DPI Helpers
  float getDevicePixelRatio() const;
};
