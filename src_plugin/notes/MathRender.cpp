#include "MathRender.h"

#define BUILD_QT
#include "latex.h"
#include "platform/qt/graphic_qt.h"

#include <QBuffer>
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QPainter>
#include <QSvgGenerator>
#include <exception>
#include <string>

MathRender *MathRender::s_instance = nullptr;
bool MathRender::s_initialized = false;

MathRender::MathRender(QObject *parent) : QObject(parent) {}

MathRender::~MathRender() {}

MathRender *MathRender::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine) {
  Q_UNUSED(qmlEngine);
  Q_UNUSED(jsEngine);
  return instance();
}

MathRender *MathRender::instance() {
  if (!s_instance) {
    s_instance = new MathRender();
  }
  return s_instance;
}

void MathRender::ensureInit() {
  if (!s_initialized) {
    try {
      QString appDir = QCoreApplication::applicationDirPath();
      qDebug() << "MathRender: App Dir:" << appDir;

      QStringList possiblePaths = {
          appDir + "/res", appDir + "/plugin/additional/EdgeGesture/Notes/res",
          QDir::currentPath() + "/res", "res"};

      bool found = false;
      for (const QString &path : possiblePaths) {
        if (QDir(path).exists()) {
          qDebug() << "MathRender: Found res at:" << path;
          tex::LaTeX::init(path.toStdString());
          found = true;
          break;
        }
      }

      if (!found) {
        qWarning() << "MathRender: Could not find 'res' folder!";
      }

      s_initialized = true;
    } catch (std::exception &e) {
      qWarning() << "MathRender init failed:" << e.what();
    }
  }
}

QString MathRender::renderToSvg(const QString &latex, float fontSize,
                                const QString &colorName) {
  ensureInit();

  if (latex.trimmed().isEmpty()) {
    return QString();
  }

  QColor color(colorName);
  if (!color.isValid()) {
    color = Qt::black;
  }

  try {
    std::wstring wlatex = latex.toStdWString();
    tex::color c = (color.alpha() << 24) | (color.red() << 16) |
                   (color.green() << 8) | color.blue();

    tex::TeXRender *render =
        tex::LaTeX::parse(wlatex, 720, fontSize, fontSize / 3.0f, c);

    if (render) {
      int w = render->getWidth();
      int h = render->getHeight();
      const int padding = 6; // Padding to prevent cropping
      int finalW = w + padding * 2;
      int finalH = h + padding * 2;

      QBuffer buffer;
      buffer.open(QIODevice::WriteOnly);

      QSvgGenerator generator;
      generator.setOutputDevice(&buffer);
      generator.setSize(QSize(finalW, finalH));
      generator.setViewBox(QRect(0, 0, finalW, finalH));
      generator.setTitle("MicroTeX Formula");
      generator.setDescription(latex);

      QPainter p;
      p.begin(&generator);
      p.setRenderHint(QPainter::Antialiasing);
      p.setRenderHint(QPainter::TextAntialiasing);

      tex::Graphics2D_qt g2(&p);
      render->draw(g2, padding, padding);

      p.end();

      delete render;

      return QString::fromUtf8(buffer.data());
    } else {
      qWarning() << "MathRender: Parse returned null for:" << latex;
    }
  } catch (std::exception &e) {
    qWarning() << "MathRender SVG failed:" << e.what();
  }

  return QString();
}
