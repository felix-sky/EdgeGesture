#include "CodeHighlighter.h"
#include <QTextDocument>

KSyntaxHighlighting::Repository *CodeHighlighter::s_repository = nullptr;

CodeHighlighter::CodeHighlighter(QObject *parent) : QObject(parent) {
  if (!s_repository) {
    s_repository = new KSyntaxHighlighting::Repository();
  }
}

CodeHighlighter::~CodeHighlighter() {
  // QPointer automatically becomes nullptr if the SyntaxHighlighter
  // was already deleted by its parent (QTextDocument)
  if (m_highlighter) {
    m_highlighter->setDocument(nullptr);
    delete m_highlighter;
  }
}

void CodeHighlighter::setTextEdit(QQuickItem *textEdit) {
  if (m_textEdit == textEdit)
    return;

  m_textEdit = textEdit;
  emit textEditChanged();
  updateHighlighter();
}

void CodeHighlighter::setLanguage(const QString &language) {
  if (m_language == language)
    return;

  m_language = language;
  emit languageChanged();
  applyDefinition();
}

void CodeHighlighter::setDarkTheme(bool dark) {
  if (m_darkTheme == dark)
    return;

  m_darkTheme = dark;
  emit darkThemeChanged();
  applyTheme();
}

void CodeHighlighter::updateHighlighter() {
  if (!m_textEdit)
    return;

  // Get QQuickTextDocument from the TextEdit/TextArea
  QVariant documentVar = m_textEdit->property("textDocument");
  if (!documentVar.isValid())
    return;

  QQuickTextDocument *quickDoc = documentVar.value<QQuickTextDocument *>();
  if (!quickDoc)
    return;

  QTextDocument *doc = quickDoc->textDocument();
  if (!doc)
    return;

  // QPointer automatically becomes nullptr if previously deleted by doc
  if (m_highlighter) {
    m_highlighter->setDocument(nullptr);
    delete m_highlighter;
  }

  m_highlighter = new KSyntaxHighlighting::SyntaxHighlighter(doc);
  applyDefinition();
  applyTheme();
}

void CodeHighlighter::applyDefinition() {
  if (!m_highlighter || !s_repository)
    return;

  try {
    if (m_language.isEmpty()) {
      m_highlighter->setDefinition(KSyntaxHighlighting::Definition());
    } else {
      auto def = s_repository->definitionForName(m_language);
      if (!def.isValid()) {
        // Try by file extension as fallback
        QString fileName = QStringLiteral("file.") + m_language.toLower();
        def = s_repository->definitionForFileName(fileName);
      }
      m_highlighter->setDefinition(def);
    }
  } catch (...) {
    // Silently ignore exceptions
  }
}

void CodeHighlighter::applyTheme() {
  if (!m_highlighter || !s_repository)
    return;

  try {
    auto theme = s_repository->defaultTheme(
        m_darkTheme ? KSyntaxHighlighting::Repository::DarkTheme
                    : KSyntaxHighlighting::Repository::LightTheme);
    m_highlighter->setTheme(theme);
  } catch (...) {
    // Silently ignore exceptions
  }
}
