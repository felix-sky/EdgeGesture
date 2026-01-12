#include "SystemBridge.h"
#include "../system/WindowsUtils.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QProcess>
#include <QSettings>
#include <QUrl>
#include <QWindow>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Wbemidl.h>
#include <comdef.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <wlanapi.h>

#include <dwmapi.h>

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")


SystemBridge::SystemBridge(QObject *parent) : QObject(parent) {
  // Initialize COM for audio control - use COINIT_MULTITHREADED if possible, or
  // handle existing init
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    qWarning() << "CoInitializeEx failed: " << hr;
  }

  // Get initial system states
  updateWifiState();
  updateBrightnessLevel();
  updateVolumeLevel();
  updateStartWithWindowsState();
}

void SystemBridge::updateWifiState() {
  // Query WiFi state using WLAN API
  HANDLE hClient = NULL;
  DWORD dwMaxClient = 2;
  DWORD dwCurVersion = 0;

  DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
  if (dwResult == ERROR_SUCCESS) {
    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);

    if (dwResult == ERROR_SUCCESS && pIfList != NULL) {
      bool anyConnected = false;
      for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
        WLAN_INTERFACE_INFO *pIfInfo = &pIfList->InterfaceInfo[i];
        if (pIfInfo->isState == wlan_interface_state_connected) {
          anyConnected = true;
          break;
        }
      }
      if (m_wifiEnabled != anyConnected) {
        m_wifiEnabled = anyConnected;
        emit wifiEnabledChanged();
      }
      WlanFreeMemory(pIfList);
    }
    WlanCloseHandle(hClient, NULL);
  }
}

void SystemBridge::updateBrightnessLevel() {
  // Utilize WMI to get brightness
  IWbemLocator *pLoc = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                                IID_IWbemLocator, (LPVOID *)&pLoc);

  if (SUCCEEDED(hr)) {
    IWbemServices *pSvc = nullptr;
    BSTR namespacePath = SysAllocString(L"ROOT\\WMI");
    hr =
        pLoc->ConnectServer(namespacePath, nullptr, nullptr, 0, 0, 0, 0, &pSvc);
    SysFreeString(namespacePath);

    if (SUCCEEDED(hr)) {
      hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                             RPC_C_AUTHN_LEVEL_CALL,
                             RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

      if (SUCCEEDED(hr)) {
        IEnumWbemClassObject *pEnumerator = nullptr;
        BSTR query = SysAllocString(
            L"SELECT CurrentBrightness FROM WmiMonitorBrightness");
        BSTR lang = SysAllocString(L"WQL");
        hr = pSvc->ExecQuery(
            lang, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &pEnumerator);
        SysFreeString(query);
        SysFreeString(lang);

        if (SUCCEEDED(hr) && pEnumerator) {
          IWbemClassObject *pclsObj = nullptr;
          ULONG uReturn = 0;

          while (pEnumerator) {
            hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (0 == uReturn)
              break;

            VARIANT varProp;
            hr = pclsObj->Get(L"CurrentBrightness", 0, &varProp, 0, 0);
            if (SUCCEEDED(hr)) {
              // WMI returns VT_UI1 or VT_I4 usually
              int level = 0;
              if (varProp.vt == VT_UI1)
                level = varProp.bVal;
              else if (varProp.vt == VT_I4)
                level = varProp.lVal;

              if (m_brightnessLevel != level) {
                m_brightnessLevel = level;
                emit brightnessLevelChanged();
              }
            }
            VariantClear(&varProp);
            pclsObj->Release();
          }
          pEnumerator->Release();
        }
      }
      pSvc->Release();
    }
    pLoc->Release();
  }
}

void SystemBridge::updateVolumeLevel() {
  // Get system volume using Core Audio API
  IMMDeviceEnumerator *deviceEnumerator = NULL;
  CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                   __uuidof(IMMDeviceEnumerator), (LPVOID *)&deviceEnumerator);

  if (deviceEnumerator) {
    IMMDevice *defaultDevice = NULL;
    HRESULT hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia,
                                                           &defaultDevice);
    if (FAILED(hr)) {
      hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                     &defaultDevice);
    }

    if (SUCCEEDED(hr) && defaultDevice) {
      IAudioEndpointVolume *endpointVolume = NULL;
      defaultDevice->Activate(__uuidof(IAudioEndpointVolume),
                              CLSCTX_INPROC_SERVER, NULL,
                              (LPVOID *)&endpointVolume);

      if (endpointVolume) {
        float currentVolume = 0;
        endpointVolume->GetMasterVolumeLevelScalar(&currentVolume);
        int newVolume = static_cast<int>(currentVolume * 100.0f);

        if (m_volumeLevel != newVolume) {
          m_volumeLevel = newVolume;
          emit volumeLevelChanged();
        }

        endpointVolume->Release();
      }
      defaultDevice->Release();
    }
    deviceEnumerator->Release();
  }
}

// Unfortunately, not working
void SystemBridge::setBrightnessLevel(int level) {
  if (level < 0)
    level = 0;
  if (level > 100)
    level = 100;

  if (m_brightnessLevel != level) {
    m_brightnessLevel = level;

    // Utilize WMI to set brightness
    IWbemLocator *pLoc = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                                  IID_IWbemLocator, (LPVOID *)&pLoc);

    if (SUCCEEDED(hr)) {
      IWbemServices *pSvc = nullptr;
      BSTR namespacePath = SysAllocString(L"ROOT\\WMI");
      hr = pLoc->ConnectServer(namespacePath, nullptr, nullptr, 0, 0, 0, 0,
                               &pSvc);
      SysFreeString(namespacePath);

      if (SUCCEEDED(hr)) {
        hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                               nullptr, RPC_C_AUTHN_LEVEL_CALL,
                               RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        if (SUCCEEDED(hr)) {
          IEnumWbemClassObject *pEnumerator = nullptr;
          BSTR query =
              SysAllocString(L"SELECT * FROM WmiMonitorBrightnessMethods");
          BSTR lang = SysAllocString(L"WQL");
          hr = pSvc->ExecQuery(lang, query,
                               WBEM_FLAG_FORWARD_ONLY |
                                   WBEM_FLAG_RETURN_IMMEDIATELY,
                               nullptr, &pEnumerator);
          SysFreeString(query);
          SysFreeString(lang);

          if (SUCCEEDED(hr) && pEnumerator) {
            IWbemClassObject *pclsObj = nullptr;
            ULONG uReturn = 0;

            while (pEnumerator) {
              hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
              if (0 == uReturn)
                break;

              // Execute WmiSetBrightness method
              BSTR methodName = SysAllocString(L"WmiSetBrightness");
              BSTR className = SysAllocString(L"WmiMonitorBrightnessMethods");

              IWbemClassObject *pClass = nullptr;
              pSvc->GetObject(className, 0, nullptr, &pClass, nullptr);

              IWbemClassObject *pInParamsDefinition = nullptr;
              pClass->GetMethod(methodName, 0, &pInParamsDefinition, nullptr);

              IWbemClassObject *pClassInstance = nullptr;
              pInParamsDefinition->SpawnInstance(0, &pClassInstance);

              // Set arguments: Timeout = 1, Brightness = level
              VARIANT varTimeout;
              varTimeout.vt = VT_I4;
              varTimeout.lVal = 1;
              pClassInstance->Put(L"Timeout", 0, &varTimeout, 0);

              VARIANT varBrightness;
              varBrightness.vt = VT_UI1;
              varBrightness.bVal = static_cast<BYTE>(level);
              pClassInstance->Put(L"Brightness", 0, &varBrightness, 0);

              // Execute
              VARIANT pathVar;
              pclsObj->Get(L"__PATH", 0, &pathVar, 0, 0);

              hr = pSvc->ExecMethod(pathVar.bstrVal, methodName, 0, nullptr,
                                    pClassInstance, nullptr, nullptr);

              SysFreeString(methodName);
              SysFreeString(className);
              VariantClear(&pathVar);

              pClass->Release();
              pInParamsDefinition->Release();
              pClassInstance->Release();
              pclsObj->Release();
            }
            pEnumerator->Release();
          }
        }
        pSvc->Release();
      }
      pLoc->Release();
    } else {
      qWarning() << "Failed to connect to WMI for brightness control";
    }

    emit brightnessLevelChanged();
  }
}

void SystemBridge::setVolumeLevel(int level) {
  if (level < 0)
    level = 0;
  if (level > 100)
    level = 100;

  if (m_volumeLevel != level) {
    m_volumeLevel = level;

    // Set system volume using Core Audio API
    IMMDeviceEnumerator *deviceEnumerator = NULL;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                     __uuidof(IMMDeviceEnumerator),
                     (LPVOID *)&deviceEnumerator);

    if (deviceEnumerator) {
      IMMDevice *defaultDevice = NULL;
      HRESULT hr = deviceEnumerator->GetDefaultAudioEndpoint(
          eRender, eMultimedia, &defaultDevice);
      if (FAILED(hr)) {
        // Fallback to console if multimedia fails
        hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                       &defaultDevice);
      }

      if (SUCCEEDED(hr) && defaultDevice) {
        IAudioEndpointVolume *endpointVolume = NULL;
        defaultDevice->Activate(__uuidof(IAudioEndpointVolume),
                                CLSCTX_INPROC_SERVER, NULL,
                                (LPVOID *)&endpointVolume);

        if (endpointVolume) {
          float newVolume = level / 100.0f;
          endpointVolume->SetMasterVolumeLevelScalar(newVolume, NULL);
          endpointVolume->Release();
        }
        defaultDevice->Release();
      }
      deviceEnumerator->Release();
    }

    qDebug() << "[SystemBridge] Volume set to:" << level << "%";
    emit volumeLevelChanged();
  }
}


// WIndows effect ------------
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2      // Mica
#endif

#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3 // Acrylic
#endif

#ifndef DWMSBT_TABBEDWINDOW
#define DWMSBT_TABBEDWINDOW 4    // Mica Alt
#endif

void SystemBridge::applyWindowEffects(QObject *window) {
    QWindow *qWin = qobject_cast<QWindow *>(window);
    if (!qWin) return;

    HWND hwnd = (HWND)qWin->winId();
    bool isConfigWindow = (qWin->title() == QStringLiteral("EdgeGesture Config"));

    DWORD backdropType = isConfigWindow ?  DWMSBT_MAINWINDOW : DWMSBT_TRANSIENTWINDOW;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));

    DWORD preference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

    MARGINS margins = {-1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}
// End of Windows effect ----------------

void SystemBridge::toggleWifi() {
  // Open WiFi settings - actual toggle requires elevated permissions
  QProcess::startDetached("explorer.exe", QStringList()
                                              << "ms-settings:network-wifi");
  qDebug() << "[SystemBridge] Opening WiFi settings";
}

void SystemBridge::toggleBluetooth() {
  // Open Bluetooth settings
  QProcess::startDetached("explorer.exe", QStringList()
                                              << "ms-settings:bluetooth");
  qDebug() << "[SystemBridge] Opening Bluetooth settings";
}

void SystemBridge::toggleNightLight() {
  m_nightLightEnabled = !m_nightLightEnabled;
  emit nightLightEnabledChanged();

  // Open Night Light settings
  QProcess::startDetached("explorer.exe", QStringList()
                                              << "ms-settings:nightlight");
  qDebug() << "[SystemBridge] Night Light toggled:" << m_nightLightEnabled;
}

void SystemBridge::toggleFocusAssist() {
  // Open Focus Assist settings
  QProcess::startDetached("explorer.exe", QStringList()
                                              << "ms-settings:quiethours");
  qDebug() << "[SystemBridge] Opening Focus Assist settings";
}

void SystemBridge::lockScreen() {
  // Lock workstation
  LockWorkStation();
  qDebug() << "[SystemBridge] Locking screen";
}

void SystemBridge::launchTaskManager() {
  QProcess::startDetached("taskmgr.exe");
  qDebug() << "[SystemBridge] Launching Task Manager";
}

void SystemBridge::launchCalculator() {
  QProcess::startDetached("calc.exe");
  qDebug() << "[SystemBridge] Launching Calculator";
}

void SystemBridge::launchSnippingTool() {
  // Try new Snip & Sketch first
  if (!QProcess::startDetached("SnippingTool.exe")) {
    // Fallback to old Snipping Tool
    QProcess::startDetached("snippingtool.exe");
  }
  qDebug() << "[SystemBridge] Launching Snipping Tool";
}

void SystemBridge::openSettings(const QString &page) {
  QString settingsUri = "ms-settings:";
  if (!page.isEmpty()) {
    settingsUri += page;
  }
  QProcess::startDetached("explorer.exe", QStringList() << settingsUri);
  qDebug() << "[SystemBridge] Opening settings:" << settingsUri;
}

void SystemBridge::launchProcess(const QString &executable,
                                 const QStringList &args) {
  QProcess::startDetached(executable, args);
  qDebug() << "[SystemBridge] Launching:" << executable << args;
}

QString SystemBridge::saveClipboardImage(const QString &saveDir) {
  QClipboard *clipboard = QGuiApplication::clipboard();
  const QMimeData *mime = clipboard->mimeData();

  if (mime->hasImage()) {
    QImage svImg = qvariant_cast<QImage>(mime->imageData());
    if (!svImg.isNull()) {
      QString localPath = saveDir;
      if (localPath.startsWith("file:")) {
        localPath = QUrl(localPath).toLocalFile();
      }

      QDir dir(localPath);
      if (!dir.exists()) {
        dir.mkpath(".");
      }
      QString fileName =
          "paste_" +
          QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") + ".png";
      QString fullPath = dir.filePath(fileName);
      if (svImg.save(fullPath)) {
        // Return file URL format
        return QUrl::fromLocalFile(fullPath).toString();
      }
    }
  }
  return "";
}

void SystemBridge::updateStartWithWindowsState() {
  QSettings bootSettings(
      "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
      QSettings::NativeFormat);
  bool enabled = bootSettings.contains("EdgeGesture");
  if (m_startWithWindows != enabled) {
    m_startWithWindows = enabled;
    emit startWithWindowsChanged();
  }
}

void SystemBridge::setStartWithWindows(bool enable) {
  if (m_startWithWindows != enable) {
    QSettings bootSettings(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        QSettings::NativeFormat);

    // Register the current application (SettingsUI.exe) to start with Windows
    // It will be responsible for starting GestureEngine.exe
    QString appPath =
        QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    if (enable) {
      bootSettings.setValue("EdgeGesture", "\"" + appPath + "\"");
    } else {
      bootSettings.remove("EdgeGesture");
    }

    m_startWithWindows = enable;
    emit startWithWindowsChanged();
    qDebug() << "[SystemBridge] Start with Windows set to:" << enable;
  }
}
