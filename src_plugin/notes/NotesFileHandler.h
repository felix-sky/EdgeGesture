#pragma once

#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml>


class NotesFileHandler : public QObject {
  Q_OBJECT
  QML_ELEMENT

public:
  explicit NotesFileHandler(QObject *parent = nullptr);

  /**
   * @brief Creates a new note file.
   * @param folderPath The folder to create the note in.
   * @param title The note title (filename without extension).
   * @param content The note content.
   * @param color The note color for frontmatter.
   * @return The full path to the created file, or empty string on failure.
   */
  Q_INVOKABLE QString createNote(const QString &folderPath,
                                 const QString &title, const QString &content,
                                 const QString &color);

  /**
   * @brief Saves content to an existing note.
   * @param filePath The full path to the note file.
   * @param content The note content.
   * @param color The note color for frontmatter.
   * @return True if successful.
   */
  Q_INVOKABLE bool saveNote(const QString &filePath, const QString &content,
                            const QString &color);

  /**
   * @brief Reads a note's content and metadata.
   * @param filePath The full path to the note file.
   * @return A map with "content" and "color" keys.
   */
  Q_INVOKABLE QVariantMap readNote(const QString &filePath);

  /**
   * @brief Creates a new folder.
   * @param parentPath The parent folder path.
   * @param name The folder name.
   * @return True if successful.
   */
  Q_INVOKABLE bool createFolder(const QString &parentPath, const QString &name);

  /**
   * @brief Deletes a file or folder.
   * @param path The path to delete.
   * @param isFolder True if it's a folder.
   * @return True if successful.
   */
  Q_INVOKABLE bool deleteItem(const QString &path, bool isFolder);

  /**
   * @brief Renames a file or folder.
   * @param oldPath The current path.
   * @param newName The new name (without extension for files).
   * @param isFolder True if it's a folder.
   * @return The new path if successful, or empty string on failure.
   */
  Q_INVOKABLE QString renameItem(const QString &oldPath, const QString &newName,
                                 bool isFolder);

  /**
   * @brief Checks if a path exists.
   * @param path The path to check.
   * @return True if exists.
   */
  Q_INVOKABLE bool exists(const QString &path);

  /**
   * @brief Gets the filename from a path.
   * @param path The path.
   * @return The filename.
   */
  Q_INVOKABLE QString getFileName(const QString &path);

  /**
   * @brief Converts a URL string to a local file path.
   * @param urlString The URL string.
   * @return The local file path.
   */
  Q_INVOKABLE QString urlToPath(const QString &urlString);

  /**
   * @brief Gets the base name (without extension) from a path.
   * @param path The path.
   * @return The base name.
   */
  Q_INVOKABLE QString getBaseName(const QString &path);

  /**
   * @brief Updates a single frontmatter key-value pair.
   * @param path The path to the note file.
   * @param key The frontmatter key to update.
   * @param value The new value.
   * @return True if successful.
   */
  Q_INVOKABLE bool updateFrontmatter(const QString &path, const QString &key,
                                     const QVariant &value);

  /**
   * @brief Gets the tags from a note's frontmatter.
   * @param path The path to the note file.
   * @return List of tags.
   */
  Q_INVOKABLE QStringList getTags(const QString &path);

  /**
   * @brief Saves an image from the clipboard to the specified folder.
   * @param folderPath The folder to save the image in.
   * @return The saved filename if successful, or empty string.
   */
  Q_INVOKABLE QString saveClipboardImage(const QString &folderPath);

  /**
   * @brief Finds an image file within the notes vault.
   * Searches in common Obsidian locations: same folder, attachments, images
   * subfolders.
   * @param imageName The image filename (e.g., "Pasted image
   * 20260101211933.png").
   * @param notePath The path to the note containing the image reference.
   * @param rootPath The root path of the notes vault.
   * @return The full path to the image if found, or empty string if not found.
   */
  Q_INVOKABLE QString findImage(const QString &imageName,
                                const QString &notePath,
                                const QString &rootPath);

private:
  QString sanitizeFileName(const QString &name);
  QString normalizePath(const QString &path);
};
