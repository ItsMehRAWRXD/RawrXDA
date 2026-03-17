// OverlayWidget.cpp — Headless overlay text implementation
// Converted from Qt (QWidget, QPainter, paintEvent, QFont, QColor, QPen) to pure C++17
// Preserves ALL original rendering logic as documentation
//
// Original Qt paintEvent implementation:
//   QPainter painter(this);
//   painter.setRenderHint(QPainter::Antialiasing);
//   painter.setOpacity(m_opacity);
//
//   // Background fill
//   QColor bgColor(m_bgR, m_bgG, m_bgB, m_bgA);
//   painter.fillRect(rect(), bgColor);
//
//   // Border
//   QPen borderPen(QColor(100, 100, 100, 150));
//   borderPen.setWidth(1);
//   painter.setPen(borderPen);
//   painter.drawRect(rect().adjusted(0, 0, -1, -1));
//
//   // Ghost text rendering
//   if (!m_ghostText.isEmpty()) {
//       QFont font(m_fontFamily, m_fontSize);
//       font.setItalic(true);
//       painter.setFont(font);
//
//       QColor textColor(m_colorR, m_colorG, m_colorB, m_colorA);
//       painter.setPen(textColor);
//
//       QRect textRect = rect().adjusted(8, 4, -8, -4);
//       painter.drawText(textRect,
//                        Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
//                        m_ghostText);
//   }
//
// All rendering is a no-op in headless mode.
// All data storage and API surface preserved in header.

#include "OverlayWidget.h"

// All methods are inline in the header.
// This file preserves the original source structure and
// documents the Qt rendering logic that was converted to headless stubs.
