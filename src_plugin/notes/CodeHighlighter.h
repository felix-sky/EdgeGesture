#ifndef CODEHIGHLIGHTER_H
#define CODEHIGHLIGHTER_H

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>

class CodeHighlighter : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QQuickItem *textEdit READ textEdit WRITE setTextEdit NOTIFY
                 textEditChanged)
  Q_PROPERTY(
      QString language READ language WRITE setLanguage NOTIFY languageChanged)
  Q_PROPERTY(
      bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)

public:
  explicit CodeHighlighter(QObject *parent = nullptr);
  ~CodeHighlighter() override;

  QQuickItem *textEdit() const { return m_textEdit; }
  void setTextEdit(QQuickItem *textEdit);

  QString language() const { return m_language; }
  void setLanguage(const QString &language);

  bool darkTheme() const { return m_darkTheme; }
  void setDarkTheme(bool dark);

signals:
  void textEditChanged();
  void languageChanged();
  void darkThemeChanged();

private:
  void updateHighlighter();
  void applyDefinition();
  void applyTheme();

  QQuickItem *m_textEdit = nullptr;
  QString m_language;
  bool m_darkTheme = true;
  QPointer<KSyntaxHighlighting::SyntaxHighlighter> m_highlighter;
  static KSyntaxHighlighting::Repository *s_repository;
};

#endif // CODEHIGHLIGHTER_H
