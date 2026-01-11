#include "MathImageProvider.h"
#include <QStringList>

#define BUILD_QT
#include "latex.h"
#include "platform/qt/graphic_qt.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QPainter>
#include <QUrl>
#include <exception>
#include <stdexcept>
#include <string>

bool MathImageProvider::s_initialized = false;

MathImageProvider::MathImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {}

MathImageProvider::~MathImageProvider() {}

void MathImageProvider::ensureInit() {
  if (!s_initialized) {
    try {
      QString appDir = QCoreApplication::applicationDirPath();
      qDebug() << "MathImageProvider: App Dir:" << appDir;

      // Try multiple common locations for 'res'
      QStringList possiblePaths = {
          appDir + "/res",
          appDir + "/plugin/additional/EdgeGesture/Notes/res", // Deployment
          QDir::currentPath() + "/res", "res"};

      bool found = false;
      for (const QString &path : possiblePaths) {
        if (QDir(path).exists()) {
          qDebug() << "MathImageProvider: Found res at:" << path;
          tex::LaTeX::init(path.toStdString());
          found = true;
          break;
        }
      }

      if (!found) {
        qWarning() << "MathImageProvider: Could not find 'res' folder in any "
                      "expected location!";
      }

      s_initialized = true;
    } catch (std::exception &e) {
      qWarning() << "MicroTeX init failed:" << e.what();
    }
  }
}

QImage MathImageProvider::requestImage(const QString &id, QSize *size,
                                       const QSize &requestedSize) {
  qCritical() << "MathImageProvider: Requesting image for ID:" << id;
  ensureInit();

  // ID format: latex_string?color=hex&size=float
  QString decodedId = QUrl::fromPercentEncoding(id.toUtf8());
  QString latex = decodedId;
  QColor color = Qt::black;
  float fontSize = 20.0f;

  // Simple param parsing
  int queryIdx = latex.indexOf('?');
  if (queryIdx != -1) {
    QString queryString = latex.mid(queryIdx + 1);
    latex = latex.left(queryIdx);

    QStringList parts = queryString.split('&');
    for (const QString &part : parts) {
      QStringList kv = part.split('=');
      if (kv.size() == 2) {
        if (kv[0] == "color") {
          color = QColor(kv[1]);
        } else if (kv[0] == "size") {
          fontSize = kv[1].toFloat();
        }
      }
    }
  }

  qDebug() << "MathImageProvider: Parsed Latex:" << latex << "Color:" << color
           << "Size:" << fontSize;

  if (latex.isEmpty()) {
    return QImage();
  }

  try {
    std::wstring wlatex = latex.toStdWString();
    tex::color c = (color.alpha() << 24) | (color.red() << 16) |
                   (color.green() << 8) | color.blue();

    // Render at requested size (no scaling - HTML Text doesn't respect
    // devicePixelRatio)
    tex::TeXRender *render =
        tex::LaTeX::parse(wlatex, 720, fontSize, fontSize / 3.0f, c);
    if (render) {
      int w = render->getWidth();
      int h = render->getHeight();
      qDebug() << "MathImageProvider: Rendered size:" << w << "x" << h;

      // Add padding around the math to prevent clipping
      const int padding = 8;
      QImage image(w + padding * 2, h + padding * 2, QImage::Format_ARGB32);
      image.fill(Qt::transparent);

      QPainter p(&image);
      p.setRenderHint(QPainter::Antialiasing);
      p.setRenderHint(QPainter::TextAntialiasing);
      p.setRenderHint(QPainter::SmoothPixmapTransform);

      tex::Graphics2D_qt g2(&p);
      // Draw with padding offset
      render->draw(g2, padding, padding);

      delete render;

      if (size)
        *size = image.size();
      return image;
    } else {
      qWarning() << "MathImageProvider: Parse returned null render object.";
    }
  } catch (std::exception &e) {
    qWarning() << "MicroTeX render failed:" << e.what();
  }

  return QImage();
}
