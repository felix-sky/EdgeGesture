#ifndef NOTEBLOCKMODEL_H
#define NOTEBLOCKMODEL_H

#include "NoteBlock.h"
#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QVariant>
#include <QVector>
#include <QtQml>
#include <QtQml/qqmlregistration.h>

class NoteBlockModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
  Q_PROPERTY(bool isModified READ isModified NOTIFY isModifiedChanged)

public:
  enum NoteBlockRoles {
    TypeRole = Qt::UserRole + 1,
    ContentRole,
    MetadataRole,
    LevelRole,
    LanguageRole,
    HeightHintRole,
    FormattedContentRole, // Pre-rendered HTML for display
    RawRole,
    FoldStateRole,
    IsFoldableRole,
    IsCollapsedRole
  };

  explicit NoteBlockModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;
  QHash<int, QByteArray> roleNames() const override;

  bool loading() const { return m_loading; }
  bool isModified() const { return m_isModified; }

  Q_INVOKABLE void loadMarkdown(const QString &content);
  Q_INVOKABLE void updateBlock(int index, const QString &text);
  Q_INVOKABLE void insertBlock(int index, const QString &type,
                               const QString &content);
  Q_INVOKABLE void replaceBlock(int index, const QString &text);
  Q_INVOKABLE void removeBlock(int index);
  Q_INVOKABLE void toggleCalloutFold(int index);
  Q_INVOKABLE QString getMarkdown() const;
  Q_INVOKABLE void setDarkMode(bool dark);

signals:
  void loadingChanged();
  void isModifiedChanged();

private slots:
  void onParseFinished();

private:
  QVector<NoteBlock> m_blocks;
  QString m_originalMarkdown;
  bool m_isModified = false;
  bool m_loading = false;
  bool m_darkMode = true;
  QFutureWatcher<QVector<NoteBlock>> *m_watcher;
};

#endif // NOTEBLOCKMODEL_H
