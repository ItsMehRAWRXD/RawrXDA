#pragma once

#include <QObject>

class AutoUpdate : public QObject {
    Q_OBJECT
public:
    explicit AutoUpdate(QObject* parent = nullptr) : QObject(parent) {}
    bool checkAndInstall();
};
