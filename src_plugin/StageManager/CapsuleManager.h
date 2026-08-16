#pragma once

#include "WindowController.h"
#include <QAbstractListModel>
#include <QObject>
#include <QVector>

class CapsuleManager : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  enum Roles { HwndRole = Qt::UserRole + 1, TitleRole, ControllerRole };
  Q_ENUM(Roles)

  static CapsuleManager *instance();
  explicit CapsuleManager(QObject *parent = nullptr);
  ~CapsuleManager();

  // QAbstractListModel interface
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  // API
  Q_INVOKABLE void addWindow(qint64 hwnd);
  Q_INVOKABLE void removeWindow(qint64 hwnd);
  Q_INVOKABLE void closeAll();

private:
  static CapsuleManager *s_instance;
  QVector<WindowController *> m_controllers;
};
