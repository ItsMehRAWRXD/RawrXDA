#include <QApplication>
#include <QtTest>
#include "model_loader_widget.hpp"
#include <QDir>
#include <QFile>

class TestModelLoaderTooltip : public QObject {
private slots:
    void test_heuristic_tooltip_from_filename() {
        // Create a temporary working dir
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QDir::setCurrent(tmp.path());

        QDir d;
        QVERIFY(d.mkpath("models"));
        QString filePath = d.filePath("models/wizardLM-7B-q4_1.gguf");
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("\n");
        f.close();

        ModelLoaderWidget w;
        // The widget schedules an initial refresh on construction (singleShot), process events to allow it to run
        QTest::qWait(100);

        // Look up our file
        int idx = -1;
        for (int i = 0; i < w.modelSelector()->count(); ++i) {
            if (w.modelSelector()->itemData(i).toString() == filePath) {
                idx = i; break;
            }
        }
        QVERIFY(idx >= 0);
        QString tt = w.modelSelector()->itemData(idx, Qt::ToolTipRole).toString();
        QVERIFY(tt.contains("q4_1") || tt.contains("q4-1"));
        QVERIFY(tt.contains("7B") || tt.contains("7.0B") || tt.contains("7"));

        // Simulate an Ollama entry that resolves to the same GGUF
        QString ollamaKey = "ollama:wizardLM";
        w.modelSelector()->addItem("[Ollama] wizardLM", ollamaKey);
        w.ensureTooltipForModelData(ollamaKey, filePath);
        // Find Ollama item tooltip
        int oidx = -1;
        for (int i = 0; i < w.modelSelector()->count(); ++i) {
            if (w.modelSelector()->itemData(i).toString() == ollamaKey) {
                oidx = i; break;
            }
        }
        QVERIFY(oidx >= 0);
        QString ott = w.modelSelector()->itemData(oidx, Qt::ToolTipRole).toString();
        QVERIFY(ott.contains("q4_1") || ott.contains("7B"));

        // Programmatic load test: selecting the Ollama item should start a load when it resolves
        int selectIdx = -1;
        for (int i = 0; i < w.modelSelector()->count(); ++i) {
            if (w.modelSelector()->itemData(i).toString() == ollamaKey) { selectIdx = i; break; }
        }
        QVERIFY(selectIdx >= 0);
        // Trigger the selection by changing the combo box index (public API)
        w.modelSelector()->setCurrentIndex(selectIdx);
        // The widget should have started loading in the background (m_loading true)
        QTRY_VERIFY(w.isLoading());
    }

private:
    // Minimal helper to avoid relying on argv in tests
    int argc_zeros() { return 0; }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    TestModelLoaderTooltip tc;
    return QTest::qExec(&tc, argc, argv);
}
