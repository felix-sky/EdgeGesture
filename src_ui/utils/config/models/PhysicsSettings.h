#ifndef PHYSICSSETTINGS_H
#define PHYSICSSETTINGS_H

#include <QObject>

class PhysicsSettings : public QObject {
  Q_OBJECT
  Q_PROPERTY(double tension READ tension WRITE setTension NOTIFY tensionChanged)
  Q_PROPERTY(
      double friction READ friction WRITE setFriction NOTIFY frictionChanged)
  Q_PROPERTY(int verticalRange READ verticalRange WRITE setVerticalRange NOTIFY
                 verticalRangeChanged)
  Q_PROPERTY(double longSwipeThreshold READ longSwipeThreshold WRITE
                 setLongSwipeThreshold NOTIFY longSwipeThresholdChanged)
  Q_PROPERTY(double shortSwipeThreshold READ shortSwipeThreshold WRITE
                 setShortSwipeThreshold NOTIFY shortSwipeThresholdChanged)

public:
  explicit PhysicsSettings(QObject *parent = nullptr);

  double tension() const { return m_tension; }
  void setTension(double v);

  double friction() const { return m_friction; }
  void setFriction(double v);

  int verticalRange() const { return m_verticalRange; }
  void setVerticalRange(int v);

  double longSwipeThreshold() const { return m_longSwipeThreshold; }
  void setLongSwipeThreshold(double v);

  double shortSwipeThreshold() const { return m_shortSwipeThreshold; }
  void setShortSwipeThreshold(double v);

  // IO
  void loadFromConfig(const QJsonObject &data);
  void saveToConfig(QJsonObject &data) const;

signals:
  void tensionChanged();
  void frictionChanged();
  void verticalRangeChanged();
  void longSwipeThresholdChanged();
  void shortSwipeThresholdChanged();
  void settingsChanged(); // Aggregate signal for ease of saving

private:
  double m_tension = 0.35;
  double m_friction = 0.65;
  int m_verticalRange = 50;
  double m_longSwipeThreshold = 250.0;
  double m_shortSwipeThreshold = 30.0;
};

#endif // PHYSICSSETTINGS_H
