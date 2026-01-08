#pragma once

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QString>
#include <QTextStream>
#include <QUrl>

class FileBridge : public QObject {
  Q_OBJECT
public:
  explicit FileBridge(QObject *parent = nullptr);

  /**
   * @brief Reads the content of a file.
   * @param path The absolute path or file URL to the file.
   * @return The content of the file, or an empty string on failure.
   */
  Q_INVOKABLE QString readFile(const QString &path);

  /**
   * @brief Writes content to a file. Overwrites existing content.
   * @param path The absolute path or file URL to the file.
   * @param content The content to write.
   * @return True if successful, false otherwise.
   */
  Q_INVOKABLE bool writeFile(const QString &path, const QString &content);

  /**
   * @brief Appends content to a file.
   * @param path The absolute path or file URL to the file.
   * @param content The content to append.
   * @return True if successful, false otherwise.
   */
  Q_INVOKABLE bool appendFile(const QString &path, const QString &content);

  /**
   * @brief Checks if a file or directory exists.
   * @param path The absolute path or file URL.
   * @return True if it exists, false otherwise.
   */
  Q_INVOKABLE bool exists(const QString &path);

  /**
   * @brief Deletes a file.
   * @param path The absolute path or file URL.
   * @return True if successful, false otherwise.
   */
  Q_INVOKABLE bool removeFile(const QString &path);

  /**
   * @brief Creates a directory.
   * @param path The absolute path or file URL.
   * @return True if successful, false otherwise.
   */
  Q_INVOKABLE bool createDirectory(const QString &path);

  /**
   * @brief Gets the file name from a path.
   * @param path The path.
   * @return The file name.
   */
  Q_INVOKABLE QString getFileName(const QString &path);

  /**
   * @brief Converts a URL string to a local file path.
   * @param urlString The URL string (e.g. "file:///C:/path/to/file").
   * @return The local file path.
   */
  Q_INVOKABLE QString urlToPath(const QString &urlString);

  /**
   * @brief Gets the absolute path of the application directory.
   * @return The application directory path.
   */
  Q_INVOKABLE QString getAppPath();

  /**
   * @brief Lists all files and directories in a directory.
   * @param path The directory path.
   * @return A JSON array string of objects with name, path, isDir, and
   * modifiedDate.
   */
  Q_INVOKABLE QString listDirectory(const QString &path);

  /**
   * @brief Removes a directory and all its contents recursively.
   * @param path The directory path.
   * @return True if successful, false otherwise.
   */
  Q_INVOKABLE bool removeDirectory(const QString &path);

  /**
   * @brief Checks if the given path is a directory.
   * @param path The path to check.
   * @return True if it's a directory, false otherwise.
   */
  Q_INVOKABLE bool isDirectory(const QString &path);

  /**
   * @brief Gets the file name without extension from a path.
   * @param path The path.
   * @return The file name without extension.
   */
  Q_INVOKABLE QString getBaseName(const QString &path);

private:
  QString normalizePath(const QString &path);
};
