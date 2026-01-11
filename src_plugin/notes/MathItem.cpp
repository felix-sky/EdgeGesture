#include "MathItem.h"
#include "latex.h"
#include "platform/qt/graphic_qt.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>


bool MathItem::s_initialized = false;

MathItem::MathItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  checkInit();
}

MathItem::~MathItem() {
  if (m_render) {
    delete m_render;
    m_render = nullptr;
  }
}

void MathItem::checkInit() {
  if (!s_initialized) {
    // Assume 'res' folder is in the same directory as the plugin/executable or
    // "res" in CWD For robustness, try to find it. In this setup, CMake copies
    // 'res' to the plugin output directory. We might need to find where the
    // plugin library is loaded from, but for now let's try strict relative
    // "res" or look relative to application dir.

    // MicroTeX expects std::string
    // We can pass absolute path if needed.
    QString appDir = QCoreApplication::applicationDirPath();
    // Assuming plugin layout: .../Notes/res
    // But appDir might be .../Release/

    // Let's try default "res" first (CWD).
    // If fail, we might see errors.
    try {
      // For now, let's point to where we copy it:
      // The cmake copies to
      // ${CMAKE_BINARY_DIR}/$<CONFIG>/plugin/additional/EdgeGesture/Notes/ The
      // executable is likely in ${CMAKE_BINARY_DIR}/$<CONFIG>/ So it might be
      // in plugin/additional/EdgeGesture/Notes/res

      QString resPath = appDir + "/plugin/additional/EdgeGesture/Notes/res";
      if (QDir(resPath).exists()) {
        tex::LaTeX::init(resPath.toStdString());
      } else {
        // Fallback to local 'res' if strict
        tex::LaTeX::init("res");
      }
      s_initialized = true;
    } catch (std::exception &e) {
      qWarning() << "MicroTeX init failed:" << e.what();
    }
  }
}

QString MathItem::latex() const { return m_latex; }

void MathItem::setLatex(const QString &latex) {
  if (m_latex != latex) {
    m_latex = latex;
    emit latexChanged();
    updateRender();
  }
}

QColor MathItem::color() const { return m_color; }

void MathItem::setColor(const QColor &color) {
  if (m_color != color) {
    m_color = color;
    emit colorChanged();
    updateRender();
  }
}

qreal MathItem::fontSize() const { return m_fontSize; }

void MathItem::setFontSize(qreal size) {
  if (m_fontSize != size) {
    m_fontSize = size;
    emit fontSizeChanged();
    updateRender();
  }
}

void MathItem::updateRender() {
  if (m_render) {
    delete m_render;
    m_render = nullptr;
  }

  if (m_latex.isEmpty()) {
    update();
    return;
  }

  try {
    // arg: latex, width (0 for auto), textSize, lineSpace, color
    std::wstring wlatex = m_latex.toStdWString();
    // Convert QColor to tex::color (ARGB 8888)
    tex::color c = (m_color.alpha() << 24) | (m_color.red() << 16) |
                   (m_color.green() << 8) | m_color.blue();

    m_render = tex::LaTeX::parse(wlatex, 0, (float)m_fontSize,
                                 (float)m_fontSize / 3.0f, c);

    if (m_render) {
      setWidth(m_render->getWidth());
      setHeight(m_render->getHeight());
    }
  } catch (std::exception &e) {
    qWarning() << "MicroTeX parse error:" << e.what();
  }
  update();
}

void MathItem::paint(QPainter *painter) {
  if (!m_render)
    return;

  try {
    tex::Graphics2D_qt g2(painter);
    m_render->draw(g2, 0, 0);
  } catch (std::exception &e) {
    qWarning() << "MicroTeX paint error:" << e.what();
  }
}
