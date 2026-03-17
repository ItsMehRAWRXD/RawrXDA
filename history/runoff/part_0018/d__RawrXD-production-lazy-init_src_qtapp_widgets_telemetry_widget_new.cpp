/**
 * @file telemetry_widget.cpp
 * @brief Implementation of TelemetryWidget - Telemetry display
 */

#include "telemetry_widget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>

TelemetryWidget::TelemetryWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setWindowTitle("Telemetry");
}

TelemetryWidget::~TelemetryWidget() = default;

void TelemetryWidget::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    mSessionTimeLabel = new QLabel("Session Time: 0h 0m", this);
    mMainLayout->addWidget(mSessionTimeLabel);
    
    mLinesEditedLabel = new QLabel("Lines Edited: 0", this);
    mMainLayout->addWidget(mLinesEditedLabel);
    
    mFilesOpenedLabel = new QLabel("Files Opened: 0", this);
    mMainLayout->addWidget(mFilesOpenedLabel);
    
    mSearchesLabel = new QLabel("Searches: 0", this);
    mMainLayout->addWidget(mSearchesLabel);
    
    mBuildCountLabel = new QLabel("Builds: 0", this);
    mMainLayout->addWidget(mBuildCountLabel);
    
    mMainLayout->addSpacing(20);
    
    mActivityBar = new QProgressBar(this);
    mActivityBar->setRange(0, 100);
    mActivityBar->setValue(45);
    mMainLayout->addWidget(new QLabel("Activity Level:", this));
    mMainLayout->addWidget(mActivityBar);
    
    mMainLayout->addStretch();
}

void TelemetryWidget::updateStats()
{
    // TODO: Update telemetry statistics from actual data
}
