#include "replaycontrolwidget.h"
#include <QDebug>

ReplayControlWidget::ReplayControlWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();

    // 文件选择
    connect(fileLabel, &QLabel::linkActivated, this, &ReplayControlWidget::onSelectFile);
    connect(fileClearLabel,&QLabel::linkActivated, this, &ReplayControlWidget::onClearSelectFile);
    connect(startButton, &QPushButton::clicked, this, &ReplayControlWidget::onStartReplay);

    // 连接ReplayManager信号
    auto &replay = ReplayManager::instance();
    connect(&replay, &ReplayManager::replayProgress, this, &ReplayControlWidget::onReplayProgress);
    connect(&replay, &ReplayManager::stateChanged, this, &ReplayControlWidget::onReplayStateChanged);

    // 连接热键控制
    auto &hotkeys = GlobalHotkeyManager::instance();
    connect(&hotkeys, &GlobalHotkeyManager::hotkeyPressed, this, &ReplayControlWidget::onHotkeyCommand);
}

ReplayControlWidget::~ReplayControlWidget() {}

void ReplayControlWidget::setupUi()
{
    // 主布局
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 15, 20, 15);  // 左右保持20像素空隙，上下15像素
    layout->setSpacing(10);

    QLabel *title = new QLabel("<b>操作回放控制面板</b>");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // 文件选择标签
    fileLabel = new QLabel("<a href='#'>选择回放文件</a>");
    fileLabel->setTextFormat(Qt::RichText);
    fileLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    fileLabel->setOpenExternalLinks(false);
    //fileLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(fileLabel);

    // 文件选择清空标签
    fileClearLabel = new QLabel("<a href='#'>clear</a>");
    fileClearLabel->setTextFormat(Qt::RichText);
    fileClearLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    fileClearLabel->setOpenExternalLinks(false);
    //fileLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(fileClearLabel);

    // 配置选项
    mouseCheck = new QCheckBox("回放鼠标事件");
    mouseCheck->setChecked(true);
    keyboardCheck = new QCheckBox("回放键盘事件");
    keyboardCheck->setChecked(true);

    QHBoxLayout *checkLayout = new QHBoxLayout();
    checkLayout->addWidget(mouseCheck);
    checkLayout->addWidget(keyboardCheck);
    checkLayout->addStretch();
    layout->addLayout(checkLayout);

    // 回放速度
    QLabel *speedLabel = new QLabel("replay speed");
    speedBox = new QComboBox();
    speedBox->addItems({"0.5x", "1x", "2x", "4x"});
    speedBox->setCurrentIndex(1);

    QHBoxLayout *speedLayout = new QHBoxLayout();
    speedLayout->addWidget(speedLabel);
    speedLayout->addWidget(speedBox);
    speedLayout->addStretch();
    layout->addLayout(speedLayout);

    // 启动按钮
    startButton = new QPushButton("启动回放");
    startButton->setMinimumHeight(32);
    startButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(startButton);

    // 进度条（自适应宽度）
    progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setTextVisible(true);
    progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    progressBar->setMinimumHeight(20);
    progressBar->setStyleSheet(
        "QProgressBar { "
        "  border: 1px solid #aaa; "
        "  border-radius: 5px; "
        "  text-align: center; "
        "  padding: 1px; "
        "}"
        "QProgressBar::chunk { "
        "  background-color: #5cb85c; "
        "  width: 20px; "
        "  margin: 0.5px; "
        "}"
    );
    layout->addWidget(progressBar);

    // 状态显示
    statusLabel = new QLabel("状态：就绪");
    //statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(statusLabel);

    setLayout(layout);
}

void ReplayControlWidget::onSelectFile()
{
    QString file = QFileDialog::getOpenFileName(this, "选择回放数据文件", "", "JSON Files (*.json)");
    if (!file.isEmpty()) {
        replayFilePath = file;
        fileLabel->setText(file);
    }
}

void ReplayControlWidget::onClearSelectFile()
{
    if(replayFilePath.isEmpty()){
       return;
    }
    replayFilePath.clear();
    fileLabel->setText("<a href='#'>选择回放文件</a>");
}

void ReplayControlWidget::onStartReplay()
{
    if (replayFilePath.isEmpty()) {
        statusLabel->setText("请先选择文件");
        return;
    }

    loadConfigToManager();
    auto &replay = ReplayManager::instance();

    if (replay.loadReplayFile(replayFilePath)) {
        replay.startReplay();
        statusLabel->setText("状态：正在回放");
    } else {
        statusLabel->setText("文件加载失败");
    }
}

void ReplayControlWidget::loadConfigToManager()
{
    auto &replay = ReplayManager::instance();
    replay.setReplayMouse(mouseCheck->isChecked());
    replay.setReplayKeyboard(keyboardCheck->isChecked());

    QString speedStr = speedBox->currentText();
    double speed = 1.0;
    if (speedStr == "0.5x") speed = 0.5;
    else if (speedStr == "2x") speed = 2.0;
    else if (speedStr == "4x") speed = 4.0;
    replay.setSpeedMultiplier(speed);

    emit configChanged();
}

void ReplayControlWidget::onReplayProgress(int current, int total)
{
    if (total > 0)
        progressBar->setValue((current * 100) / total);
}

void ReplayControlWidget::onReplayStateChanged(QString state)
{
    statusLabel->setText("状态：" + state);
}

void ReplayControlWidget::onHotkeyCommand(GlobalHotkeyManager::HotkeyAction action)
{
    auto &replay = ReplayManager::instance();

    switch (action) {
    case GlobalHotkeyManager::StopReplay:
        replay.stopReplay();
        statusLabel->setText("Status: Terminated");
        break;
    case GlobalHotkeyManager::PauseReplay:
        replay.pauseReplay();
        statusLabel->setText("Status: Paused");
        break;
    case GlobalHotkeyManager::ResumeReplay:
        replay.resumeReplay();
        statusLabel->setText("Status: Continuing");
        break;
    case GlobalHotkeyManager::SpeedUpReplay:
        replay.setSpeedMultiplier(2.0);
        statusLabel->setText("Status: Double Speed x2");
        break;
    }
}
