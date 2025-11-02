#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QDebug>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      capturing_(false),
      replayControlWidget_(nullptr),
      hotkeyManager_(nullptr)
{
    ui->setupUi(this);
    ui->statusLabel->setText("status: off");
    // 初始化全局热键管理器
    hotkeyManager_ = &GlobalHotkeyManager::instance();

    // 初始化回放控制面板
    replayControlWidget_ = new ReplayControlWidget(this);
    replayControlWidget_->setMinimumHeight(200);

    // 主体布局：原UI + 控制面板
    QWidget *container = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->addWidget(ui->centralwidget);
    mainLayout->addWidget(replayControlWidget_);
    setCentralWidget(container);

    setupMenus();
    setupConnections();

    // 注册信号槽中使用的自定义类型
    qRegisterMetaType<MouseEventData>("MouseEventData");
    qRegisterMetaType<KeyEventData>("KeyEventData");

    // 捕获引擎信号连接
    CaptureEngine &engine = CaptureEngine::instance();
    connect(&engine, &CaptureEngine::mouseEventCaptured,
            this, &MainWindow::handleMouseEvent, Qt::QueuedConnection);
    connect(&engine, &CaptureEngine::keyEventCaptured,
            this, &MainWindow::handleKeyEvent, Qt::QueuedConnection);

    // 捕获数据传给录制模块
    connect(&engine, &CaptureEngine::mouseEventCaptured,
            &Recorder::instance(), &Recorder::onMouseEventCaptured, Qt::QueuedConnection);
    connect(&engine, &CaptureEngine::keyEventCaptured,
            &Recorder::instance(), &Recorder::onKeyEventCaptured, Qt::QueuedConnection);

    // 默认注册热键（可被 HotkeyConfigDialog 覆盖）
    hotkeyManager_->registerHotkey(GlobalHotkeyManager::StopReplay,   QKeySequence("Ctrl+Alt+S"));
    hotkeyManager_->registerHotkey(GlobalHotkeyManager::PauseReplay,  QKeySequence("Ctrl+Alt+P"));
    hotkeyManager_->registerHotkey(GlobalHotkeyManager::ResumeReplay, QKeySequence("Ctrl+Alt+R"));
    hotkeyManager_->registerHotkey(GlobalHotkeyManager::SpeedUpReplay,QKeySequence("Ctrl+Alt+F"));

//    GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::StopReplay, QKeySequence("Ctrl+Shift+E"));
//    GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::PauseReplay, QKeySequence("Ctrl+Shift+S"));
//    GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::ResumeReplay, QKeySequence("Ctrl+Shift+C"));
//    GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::SpeedUpReplay, QKeySequence("Ctrl+Shift+A"));
}

MainWindow::~MainWindow()
{
    delete ui;
    CaptureEngine::instance().stop();
}

void MainWindow::setupMenus()
{
    QMenu *menu = menuBar()->addMenu(tr("设置"));
    QAction *hotkeyAction = menu->addAction(tr("热键配置..."));
    connect(hotkeyAction, &QAction::triggered, this, &MainWindow::onOpenHotkeyConfig);
}

void MainWindow::setupConnections()
{
    // 全局热键触发 → 控制回放逻辑
    connect(hotkeyManager_, &GlobalHotkeyManager::hotkeyPressed,
            replayControlWidget_, &ReplayControlWidget::onHotkeyCommand);
}

void MainWindow::on_startButton_clicked()
{
    if (!capturing_) {
        QString filePath = QFileDialog::getSaveFileName(this, "选择录制文件", "", "JSON Files (*.json)");
        if (filePath.isEmpty()) return;

        Recorder::instance().startRecording(filePath);

        if (CaptureEngine::instance().start()) {
            capturing_ = true;
            ui->statusLabel->setText("status: capture...");
        }
    }
}

void MainWindow::on_stopButton_clicked()
{
    if (capturing_) {
        CaptureEngine::instance().stop();
        Recorder::instance().stopRecording();
        capturing_ = false;
        ui->statusLabel->setText("status: off");
    }
}

void MainWindow::on_replayButton_clicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("选择操作记录文件"),
        lastReplayPath_.isEmpty() ? QDir::currentPath() : lastReplayPath_,
        tr("Operation Record (*.json);;All files (*.*)")
    );
    if (path.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("未选择文件"));
        return;
    }
    lastReplayPath_ = QFileInfo(path).absolutePath();

    auto &replay = ReplayManager::instance();
    if (replay.loadReplayFile(path)) {
        connect(&replay, &ReplayManager::replayProgress, this,
                [](int cur, int total) { qDebug() << "replay progress:" << cur << "/" << total; });
        connect(&replay, &ReplayManager::replayFinished, this,
                []() { QMessageBox::information(nullptr, "prompt", "replay finish"); });

        replay.startReplay();
        QMessageBox::information(this, tr("prompt"), tr("replay start"));
    } else {
        QMessageBox::warning(this, tr("error"), tr("File failed to load"));
    }
}

void MainWindow::handleMouseEvent(const MouseEventData &e)
{
    QString msg = QString("[%1] 鼠标事件: (%2,%3) Type=%4")
                      .arg(e.time.toString("hh:mm:ss.zzz"))
                      .arg(e.pos.x())
                      .arg(e.pos.y())
                      .arg(e.type);
    ui->logTextEdit->append(msg);
}

void MainWindow::handleKeyEvent(const KeyEventData &e)
{
    QString msg = QString("[%1] 键盘事件: %2 KeyCode=%3")
                      .arg(e.time.toString("hh:mm:ss.zzz"))
                      .arg(e.keyDown ? "按下" : "释放")
                      .arg(e.vkCode);
    ui->logTextEdit->append(msg);
}

// 打开热键配置对话框
void MainWindow::onOpenHotkeyConfig()
{
    HotkeyConfigDialog dlg(this);
    dlg.exec();

    // 重新加载用户自定义的热键设置
    QSettings settings;
    auto load = [&](GlobalHotkeyManager::HotkeyAction action, const QString &key) {
        QString seq = settings.value(key).toString();
        if (!seq.isEmpty())
            hotkeyManager_->registerHotkey(action, QKeySequence(seq));
    };

    load(GlobalHotkeyManager::StopReplay,   "hotkeys/stop");
    load(GlobalHotkeyManager::PauseReplay,  "hotkeys/pause");
    load(GlobalHotkeyManager::ResumeReplay, "hotkeys/resume");
    load(GlobalHotkeyManager::SpeedUpReplay,"hotkeys/speed");
}


//#include "mainwindow.h"
//#include "ui_mainwindow.h"
//#include "globalhotkeymanager.h"
//#include <QDebug>
//#include <QDateTime>
//#include <QFileDialog>
//#include <QMessageBox>

//MainWindow::MainWindow(QWidget *parent)
//    : QMainWindow(parent),
//      ui(new Ui::MainWindow),
//      capturing_(false)
//{
//    ui->setupUi(this);
//    ui->statusLabel->setText("status: off");

//    // 初始化回放控制面板
//    replayControlWidget_ = new ReplayControlWidget(this);
//    replayControlWidget_->setMinimumHeight(180);

//    // 将其嵌入主窗口布局中
//    QWidget *container = new QWidget(this);
//    QVBoxLayout *layout = new QVBoxLayout(container);
//    layout->addWidget(ui->centralwidget);         // 原有UI（录制相关）
//    layout->addWidget(replayControlWidget_);      // 回放控制模块
//    setCentralWidget(container);

//    // 菜单栏添加 "设置 -> 热键配置"
//    QMenu *menu = menuBar()->addMenu(tr("设置"));
//    QAction *hotkeyAction = menu->addAction(tr("全局热键配置..."));
//    connect(hotkeyAction, &QAction::triggered, this, &MainWindow::on_openHotkeyConfig);

//    // 信号连接：全局热键触发 → 控制 ReplayManager
//    connect(hotkeyManager_, &GlobalHotkeyManager::hotkeyTriggered,
//            replayControlWidget_, &ReplayControlWidget::onHotkeyCommand);

//    // 注册全局热键，以控制 操作回放 线程
//    GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::StopReplay, QKeySequence("Ctrl+Shift+E"));
//    GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::PauseReplay, QKeySequence("Ctrl+Shift+S"));
//    GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::ResumeReplay, QKeySequence("Ctrl+Shift+C"));
//    GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::SpeedUpReplay, QKeySequence("Ctrl+Shift+A"));

//    // GlobalHotkeyManager::instance().registerHotkeys();

////    GlobalHotkeyManager *hotkeyManager = new GlobalHotkeyManager();

////    connect(hotkeyManager, &GlobalHotkeyManager::stopTriggered, [](){
////        qDebug() << "[] end replay";
////    });

////    connect(hotkeyManager, &GlobalHotkeyManager::pauseTriggered, [](){
////        qDebug() << "[] stop replay";
////    });

////    connect(hotkeyManager, &GlobalHotkeyManager::continueTriggered, [](){
////        qDebug() << "[] continue replay";
////    });

////    connect(hotkeyManager, &GlobalHotkeyManager::speedUpTriggered, [](){
////        qDebug() << "[] acc replay";
////    });


//    // 注册信号槽中使用的自定义类型(因为 Qt 的信号槽机制在跨线程传递 MouseEventData 等结构时无法识别其类型)
//    qRegisterMetaType<MouseEventData>("MouseEventData");
//    qRegisterMetaType<KeyEventData>("KeyEventData");

//    // 绑定 CaptureEngine 信号
//    // 本质上是从队列中只要存储了捕获的事件，则会出发相应的信号，将来自CaptureEngine的信号与主进程MainWindow中的处理事件函数绑定，来更新UI
//    CaptureEngine& engine = CaptureEngine::instance();
//    connect(&engine, &CaptureEngine::mouseEventCaptured,
//            this, &MainWindow::handleMouseEvent, Qt::QueuedConnection);
//    connect(&engine, &CaptureEngine::keyEventCaptured,
//            this, &MainWindow::handleKeyEvent, Qt::QueuedConnection);

//    connect(&CaptureEngine::instance(), &CaptureEngine::mouseEventCaptured,
//            &Recorder::instance(), &Recorder::onMouseEventCaptured, Qt::QueuedConnection);

//    connect(&CaptureEngine::instance(), &CaptureEngine::keyEventCaptured,
//            &Recorder::instance(), &Recorder::onKeyEventCaptured, Qt::QueuedConnection);

//}

//MainWindow::~MainWindow()
//{
//    delete ui;
//    CaptureEngine::instance().stop();

//}

//void MainWindow::on_startButton_clicked()
//{
////    捕获信号连接到录制槽
////    auto& engine = CaptureEngine::instance();
////    connect(&engine, &CaptureEngine::mouseEventCaptured,
////            &Recorder::instance(), &Recorder::onMouseEventCaptured, Qt::QueuedConnection);

////    connect(&engine, &CaptureEngine::keyEventCaptured,
////            &Recorder::instance(), &Recorder::onKeyEventCaptured, Qt::QueuedConnection);

//    if (!capturing_) {
//        QString filePath = QFileDialog::getSaveFileName(this, "选择录制文件", "", "JSON Files (*.json)");
//        if (filePath.isEmpty()) return;

//        // 启动录制
//        Recorder::instance().startRecording(filePath);

//        if (CaptureEngine::instance().start()) {
//            capturing_ = true;
//            ui->statusLabel->setText("status: capture...");
//        }
//    }
//}

//void MainWindow::on_stopButton_clicked()
//{
//    if (capturing_) {
//        CaptureEngine::instance().stop();
//        Recorder::instance().stopRecording();
//        capturing_ = false;
//        ui->statusLabel->setText("status: off");
//    }
//}

//// 负责触发操作记录文件的选择、加载和回放启动流程，
//// 核心作用是将用户交互（选择文件）与回放系统（ReplayManager）连接起来，实现从文件选择到开始回放的完整交互链路
//void MainWindow::on_replayButton_clicked()
//{
//    // 弹出文件选择对话框，获取用户选择的文件路径
//    QString path = QFileDialog::getOpenFileName(
//        this,
//        tr("选择操作记录文件"),
//        lastReplayPath_.isEmpty() ? QDir::currentPath() : lastReplayPath_,      // 默认路径，可改为上次录制路径
//        tr("Operations record files (*.json);;All files (*.*)")
//    );
//    // 记录上次选择的路径（优化下次交互）
//    if (!path.isEmpty())
//        lastReplayPath_ = QFileInfo(path).absolutePath();

//    // 处理用户未选择文件的情况
//    if (path.isEmpty()) {
//        QMessageBox::information(this, tr("Prompt"), tr("No file selected."));
//        return;
//    }

//    // 如果当前路径文件不空，加载选择的回放文件并启动回放
//    // 获取回放管理器单例
//    auto& replay = ReplayManager::instance();

//    // 尝试加载文件，成功则返回true
//    if (replay.loadReplayFile(path)) {
//        // 绑定回放进度信号：收到进度时输出日志
//        connect(&replay, &ReplayManager::replayProgress, this, [](int cur, int total) {
//            qDebug() << "回放进度:" << cur << "/" << total;
//        });

//        // 绑定回放结束信号：回放完成后弹出提示框
//        connect(&replay, &ReplayManager::replayFinished, this, []() {
//            QMessageBox::information(nullptr, "Finish", "relpay finish!");
//        });

//        // 启动回放
//        replay.startReplay();
//        // 提示回放已启动
//        QMessageBox::information(this, tr("Prompt"), tr("relpay start..."));
//    }
//    else {
//        // 加载失败（如文件损坏、格式错误），弹出警告
//        QMessageBox::warning(this, tr("Error"), tr("Unable to load the selected file."));
//    }
//}

//void MainWindow::on_openHotkeyConfig()
//{
//    HotkeyConfigDialog dlg(hotkeyManager_, this);
//    dlg.exec();
//}


//void MainWindow::handleMouseEvent(const MouseEventData &e)
//{
//    QString msg = QString("[%1] 鼠标事件: (%2,%3) Type=%4")
//            .arg(e.time.toString("hh:mm:ss.zzz"))
//            .arg(e.pos.x()).arg(e.pos.y()).arg(e.type);
//    ui->logTextEdit->append(msg);
//}

//void MainWindow::handleKeyEvent(const KeyEventData &e)
//{
//    QString msg = QString("[%1] 键盘事件: %2 KeyCode=%3")
//            .arg(e.time.toString("hh:mm:ss.zzz"))
//            .arg(e.keyDown ? "按下" : "释放")
//            .arg(e.vkCode);
//    ui->logTextEdit->append(msg);
//}


//---------------------------------------------------------------------------------


//#include "mainwindow.h"
//#include "ui_mainwindow.h"
//#include <QDebug>

//// 创建并初始化由 Qt Designer 设计的界面
//MainWindow::MainWindow(QWidget *parent)
//    : QMainWindow(parent),
//      ui(new Ui::MainWindow)
//{
//    ui->setupUi(this);
//    // 把“开始”按钮的点击信号与自己的槽 onStartClicked 绑定
//    connect(ui->btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked);
//    // 把“停止”按钮的点击信号与自己的槽 onStopClicked 绑定。
//    connect(ui->btnStop, &QPushButton::clicked, this, &MainWindow::onStopClicked);
//    // 一旦 m_engine 捕获到输入事件，就会发 eventCaptured(const InputEvent &) 信号，主窗口立即收到并调用 onEventCaptured 显示
//    connect(&m_engine, &CaptureEngine::eventCaptured, this, &MainWindow::onEventCaptured);
//}

//MainWindow::~MainWindow() {
//    delete ui;
//}

//void MainWindow::onStartClicked() {
//    // 调用 m_engine.start() 尝试启动底层捕获，只有返回 true 才在状态栏提示“Monitoring started.”。
//    if (m_engine.start())
//        ui->statusbar->showMessage("Monitoring started.");
//}

//void MainWindow::onStopClicked() {
//    // 通知引擎停止捕获，并在状态栏提示“Monitoring stopped.”。
//    m_engine.stop();
//    ui->statusbar->showMessage("Monitoring stopped.");
//}

//void MainWindow::onEventCaptured(const InputEvent &event) {
//    // 每收到一个事件就格式化成一行
//    QString info = QString("[%1] %2 (%3)")
//            .arg(event.timestamp)
//            .arg(event.keyName.isEmpty() ? event.extraInfo : event.keyName)
//            .arg(event.position.isNull() ? "" : QString("x=%1,y=%2").arg(event.position.x()).arg(event.position.y()));
//    ui->textEdit->append(info);
//}
