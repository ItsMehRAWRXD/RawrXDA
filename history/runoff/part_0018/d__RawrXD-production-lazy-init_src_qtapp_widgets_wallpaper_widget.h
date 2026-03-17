/**
 * @file wallpaper_widget.h
 * @brief Header for WallpaperWidget - Wallpaper management
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QPushButton;
class QListWidget;
class QLabel;

class WallpaperWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit WallpaperWidget(QWidget* parent = nullptr);
    ~WallpaperWidget();
    
private slots:
    void onBrowseWallpapers();
    void onSetWallpaper();
    void onRandomWallpaper();
    
private:
    void setupUI();
    
    QVBoxLayout* mMainLayout;
    QPushButton* mBrowseButton;
    QPushButton* mSetButton;
    QPushButton* mRandomButton;
    QListWidget* mWallpaperList;
    QLabel* mPreviewLabel;
};

#endif // WALLPAPER_WIDGET_H
