// No third-party dependencies: Win32/MSVC C++20 smoke for qt_block.h
// cl /nologo /std:c++20 /EHsc /W4 tests\qt_block_runtime_smoke.cpp /Fe:qt_block_runtime_smoke.exe
#include "../include/qt_block.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

int main() {
    const QString org = "RawrXDQtRuntimeTest";
    const QString app = "QtBlockSmoke";
    {
        QSettings prefs(org, app);
        prefs.remove("integration");
        prefs.beginGroup("integration");
        prefs.setValue("count", QVariant(42));
        prefs.setValue("enabled", QVariant(true));
        prefs.setValue("title", QVariant(QString("native-NoQt")));
        prefs.setValue("precision", QVariant(1.25));
        prefs.setValue("binary", QVariant::fromValue(QByteArray("a\0b", 3)));
        assert(prefs.status() == QSettings::NoError);
        prefs.endGroup();
    }
    {
        QSettings prefs(org, app);
        prefs.beginGroup("integration");
        assert(prefs.value("count").toInt() == 42);
        assert(prefs.value("enabled").toBool());
        assert(prefs.value("title").toString() == "native-NoQt");
        assert(prefs.value("precision").toDouble() == 1.25);
        auto binary = prefs.value("binary").toByteArray();
        assert(binary.size() == 3 && binary[1] == '\0');
        prefs.endGroup();
        prefs.remove("integration");
        assert(!prefs.contains("integration/count"));
        assert(prefs.status() == QSettings::NoError);
    }
    std::atomic<int> ticks{0};
    {
        QTimer timer;
        timer.onTimeout([&ticks] { ++ticks; });
        timer.setSingleShot(true);
        timer.start(10);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (ticks.load() == 0 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        timer.stop();
        assert(ticks.load() == 1);
        assert(!timer.isActive());
    }
    // Cancel a long timer; destruction must not leave a worker capturing this.
    {
        QTimer timer;
        timer.onTimeout([&ticks] { ++ticks; });
        timer.start(60000);
        timer.stop();
        assert(!timer.isActive());
        assert(ticks.load() == 1);
    }
    return 0;
}
