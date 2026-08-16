#pragma once

#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtConcurrent>
#include <QtQml>
#include <QtQml/qqmlregistration.h>

class NotesFileHandler : public QObject {
  Q_OBJECT
  QML_ELEMENT

public:
  explicit NotesFileHandler(QObject *parent = nullptr);

  /**
   * @brief Creates a new note file atomically.
   */
  Q_INVOKABLE QString createNote(const QString &folderPath,
                                 const QString &title, const QString &content,
                                 const QString &color);

  /**
   * @brief Saves content to an existing note atomically, preserving all arbitrary frontmatter.
   */
  Q_INVOKABLE bool saveNote(const QString &filePath, const QString &content,
                            const QString &color);

  /**
   * @brief Reads a note's content and metadata.
   */
  Q_INVOKABLE QVariantMap readNote(const QString &filePath);

  /**
   * @brief Creates a new folder.
   */
  Q_INVOKABLE bool createFolder(const QString &parentPath, const QString &name);

  /**
   * @brief Deletes a file or folder.
   */
  Q_INVOKABLE bool deleteItem(const QString &path, bool isFolder);

  /**
   * @brief Renames a file or folder.
   */
  Q_INVOKABLE QString renameItem(const QString &oldPath, const QString &newName,
                                 bool isFolder);

  /**
   * @brief Checks if a path exists.
   */
  Q_INVOKABLE bool exists(const QString &path);

  /**
   * @brief Gets the filename from a path.
   */
  Q_INVOKABLE QString getFileName(const QString &path);

  /**
   * @brief Converts a URL string to a local file path.
   */
  Q_INVOKABLE QString urlToPath(const QString &urlString);

  /**
   * @brief Gets the base name (without extension) from a path.
   */
  Q_INVOKABLE QString getBaseName(const QString &path);

  /**
   * @brief Updates a single frontmatter key-value pair atomically, preserving all other fields.
   */
  Q_INVOKABLE bool updateFrontmatter(const QString &path, const QString &key,
                                     const QVariant &value);

  /**
   * @brief Gets the tags from a note's frontmatter.
   */
  Q_INVOKABLE QStringList getTags(const QString &path);

  /**
   * @brief Saves an image from the clipboard to the specified folder.
   */
  Q_INVOKABLE QString saveClipboardImage(const QString &folderPath);

  /**
   * @brief Finds an image file within the notes vault.
   */
  Q_INVOKABLE QString findImage(const QString &imageName,
                                const QString &notePath,
                                const QString &rootPath);

  /**
   * @brief Asynchronously finds an image file within the notes vault.
   */
  Q_INVOKABLE void findImageAsync(const QString &imageName,
                                  const QString &notePath,
                                  const QString &rootPath);

  /**
   * @brief Extracts a section from a markdown file by heading name.
   */
  Q_INVOKABLE QString extractSection(const QString &notePath,
                                     const QString &sectionName);

  /**
   * @brief Extracts a block by block ID from a markdown file.
   */
  Q_INVOKABLE QString extractBlock(const QString &notePath,
                                   const QString &blockId);

signals:
  void imagePathFound(const QString &originalName, const QString &foundPath);

private:
  QString sanitizeFileName(const QString &name);
  QString normalizePath(const QString &path);

  // Cache for image paths to avoid repeated disk scans
  QHash<QString, QString> m_imageCache;
};
