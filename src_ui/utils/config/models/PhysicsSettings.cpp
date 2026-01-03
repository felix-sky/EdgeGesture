#include "PhysicsSettings.h"
#include <QJsonObject>

PhysicsSettings::PhysicsSettings(QObject *parent) : QObject(parent) {}

void PhysicsSettings::setTension(double v) {
  if (qAbs(m_tension - v) > 0.001) {
    m_tension = v;
    emit tensionChanged();
    emit settingsChanged();
  }
}

void PhysicsSettings::setFriction(double v) {
  if (qAbs(m_friction - v) > 0.001) {
    m_friction = v;
    emit frictionChanged();
    emit settingsChanged();
  }
}

void PhysicsSettings::setVerticalRange(int v) {
  if (m_verticalRange != v) {
    m_verticalRange = v;
    emit verticalRangeChanged();
    emit settingsChanged();
  }
}

void PhysicsSettings::setLongSwipeThreshold(double v) {
  if (qAbs(m_longSwipeThreshold - v) > 0.001) {
    m_longSwipeThreshold = v;
    emit longSwipeThresholdChanged();
    emit settingsChanged();
  }
}

void PhysicsSettings::setShortSwipeThreshold(double v) {
  if (qAbs(m_shortSwipeThreshold - v) > 0.001) {
    m_shortSwipeThreshold = v;
    emit shortSwipeThresholdChanged();
    emit settingsChanged();
  }
}

void PhysicsSettings::loadFromConfig(const QJsonObject &data) {
  QJsonObject physics = data.value("physics").toObject();
  if (physics.contains("tension"))
    setTension(physics["tension"].toDouble());
  if (physics.contains("friction"))
    setFriction(physics["friction"].toDouble());

  QJsonObject general = data.value("general").toObject();
  if (general.contains("vertical_range"))
    setVerticalRange(general["vertical_range"].toInt());
  if (general.contains("long_swipe_threshold"))
    setLongSwipeThreshold(general["long_swipe_threshold"].toDouble());
  if (general.contains("short_swipe_threshold"))
    setShortSwipeThreshold(general["short_swipe_threshold"].toDouble());
}

void PhysicsSettings::saveToConfig(QJsonObject &data) const {
  QJsonObject physics = data.value("physics").toObject();
  physics["tension"] = m_tension;
  physics["friction"] = m_friction;
  data["physics"] = physics;

  QJsonObject general = data.value("general").toObject();
  general["vertical_range"] = m_verticalRange;
  general["long_swipe_threshold"] = m_longSwipeThreshold;
  general["short_swipe_threshold"] = m_shortSwipeThreshold;
  // split_mode is handled by bridge currently, but conceptually belongs here or
  // in LayoutSettings
  data["general"] = general;
}
