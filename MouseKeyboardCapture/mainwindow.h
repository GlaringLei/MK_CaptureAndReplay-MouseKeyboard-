#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#pragma once
#include <QMainWindow>
#include "replaycontrolwidget.h"
#include "hotkeyconfigdialog.h"
#include "globalhotkeymanager.h"
#include "captureengine.h"
#include "recorder.h"
#include "replaymanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_startButton_clicked();
    void on_stopButton_clicked();
    void on_replayButton_clicked();

    void handleMouseEvent(const MouseEventData &e);
    void handleKeyEvent(const KeyEventData &e);

    void onOpenHotkeyConfig();   // 打开热键配置窗口

private:
    void setupMenus();
    void setupConnections();

private:
    Ui::MainWindow *ui;
    bool capturing_;
    QString lastReplayPath_;

    ReplayControlWidget *replayControlWidget_;
    GlobalHotkeyManager *hotkeyManager_;
};




//#include <QMainWindow>
//#include "captureengine.h"
//#include "recorder.h"
//#include "replaymanager.h"
//#include "replaycontrolwidget.h"
//#include "hotkeyconfigdialog.h"

//QT_BEGIN_NAMESPACE
//namespace Ui { class MainWindow; }
//QT_END_NAMESPACE

///**
// * @brief 主窗口类，用于控制 CaptureEngine
// */
//class MainWindow : public QMainWindow
//{
//    Q_OBJECT

//public:
//    explicit MainWindow(QWidget *parent = nullptr);
//    ~MainWindow();

//private slots:
//    void on_startButton_clicked();
//    void on_stopButton_clicked();
//    void on_replayButton_clicked();
//    void MainWindow::on_openHotkeyConfig();

//    void handleMouseEvent(const MouseEventData& e);
//    void handleKeyEvent(const KeyEventData& e);

//private:
//    Ui::MainWindow *ui;
//    bool capturing_;
//    QString lastReplayPath_;

//    ReplayControlWidget *replayControlWidget_;
//    GlobalHotkeyManager *hotkeyManager_;

//};

#endif // MAINWINDOW_H



//#ifndef MAINWINDOW_H
//#define MAINWINDOW_H

//#pragma once
//#include <QMainWindow>
//#include "captureengine.h"

//QT_BEGIN_NAMESPACE
//namespace Ui { class MainWindow; }
//QT_END_NAMESPACE

//class MainWindow : public QMainWindow {
//    Q_OBJECT
//public:
//    explicit MainWindow(QWidget *parent = nullptr);
//    ~MainWindow();

//private slots:
//    void onStartClicked();
//    void onStopClicked();
//    void onEventCaptured(const InputEvent &event);

//private:
//    Ui::MainWindow *ui;
//    // 主窗口中的事件捕获引擎对象
//    CaptureEngine m_engine;
//};


////#include <QMainWindow>

////QT_BEGIN_NAMESPACE
////namespace Ui { class MainWindow; }
////QT_END_NAMESPACE

////class MainWindow : public QMainWindow
////{
////    Q_OBJECT

////public:
////    MainWindow(QWidget *parent = nullptr);
////    ~MainWindow();

////private:
////    Ui::MainWindow *ui;
////};
//#endif // MAINWINDOW_H
