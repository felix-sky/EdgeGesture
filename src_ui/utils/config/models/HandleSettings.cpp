#include "HandleSettings.h"
#include <QJsonObject>

HandleSettings::HandleSettings(QObject *parent) : QObject(parent) {}

void HandleSettings::setEnabled(bool v) {
  if (m_enabled != v) {
    m_enabled = v;
    emit enabledChanged();
    emit settingsChanged();
  }
}

void HandleSettings::setWidth(int v) {
  if (m_width != v) {
    m_width = v;
    emit widthChanged();
    emit settingsChanged();
  }
}

void HandleSettings::setSize(int v) {
  if (m_size != v) {
    m_size = v;
    emit sizeChanged();
    emit settingsChanged();
  }
}

void HandleSettings::setPosition(int v) {
  if (m_position != v) {
    m_position = v;
    emit positionChanged();
    emit settingsChanged();
  }
}

void HandleSettings::setColor(const QString &v) {
  if (m_color != v) {
    m_color = v;
    emit colorChanged();
    emit settingsChanged();
  }
}

void HandleSettings::loadFromConfig(const QJsonObject &data) {
  if (data.contains("enabled"))
    setEnabled(data["enabled"].toBool());
  if (data.contains("width"))
    setWidth(data["width"].toInt());
  if (data.contains("size"))
    setSize(data["size"].toInt());
  if (data.contains("position"))
    setPosition(data["position"].toInt());
  if (data.contains("color"))
    setColor(data["color"].toString());
}

void HandleSettings::saveToConfig(QJsonObject &data) const {
  data["enabled"] = m_enabled;
  data["width"] = m_width;
  data["size"] = m_size;
  data["position"] = m_position;
  data["color"] = m_color;
}
