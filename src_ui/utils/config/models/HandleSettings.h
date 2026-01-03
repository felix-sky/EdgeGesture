#ifndef HANDLESETTINGS_H
#define HANDLESETTINGS_H

#include <QObject>

class HandleSettings : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
  Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged)
  Q_PROPERTY(int size READ size WRITE setSize NOTIFY sizeChanged)
  Q_PROPERTY(
      int position READ position WRITE setPosition NOTIFY positionChanged)
  Q_PROPERTY(QString color READ color WRITE setColor NOTIFY colorChanged)

public:
  explicit HandleSettings(QObject *parent = nullptr);

  bool enabled() const { return m_enabled; }
  void setEnabled(bool v);

  int width() const { return m_width; }
  void setWidth(int v);

  int size() const { return m_size; }
  void setSize(int v);

  int position() const { return m_position; }
  void setPosition(int v);

  QString color() const { return m_color; }
  void setColor(const QString &v);

  // IO
  void loadFromConfig(const QJsonObject &data);
  void saveToConfig(QJsonObject &data) const;

signals:
  void enabledChanged();
  void widthChanged();
  void sizeChanged();
  void positionChanged();
  void colorChanged();
  void settingsChanged();

private:
  bool m_enabled = true;
  int m_width = 30;
  int m_size = 100;
  int m_position = 50;
  QString m_color = "#000000";
};

#endif // HANDLESETTINGS_H
