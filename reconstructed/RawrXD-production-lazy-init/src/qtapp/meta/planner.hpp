#pragma once
#include <QObject>

class MetaPlanner : public QObject {
    Q_OBJECT
public:
    explicit MetaPlanner(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~MetaPlanner() = default;
};
