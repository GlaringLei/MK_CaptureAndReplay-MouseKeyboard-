#pragma once
#include <QObject>
#include <QThread>
#include <QJsonArray>
#include <QString>
#include <QEventLoop>
#include <QTimer>

class ReplayWorker;

class ReplayManager : public QObject
{
    Q_OBJECT
public:
    static ReplayManager& instance();

    bool loadReplayFile(const QString &path); // load json and store events
    bool startReplay();    // create worker/thread and start
    void stopReplay();
    void pauseReplay();
    void resumeReplay();
    void setReplayMouse(bool en);
    void setReplayKeyboard(bool en);
    void setSpeedMultiplier(double f);
    bool isReplaying() const { return m_replaying; }

signals:
    void replayProgress(int current, int total);
    void stateChanged(const QString &state);
    void replayFinished();

private slots:
    void onWorkerFinished();
    void onWorkerProgress(int cur, int total);
    void onWorkerStateChanged(const QString &s);

private:
    explicit ReplayManager(QObject* parent = nullptr);
    ~ReplayManager();

    QJsonArray m_events;
    QThread m_thread;
    ReplayWorker* m_worker = nullptr;

    bool m_replayMouse = true;
    bool m_replayKeyboard = true;
    double m_speed = 1.0;
    bool m_replaying = false;
};


//------------------------------------------------------------------------------------


//#pragma once
//#include <QObject>
//#include <QJsonObject>
//#include "replayworker.h"
//#include "hotkeycommand.h"

//class ReplayManager : public QObject {
//    Q_OBJECT
//public:
//    static ReplayManager& instance();

//    bool loadReplayFile(const QString& path);
//    void startReplay();
//    void stopReplay();
//    void handleHotkey(HotkeyCommand cmd);

//signals:
//    void replayProgress(int current, int total);
//    void replayFinished();

//private:
//    explicit ReplayManager(QObject* parent = nullptr);
//    ~ReplayManager();
//    QJsonObject replayData_;
//    ReplayWorker* worker_ = nullptr;
//};




//------------------------------------------------------------------------------------

//#ifndef REPLAYMANAGER_H
//#define REPLAYMANAGER_H

//#include <QObject>
//#include <QJsonArray>
//#include <QJsonObject>
//#include <QJsonDocument>
//#include <QFile>
//#include <QThread>
//#include <QMutex>
//#include <QWaitCondition>
//#include <QElapsedTimer>
//#include <atomic>

//class ReplayWorker : public QThread
//{
//    Q_OBJECT
//public:
//    explicit ReplayWorker(QObject* parent = nullptr);
//    ~ReplayWorker();

//    void setReplayData(const QJsonArray& events);
//    void stopReplay();   // 终止
//    void pauseReplay();  // 暂停
//    void resumeReplay(); // 继续
//    void setSpeedMultiplier(double m); // 倍速

//signals:
//    void replayProgress(int current, int total);
//    void replayFinished();

//protected:
//    void run() override;

//private:
//    void simulateEvent(const QJsonObject& e);

//    QJsonArray events_;
//    std::atomic<bool> running_{false};
//    std::atomic<bool> paused_{false};
//    std::atomic<double> speed_{1.0};
//    QMutex mutex_;
//    QWaitCondition cond_;
//};

//class ReplayManager : public QObject
//{
//    Q_OBJECT
//public:
//    static ReplayManager& instance();

//    bool loadReplayFile(const QString& path);
//    void startReplay();
//    void stopReplay();
//    void pauseReplay();
//    void resumeReplay();
//    void setSpeedMultiplier(double m);

//    // 快捷键检测
//    void handleGlobalHotkey(int vk, bool ctrl, bool shift);

//signals:
//    void replayProgress(int current, int total);
//    void replayFinished();

//private:
//    explicit ReplayManager(QObject* parent = nullptr);
//    ~ReplayManager();

//    ReplayWorker* worker_ = nullptr;
//    QJsonArray replayEvents_;
//    bool loaded_ = false;
//};

//#endif // REPLAYMANAGER_H



//------------------------------------------------------------------------------------



//#ifndef REPLAYMANAGER_H
//#define REPLAYMANAGER_H

//#include <QObject>
//#include <QFile>
//#include <QJsonArray>
//#include <QJsonObject>
//#include <QElapsedTimer>
//#include <QThread>
//#include <QMutex>
//#include <QWaitCondition>
//#include <QDebug>
//#include <windows.h>
//#include <atomic>

//class ReplayWorker : public QThread {
//    Q_OBJECT
//public:
//    explicit ReplayWorker(QObject* parent = nullptr);
//    ~ReplayWorker();

//    bool loadFromFile(const QString& path);
//    void stopWorker();

//signals:
//    void replayFinished();
//    void replayProgress(int current, int total);

//protected:
//    void run() override;

//private:
//    void executeEvent(const QJsonObject& evt);
//    QJsonArray events_;
//    bool running_ = false;
//    QMutex mutex_;
//    QWaitCondition cond_;
//};



//// 操作复现版本1的ReplayManager，无法通过热键实现过程中的控制
//class ReplayManager : public QObject {
//    Q_OBJECT
//public:
//    static ReplayManager& instance();

//    bool loadReplayFile(const QString& path);
//    void startReplay();
//    void stopReplay();
//    bool isReplaying() const { return replaying_; }

//signals:
//    void replayFinished();
//    void replayProgress(int current, int total);

//private:
//    explicit ReplayManager(QObject* parent = nullptr);
//    ~ReplayManager();

//private:
//    ReplayWorker* worker_ = nullptr;
//    QString currentFile_;
//    bool replaying_ = false;
//};

//#endif // REPLAYMANAGER_H

