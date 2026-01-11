#ifndef NOTEBLOCKMODEL_H
#define NOTEBLOCKMODEL_H

#include "NoteBlock.h"
#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QVariant>
#include <QVector>
#include <QtQml>

class NoteBlockModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
  enum NoteBlockRoles {
    TypeRole = Qt::UserRole + 1,
    ContentRole,
    MetadataRole,
    LevelRole,
    LanguageRole,
    HeightHintRole,
    FormattedContentRole // Pre-rendered HTML for display
  };

  explicit NoteBlockModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  bool loading() const { return m_loading; }

  Q_INVOKABLE void loadMarkdown(const QString &content);
  Q_INVOKABLE void updateBlock(int index, const QString &text);
  Q_INVOKABLE void insertBlock(int index, const QString &type,
                               const QString &content);
  Q_INVOKABLE void replaceBlock(int index, const QString &text);
  Q_INVOKABLE void removeBlock(int index);
  Q_INVOKABLE QString getMarkdown() const;
  Q_INVOKABLE void setDarkMode(bool dark); // Theme-aware formatting

signals:
  void loadingChanged();

private slots:
  void onParseFinished();

private:
  QVector<NoteBlock> m_blocks;
  bool m_loading = false;
  bool m_darkMode = true; // Theme for formatted content
  QFutureWatcher<QVector<NoteBlock>> *m_watcher;
};

#endif // NOTEBLOCKMODEL_H
