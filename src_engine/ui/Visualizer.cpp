#include "Visualizer.h"
#include <iostream>

Visualizer::Visualizer() {}

Visualizer::~Visualizer() {
  if (m_pWaveBrush)
    m_pWaveBrush->Release();
  if (m_pActiveBrush)
    m_pActiveBrush->Release();
  if (m_pArrowBrush)
    m_pArrowBrush->Release();
  if (m_pDCRT)
    m_pDCRT->Release();
  if (m_pD2DFactory)
    m_pD2DFactory->Release();
  if (m_hwnd)
    DestroyWindow(m_hwnd);
}

LRESULT CALLBACK Visualizer::WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                     LPARAM lParam) {
  if (message == WM_TIMER) {
    Visualizer *pThis = (Visualizer *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (pThis && pThis->m_onTimer) {
      pThis->m_onTimer();
    }
    return 0;
  } else if (message == WM_USER + 101) {
    ConfigManager::Get().Load();
    return 0;
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

bool Visualizer::Init(int screenW, int screenH) {
  m_screenHeight = screenH;

  WNDCLASSEXW wcex = {sizeof(WNDCLASSEX)};
  wcex.lpfnWndProc = WndProc;
  wcex.hInstance = GetModuleHandle(NULL);
  wcex.lpszClassName = L"OHOVisualizer";
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassExW(&wcex);

  m_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                               WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
                           L"OHOVisualizer", L"", WS_POPUP, 0, 0, m_width,
                           m_screenHeight, nullptr, nullptr,
                           GetModuleHandle(NULL), this);

  if (!m_hwnd)
    return false;

  SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

  HRESULT hr =
      D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
  if (FAILED(hr))
    return false;

  CreateResources();
  return true;
}

void Visualizer::CreateResources() {
  if (!m_pDCRT) {
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                          D2D1_ALPHA_MODE_PREMULTIPLIED));
    m_pD2DFactory->CreateDCRenderTarget(&props, &m_pDCRT);
  }
}

void Visualizer::Update(float currentX, float currentY, float anchorY,
                        bool isLeft, bool isTriggered,
                        const std::string &gestureIcon) {
  m_drawX = currentX;
  m_rawY = currentY;

  AppConfig &cfg = ConfigManager::Get().Current();
  int range = cfg.verticalRange;
  if (currentY < anchorY - range)
    m_drawY = anchorY - range;
  else if (currentY > anchorY + range)
    m_drawY = anchorY + range;
  else
    m_drawY = currentY;

  m_anchorY = anchorY;
  m_isLeft = isLeft;
  m_triggered = isTriggered;
  m_gestureIcon = gestureIcon;

  // Move window to correct side
  int screenW = GetSystemMetrics(SM_CXSCREEN);
  int xPos = isLeft ? 0 : (screenW - m_width);

  // We update window position only if needed, but here we just ensure it covers
  // the side strip
  SetWindowPos(m_hwnd, HWND_TOPMOST, xPos, 0, m_width, m_screenHeight,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void Visualizer::Render() {
  if (!m_pD2DFactory)
    return;

  HDC hScreenDC = GetDC(NULL);
  HDC hMemDC = CreateCompatibleDC(hScreenDC);

  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = m_width;
  bmi.bmiHeader.biHeight = m_screenHeight;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void *pBits = nullptr;
  HBITMAP hBitmap =
      CreateDIBSection(hMemDC, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
  HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

  CreateResources();
  RECT rc = {0, 0, m_width, m_screenHeight};
  m_pDCRT->BindDC(hMemDC, &rc);
  m_pDCRT->BeginDraw();
  m_pDCRT->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

  if (m_drawX > 2.0f) {
    // Color selection
    AppConfig &cfg = ConfigManager::Get().Current();
    std::string hex = m_isLeft ? cfg.left.color : cfg.right.color;
    COLORREF color = ConfigManager::Get().GetColorRef(hex);
    float r = GetRValue(color) / 255.0f;
    float g = GetGValue(color) / 255.0f;
    float b = GetBValue(color) / 255.0f;

    if (m_pWaveBrush)
      m_pWaveBrush->Release();
    if (m_pActiveBrush)
      m_pActiveBrush->Release();
    if (m_pArrowBrush)
      m_pArrowBrush->Release();

    m_pDCRT->CreateSolidColorBrush(D2D1::ColorF(r, g, b, 0.5f), &m_pWaveBrush);
    m_pDCRT->CreateSolidColorBrush(D2D1::ColorF(r, g, b, 0.8f),
                                   &m_pActiveBrush);
    m_pDCRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f),
                                   &m_pArrowBrush);

    DrawWave();
    DrawArrow();
  }

  m_pDCRT->EndDraw();

  POINT ptSrc = {0, 0};
  SIZE sizeWnd = {m_width, m_screenHeight};
  POINT ptDst = {0, 0};
  if (!m_isLeft) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    ptDst.x = screenW - m_width;
  }

  BLENDFUNCTION blend = {0};
  blend.BlendOp = AC_SRC_OVER;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat = AC_SRC_ALPHA;

  UpdateLayeredWindow(m_hwnd, hScreenDC, &ptDst, &sizeWnd, hMemDC, &ptSrc, 0,
                      &blend, ULW_ALPHA);

  SelectObject(hMemDC, hOldBitmap);
  DeleteObject(hBitmap);
  DeleteDC(hMemDC);
  ReleaseDC(NULL, hScreenDC);
}

void Visualizer::DrawWave() {
  ID2D1SolidColorBrush *currentBrush =
      m_triggered ? m_pActiveBrush : m_pWaveBrush;

  float waveHalfH = 140.0f;
  float baseTopY = m_anchorY - waveHalfH;
  float baseBottomY = m_anchorY + waveHalfH;

  // Enhance curve shape with better tangents (vertical at edges)
  // Control point tension factors
  float edgeTension = waveHalfH * 0.5f;
  float peakTension = waveHalfH * 0.35f;

  float pullMax = m_drawX;

  ID2D1PathGeometry *pGeo = nullptr;
  m_pD2DFactory->CreatePathGeometry(&pGeo);
  ID2D1GeometrySink *pSink = nullptr;
  pGeo->Open(&pSink);

  if (m_isLeft) {
    pSink->BeginFigure(D2D1::Point2F(0, baseTopY), D2D1_FIGURE_BEGIN_FILLED);
    // Smooth C1/C2 continuous curve from edge (x=0) to peak (x=pullMax)
    pSink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(0, baseTopY +
                             edgeTension), // CP1: Vertical tangent from edge
        D2D1::Point2F(
            pullMax,
            m_drawY - peakTension), // CP2: Vertical tangent approach to peak
        D2D1::Point2F(pullMax, m_drawY))); // Peak

    pSink->AddBezier(
        D2D1::BezierSegment(D2D1::Point2F(pullMax, m_drawY + peakTension),
                            D2D1::Point2F(0, baseBottomY - edgeTension),
                            D2D1::Point2F(0, baseBottomY)));

    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
  } else {
    float startX = (float)m_width;
    float peakX = startX - pullMax;

    pSink->BeginFigure(D2D1::Point2F(startX, baseTopY),
                       D2D1_FIGURE_BEGIN_FILLED);
    // Mirrored logic for right side
    pSink->AddBezier(
        D2D1::BezierSegment(D2D1::Point2F(startX, baseTopY + edgeTension),
                            D2D1::Point2F(peakX, m_drawY - peakTension),
                            D2D1::Point2F(peakX, m_drawY)));

    pSink->AddBezier(
        D2D1::BezierSegment(D2D1::Point2F(peakX, m_drawY + peakTension),
                            D2D1::Point2F(startX, baseBottomY - edgeTension),
                            D2D1::Point2F(startX, baseBottomY)));

    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
  }

  pSink->Close();
  m_pDCRT->FillGeometry(pGeo, currentBrush);

  pSink->Release();
  pGeo->Release();
}

void Visualizer::DrawArrow() {
  if (!m_pArrowBrush || m_drawX < 20.0f)
    return;

  AppConfig &cfg = ConfigManager::Get().Current();

  float arrowSize = 12.0f;
  // Center of the wave peak
  float centerX = m_isLeft ? m_drawX * 0.6f : m_width - (m_drawX * 0.6f);
  float centerY = m_drawY;

  // Calculate rotation
  float dy = m_drawY - m_anchorY;
  float range = (float)cfg.verticalRange;
  float angle = 0.0f;
  if (range > 0.1f) {
    angle = (dy / range) * 45.0f;
    // Clamp angle to 45
    if (angle > 45.0f)
      angle = 45.0f;
    if (angle < -45.0f)
      angle = -45.0f;
  }

  // Rotate around center
  D2D1::Matrix3x2F oldTransform;
  m_pDCRT->GetTransform(&oldTransform);
  m_pDCRT->SetTransform(
      D2D1::Matrix3x2F::Rotation(angle, D2D1::Point2F(centerX, centerY)) *
      oldTransform);

  ID2D1PathGeometry *pGeo = nullptr;
  m_pD2DFactory->CreatePathGeometry(&pGeo);
  ID2D1GeometrySink *pSink = nullptr;
  pGeo->Open(&pSink);

  int arrowCount = (m_drawX >= cfg.longSwipeThreshold) ? 2 : 1;
  float offset = 8.0f;

  for (int i = 0; i < arrowCount; i++) {
    float shiftX = i * offset;
    // If multiple arrows, shift them to keep centered or build outwards?
    // Let's build outwards (to the right, away from edge)
    // < <
    pSink->BeginFigure(
        D2D1::Point2F(centerX + shiftX + arrowSize * 0.6f, centerY - arrowSize),
        D2D1_FIGURE_BEGIN_HOLLOW);
    pSink->AddLine(D2D1::Point2F(centerX + shiftX - arrowSize * 0.4f,
                                 centerY)); // Tip
    pSink->AddLine(D2D1::Point2F(centerX + shiftX + arrowSize * 0.6f,
                                 centerY + arrowSize));
    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
  }

  pSink->Close();

  // Draw arrow with stroke (Stroke width 3)
  m_pDCRT->DrawGeometry(pGeo, m_pArrowBrush, 3.0f);

  pSink->Release();
  pGeo->Release();

  m_pDCRT->SetTransform(oldTransform);
}

void Visualizer::SetWindowVisible(bool visible) {
  if (m_hwnd) {
    if (visible) {
      ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
      SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
      ShowWindow(m_hwnd, SW_HIDE);
    }
  }
}
