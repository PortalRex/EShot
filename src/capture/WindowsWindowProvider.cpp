#include "WindowsWindowProvider.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

QRect overlayLocalWindowRect(const QRect &physicalWindowRect,
                             const QVector<CaptureMonitorGeometry> &monitors,
                             const QRect &virtualDesktopRect)
{
    if (!physicalWindowRect.isValid() || !virtualDesktopRect.isValid())
        return {};

    const QRect globalLogical = displayRectFromPhysical(physicalWindowRect, monitors);
    const QRect local = globalLogical.translated(-virtualDesktopRect.topLeft());
    return local.intersected(QRect(QPoint(0, 0), virtualDesktopRect.size()));
}

QVector<QRect> windowsForCaptureOverlay(quintptr overlayWindowId,
                                        const QVector<CaptureMonitorGeometry> &monitors,
                                        const QRect &virtualDesktopRect,
                                        QVector<quintptr> *handles)
{
#ifdef Q_OS_WIN
    struct EnumerationContext {
        HWND overlay = nullptr;
        DWORD processId = 0;
        const QVector<CaptureMonitorGeometry> *monitors = nullptr;
        const QRect *virtualDesktopRect = nullptr;
        QVector<QRect> windows;
        QVector<quintptr> handles;
    } context;

    context.overlay = reinterpret_cast<HWND>(overlayWindowId);
    context.processId = GetCurrentProcessId();
    context.monitors = &monitors;
    context.virtualDesktopRect = &virtualDesktopRect;

    EnumWindows([](HWND hwnd, LPARAM parameter) -> BOOL {
        auto *ctx = reinterpret_cast<EnumerationContext *>(parameter);
        if (!ctx || hwnd == ctx->overlay || !IsWindowVisible(hwnd) || IsIconic(hwnd))
            return TRUE;

        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);
        if (processId == ctx->processId)
            return TRUE;

        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED,
                                            &cloaked, sizeof(cloaked))) && cloaked != 0) {
            return TRUE;
        }

        const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if (((exStyle & WS_EX_TOOLWINDOW) && (exStyle & WS_EX_NOACTIVATE))
            || (exStyle & WS_EX_TRANSPARENT))
            return TRUE;

        wchar_t className[128] = {};
        GetClassNameW(hwnd, className, 128);
        if (lstrcmpW(className, L"Progman") == 0
            || lstrcmpW(className, L"WorkerW") == 0
            || lstrcmpW(className, L"Shell_TrayWnd") == 0
            || lstrcmpW(className, L"Shell_SecondaryTrayWnd") == 0) {
            return TRUE;
        }

        RECT nativeRect = {};
        HRESULT frameResult = DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                                    &nativeRect, sizeof(nativeRect));
        if (FAILED(frameResult) || nativeRect.right <= nativeRect.left
            || nativeRect.bottom <= nativeRect.top) {
            if (!GetWindowRect(hwnd, &nativeRect))
                return TRUE;
        }

        const QRect physical(nativeRect.left, nativeRect.top,
                             nativeRect.right - nativeRect.left,
                             nativeRect.bottom - nativeRect.top);
        if (physical.width() < 16 || physical.height() < 16)
            return TRUE;

        const QRect local = overlayLocalWindowRect(physical, *ctx->monitors,
                                                   *ctx->virtualDesktopRect);
        if (local.width() >= 16 && local.height() >= 16) {
            ctx->windows.append(local);
            ctx->handles.append(reinterpret_cast<quintptr>(hwnd));
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));

    if (handles)
        *handles = context.handles;
    return context.windows;
#else
    Q_UNUSED(overlayWindowId)
    Q_UNUSED(monitors)
    Q_UNUSED(virtualDesktopRect)
    if (handles)
        handles->clear();
    return {};
#endif
}

QVector<QVector<QRect>> controlsForCaptureWindows(const QVector<quintptr> &handles,
                                                  const QVector<QRect> &windowRects,
                                                  const QVector<CaptureMonitorGeometry> &monitors,
                                                  const QRect &virtualDesktopRect)
{
    QVector<QVector<QRect>> result(handles.size());
#ifdef Q_OS_WIN
    // Caps keep the overlay quick to open when a program has huge control trees.
    constexpr int kMaxControlsPerWindow = 1000;
    constexpr int kMaxControlsTotal = 5000;

    struct ChildContext {
        const QVector<CaptureMonitorGeometry> *monitors = nullptr;
        const QRect *virtualDesktopRect = nullptr;
        QRect window;
        QVector<QRect> controls;
        int budget = 0;
    };

    int remaining = kMaxControlsTotal;
    for (int i = 0; i < handles.size() && remaining > 0; ++i) {
        const HWND hwnd = reinterpret_cast<HWND>(handles[i]);
        if (!IsWindow(hwnd) || i >= windowRects.size())
            continue;

        ChildContext context;
        context.monitors = &monitors;
        context.virtualDesktopRect = &virtualDesktopRect;
        context.window = windowRects[i];
        context.budget = qMin(kMaxControlsPerWindow, remaining);

        // EnumChildWindows walks every descendant, not just direct children.
        EnumChildWindows(hwnd, [](HWND child, LPARAM parameter) -> BOOL {
            auto *ctx = reinterpret_cast<ChildContext *>(parameter);
            if (ctx->budget <= 0)
                return FALSE;
            // IsWindowVisible also checks every ancestor, so controls on
            // hidden tab pages are skipped.
            if (!IsWindowVisible(child))
                return TRUE;
            if (GetWindowLongPtrW(child, GWL_EXSTYLE) & WS_EX_TRANSPARENT)
                return TRUE;

            RECT nativeRect = {};
            if (!GetWindowRect(child, &nativeRect))
                return TRUE;
            const QRect physical(nativeRect.left, nativeRect.top,
                                 nativeRect.right - nativeRect.left,
                                 nativeRect.bottom - nativeRect.top);
            if (physical.width() < 8 || physical.height() < 8)
                return TRUE;

            const QRect local = overlayLocalWindowRect(physical, *ctx->monitors,
                                                       *ctx->virtualDesktopRect)
                                    .intersected(ctx->window);
            // A control filling the whole window adds nothing to snap to.
            if (local.width() >= 8 && local.height() >= 8 && local != ctx->window) {
                ctx->controls.append(local);
                --ctx->budget;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&context));

        remaining -= context.controls.size();
        result[i] = context.controls;
    }
#else
    Q_UNUSED(windowRects)
    Q_UNUSED(monitors)
    Q_UNUSED(virtualDesktopRect)
#endif
    return result;
}
