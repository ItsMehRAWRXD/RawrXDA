#include <QWidget>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QTextEdit>

class NotebookWidget : public QWidget {
    Q_OBJECT
public:
    explicit NotebookWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        auto* layout = new QVBoxLayout(this);
        auto* tabs = new QTabWidget(this);
        tabs->addTab(new QTextEdit(this), tr("Page 1"));
        layout->addWidget(tabs);
        setLayout(layout);
    }
};

#include "notebook_widget.moc"
