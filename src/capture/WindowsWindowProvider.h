#pragma once

#include "CaptureGeometry.h"

#include <QRect>
#include <QVector>
#include <QtGlobal>

QRect overlayLocalWindowRect(const QRect &physicalWindowRect,
                             const QVector<CaptureMonitorGeometry> &monitors,
                             const QRect &virtualDesktopRect);

// handles (optional) receives each returned window's HWND, at the same index.
QVector<QRect> windowsForCaptureOverlay(quintptr overlayWindowId,
                                        const QVector<CaptureMonitorGeometry> &monitors,
                                        const QRect &virtualDesktopRect,
                                        QVector<quintptr> *handles = nullptr);

// The visible controls (child windows) of each window in handles, at the same
// index, in overlay coordinates and clipped to their window. Only programs
// built from real child windows have any; most modern apps draw their own.
QVector<QVector<QRect>> controlsForCaptureWindows(const QVector<quintptr> &handles,
                                                  const QVector<QRect> &windowRects,
                                                  const QVector<CaptureMonitorGeometry> &monitors,
                                                  const QRect &virtualDesktopRect);
