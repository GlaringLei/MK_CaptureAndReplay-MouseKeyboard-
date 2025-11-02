#pragma once
#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFileDialog>

#include "replaymanager.h"
#include "globalhotkeymanager.h"

class ReplayControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ReplayControlWidget(QWidget *parent = nullptr);
    ~ReplayControlWidget();

signals:
    void configChanged();  // 通知ReplayManager更新配置

public slots:
    void onSelectFile();
    void onClearSelectFile();
    void onStartReplay();
    void onReplayProgress(int current, int total);
    void onReplayStateChanged(QString state);
    void onHotkeyCommand(GlobalHotkeyManager::HotkeyAction action);

private:
    void setupUi();
    void loadConfigToManager();

    // 控件
    QLabel *fileLabel;
    QLabel *fileClearLabel;
    QLabel *statusLabel;
    QProgressBar *progressBar;
    QCheckBox *mouseCheck;
    QCheckBox *keyboardCheck;
    QComboBox *speedBox;
    QPushButton *startButton;

    QString replayFilePath;
};
