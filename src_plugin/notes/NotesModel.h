#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QString>
#include <QVector>
#include <QtQml>

struct NoteItem {
  QString type;           // "folder" or "note"
  QString title;          // Display name
  QString path;           // Full file path
  QString date;           // Modified date string
  QString color;          // Color from frontmatter
  QString preview;        // Content preview (lazy loaded)
  QStringList tags;       // Tags from frontmatter
  bool isPinned;          // Pinned status
  QDateTime lastModified; // For sorting
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
  Q_PROPERTY(QString filterString READ filterString WRITE setFilterString NOTIFY
                 filterStringChanged)
  Q_PROPERTY(QString filterTag READ filterTag WRITE setFilterTag NOTIFY
                 filterTagChanged)
  Q_PROPERTY(bool isSearchMode READ isSearchMode NOTIFY isSearchModeChanged)

public:
  enum NotesRoles {
    TypeRole = Qt::UserRole + 1,
    TitleRole,
    PathRole,
    DateRole,
    ColorRole,
    PreviewRole,
    TagsRole,
    IsPinnedRole
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

  QString filterString() const;
  void setFilterString(const QString &filter);

  QString filterTag() const;
  void setFilterTag(const QString &tag);

  bool isSearchMode() const;

  // Q_INVOKABLE methods for QML
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void navigateToFolder(const QString &folderPath);
  Q_INVOKABLE void navigateBack();
  Q_INVOKABLE void navigateToRoot();
  Q_INVOKABLE void clearFilters();
  Q_INVOKABLE void togglePin(const QString &path);
  Q_INVOKABLE QString findPathByTitle(const QString &title);
  Q_INVOKABLE QStringList getBacklinks(const QString &title);
  Q_INVOKABLE void searchContent(const QString &query);
  Q_INVOKABLE QStringList getAllTags();

signals:
  void currentPathChanged();
  void rootPathChanged();
  void loadingChanged();
  void folderStackChanged();
  void filterStringChanged();
  void filterTagChanged();
  void isSearchModeChanged();
  void errorOccurred(const QString &message);

private slots:
  void onScanFinished();
  void onDirectoryChanged(const QString &path);
  void onIndexReady();
  void onEntryUpdated(const QString &path);
  void onContentSearchFinished();

private:
  void startScan();
  void loadFromIndex();
  void applySorting();
  static QString createPreview(const QString &filePath, int maxLength = 100);

  QString m_currentPath;
  QString m_rootPath;
  bool m_loading = false;
  QStringList m_folderStack;
  QVector<NoteItem> m_items;

  QString m_filterString;
  QString m_filterTag;
  bool m_isSearchMode = false;

  QFutureWatcher<QVector<NoteItem>> *m_watcher = nullptr;
  QFutureWatcher<QVector<NoteItem>> *m_contentSearchWatcher = nullptr;
  QFileSystemWatcher *m_fsWatcher = nullptr;
};
