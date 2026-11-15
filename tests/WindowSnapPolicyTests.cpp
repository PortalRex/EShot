#include <QtTest>

#include "capture/WindowSnapPolicy.h"
#include "capture/WindowsWindowProvider.h"

class WindowSnapPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void choosesFirstContainingWindowInZOrder()
    {
        const QVector<QRect> windows = {
            QRect(200, 150, 600, 450),
            QRect(100, 100, 1000, 700)
        };

        QCOMPARE(topmostWindowAt(windows, QPoint(300, 250), QRect(0, 0, 1920, 1080)),
                 windows.first());
    }

    void clipsPartiallyVisibleWindowsAndRejectsInvalidCandidates()
    {
        const QVector<QRect> windows = {
            QRect(),
            QRect(-80, 40, 300, 200)
        };

        QCOMPARE(topmostWindowAt(windows, QPoint(40, 80), QRect(0, 0, 1920, 1080)),
                 QRect(0, 40, 220, 200));
        QVERIFY(topmostWindowAt(windows, QPoint(500, 500), QRect(0, 0, 1920, 1080)).isEmpty());
    }

    void prefersSmallestControlInsideTopmostWindow()
    {
        const QRect overlay(0, 0, 1920, 1080);
        const QVector<QRect> windows = {
            QRect(100, 100, 800, 600),
            QRect(0, 0, 1920, 1080)
        };
        const QVector<QVector<QRect>> controls = {
            { QRect(100, 130, 800, 570), QRect(120, 150, 200, 40) },
            { QRect(1000, 100, 300, 300) }
        };

        // Inside both controls: the smaller one wins.
        QCOMPARE(windowOrControlAt(windows, controls, QPoint(150, 160), overlay),
                 QRect(120, 150, 200, 40));
        // Title bar: no control there, so the whole window.
        QCOMPARE(windowOrControlAt(windows, controls, QPoint(500, 110), overlay),
                 windows.first());
        // Controls of a window lower in z-order never leak through the top one.
        QCOMPARE(windowOrControlAt(windows, controls, QPoint(1100, 200), overlay),
                 QRect(1000, 100, 300, 300));
        // No controls list behaves like topmostWindowAt.
        QCOMPARE(windowOrControlAt(windows, {}, QPoint(150, 160), overlay),
                 windows.first());
        QVERIFY(windowOrControlAt(windows, controls, QPoint(2000, 10), overlay).isEmpty());
    }

    void distinguishesClickFromManualDrag()
    {
        QVERIFY(isWindowSnapClick(QPoint(100, 100), QPoint(103, 102), 10));
        QVERIFY(!isWindowSnapClick(QPoint(100, 100), QPoint(112, 100), 10));
    }

    void mapsNativeWindowBoundsToOverlayCoordinatesOnScaledMonitor()
    {
        const QVector<CaptureMonitorGeometry> monitors = {
            {QRect(-1536, 0, 1536, 864), QRect(-1920, 0, 1920, 1080), 1.25},
            {QRect(0, 0, 1920, 1080), QRect(0, 0, 1920, 1080), 1.0}
        };

        QCOMPARE(overlayLocalWindowRect(QRect(-1800, 100, 1000, 600), monitors,
                                        QRect(-1536, 0, 3456, 1080)),
                 QRect(96, 80, 800, 480));
    }

    void windowHighlightUsesFastCubicGeometryTransition()
    {
        QCOMPARE(windowSnapAnimationDurationMs(), 120);
        const QRect from(0, 0, 100, 100);
        const QRect to(100, 200, 300, 500);
        QCOMPARE(windowSnapTransitionRect(from, to, 0.0), from);
        QCOMPARE(windowSnapTransitionRect(from, to, 0.5),
                 QRect(88, 175, 275, 450));
        QCOMPARE(windowSnapTransitionRect(from, to, 1.0), to);
    }

    void freeRegionModeNeverTargetsWindows()
    {
        const QVector<QRect> windows = {QRect(0, 0, 1920, 1080)};

        QVERIFY(windowSnapTargetForMode(CaptureSelectionMode::FreeRegion, windows,
                                        QPoint(600, 400), QRect(0, 0, 1920, 1080)).isEmpty());
    }

    void windowModeTargetsTopmostWindow()
    {
        const QVector<QRect> windows = {
            QRect(300, 200, 800, 600),
            QRect(0, 0, 1920, 1080)
        };

        QCOMPARE(windowSnapTargetForMode(CaptureSelectionMode::Window, windows,
                                         QPoint(600, 400), QRect(0, 0, 1920, 1080)),
                 windows.first());
    }

    void onlyFreeRegionModeAllowsManualSelection()
    {
        QVERIFY(allowsManualSelection(CaptureSelectionMode::FreeRegion));
        QVERIFY(!allowsManualSelection(CaptureSelectionMode::Window));
    }
};

QTEST_APPLESS_MAIN(WindowSnapPolicyTests)
#include "WindowSnapPolicyTests.moc"
