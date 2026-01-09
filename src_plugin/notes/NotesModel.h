#pragma once

#include <QAbstractListModel>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QString>
#include <QVector>
#include <QtQml>


struct NoteItem {
  QString type;    // "folder" or "note"
  QString title;   // Display name
  QString path;    // Full file path
  QString date;    // Modified date string
  QString color;   // Color from frontmatter
  QString preview; // Content preview
};

class NotesModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString currentPath READ currentPath WRITE setCurrentPath NOTIFY
                 currentPathChanged)
  Q_PROPERTY(
      QString rootPath READ rootPath WRITE setRootPath NOTIFY rootPathChanged)
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
  Q_PROPERTY(QStringList folderStack READ folderStack NOTIFY folderStackChanged)

public:
  enum NotesRoles {
    TypeRole = Qt::UserRole + 1,
    TitleRole,
    PathRole,
    DateRole,
    ColorRole,
    PreviewRole
  };

  explicit NotesModel(QObject *parent = nullptr);
  ~NotesModel() override;

  // QAbstractListModel interface
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  // Properties
  QString currentPath() const;
  void setCurrentPath(const QString &path);

  QString rootPath() const;
  void setRootPath(const QString &path);

  bool loading() const;
  QStringList folderStack() const;

  // Q_INVOKABLE methods for QML
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void navigateToFolder(const QString &folderPath);
  Q_INVOKABLE void navigateBack();
  Q_INVOKABLE void navigateToRoot();

signals:
  void currentPathChanged();
  void rootPathChanged();
  void loadingChanged();
  void folderStackChanged();
  void errorOccurred(const QString &message);

private slots:
  void onScanFinished();
  void onDirectoryChanged(const QString &path);

private:
  void startScan();
  static QVector<NoteItem> scanDirectory(const QString &path);
  static QString parseColor(const QString &content);
  static QString createPreview(const QString &content, int maxLength = 100);

  QString m_currentPath;
  QString m_rootPath;
  bool m_loading = false;
  QStringList m_folderStack;
  QVector<NoteItem> m_items;

  QFutureWatcher<QVector<NoteItem>> *m_watcher = nullptr;
  QFileSystemWatcher *m_fsWatcher = nullptr;
};
