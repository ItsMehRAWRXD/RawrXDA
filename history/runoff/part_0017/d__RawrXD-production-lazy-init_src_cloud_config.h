#pragma once

#include <QString>
#include <QMap>
#include <QJsonObject>

// Cloud provider enum - shared across cloud integration components
enum CloudProviderType { AWS, AZURE, GCP, HYBRID };

struct CloudConfig {
    // Basic connection info
    QString accessKey;
    QString secretKey;
    QString region;
    QString endpoint;
    QString k8sNamespace;
    QString environment;
    bool useSSL = true;
    int timeoutSeconds = 30;
    
    // Extended fields for enterprise cloud integration
    CloudProviderType provider = AWS;
    QString projectName;
    QMap<QString, QString> credentials;
    QJsonObject settings;
};
