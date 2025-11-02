#ifndef REPLAYWORKER_H
#define REPLAYWORKER_H

#pragma once
#include <QObject>
#include <QJsonArray>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>

/*
 * ReplayWorker（线程内执行的对象）
 * ---------------------------------------------------------
 * - 支持即停即止（stopReplay 立刻唤醒所有 wait/sleep）
 * - 支持暂停/继续
 * - 支持倍速播放
 * - 信号：
 *     replayProgress(int current, int total)
 *     stateChanged(QString)
 *     finished()
 */

class ReplayWorker : public QObject
{
    Q_OBJECT
public:
    explicit ReplayWorker(QObject *parent = nullptr);
    ~ReplayWorker();

    void setEvents(const QJsonArray &events);
    void setOptions(bool replayMouse, bool replayKeyboard);

public slots:
    void startReplay();     // 主循环（在工作线程执行）
    void stopReplay();      // 请求停止（即停即止）
    void pauseReplay();
    void resumeReplay();
    void setSpeedFactor(double f); // 倍速控制

signals:
    void replayProgress(int current, int total);
    void stateChanged(const QString &state);
    void finished();

private:
    void runLoop();
    void simulateEvent(const QJsonObject &evt);

private:
    QJsonArray m_events;
    bool m_replayMouse = true;
    bool m_replayKeyboard = true;

    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_paused{false};
    std::atomic<double> m_speed{1.0};

    QMutex m_pauseMutex;
    QWaitCondition m_pauseCond;

    QMutex m_waitMutex;        // 用于 sleep 的中断等待
    QWaitCondition m_waitCond; // 可被 stopReplay() 唤醒
};

#endif // REPLAYWORKER_H


//--------------------------------------------------------------------

//#ifndef REPLAYWORKER_H
//#define REPLAYWORKER_H

//#pragma once
//#include <QObject>
//#include <QJsonArray>
//#include <QMutex>
//#include <QWaitCondition>
//#include <atomic>

///**
// * @brief ReplayWorker 类（QObject风格）
// * 负责在独立线程中回放输入事件。
// * 支持开始、暂停、恢复、停止、倍速调节。
// */
//class ReplayWorker : public QObject
//{
//    Q_OBJECT
//public:
//    explicit ReplayWorker(QObject *parent = nullptr);
//    ~ReplayWorker() override;

//    void setEvents(const QJsonArray &events);
//    void setOptions(bool replayMouse, bool replayKeyboard);

//public slots:
//    void startReplay();        // 主回放循环（在worker线程中调用）
//    void stopReplay();         // 请求停止（线程安全）
//    void pauseReplay();        // 请求暂停
//    void resumeReplay();       // 恢复暂停
//    void setSpeedFactor(double factor); // 倍速控制（>0.0）

//signals:
//    void replayProgress(int current, int total);  // 进度
//    void stateChanged(const QString &state);      // 状态变更
//    void finished();                              // 回放完成（自然或中断）

//private:
//    void runLoop();
//    void simulateEvent(const QJsonObject &evt);

//private:
//    // 数据
//    QJsonArray m_events;
//    bool m_replayMouse = true;
//    bool m_replayKeyboard = true;

//    // 状态控制
//    std::atomic<bool> m_stopRequested{false};
//    std::atomic<bool> m_paused{false};
//    std::atomic<double> m_speed{1.0};

//    // 同步控制
//    QMutex m_pauseMutex;
//    QWaitCondition m_pauseCond;
//    QWaitCondition m_waitCond;
//};

//#endif // REPLAYWORKER_H

//--------------------------------------------------------------------

//#ifndef REPLAYWORKER_H
//#define REPLAYWORKER_H

//#pragma once
//#include <QObject>
//#include <QJsonArray>
//#include <QMutex>
//#include <QWaitCondition>
//#include <atomic>

///*
// ReplayWorker: QObject style worker. Move to thread and call startReplay() slot.
// - supports startReplay(), stopReplay(), pauseReplay(), resumeReplay(), setSpeedFactor()
// - emits replayProgress(current, total) and stateChanged(QString) and finished()
//*/

//class ReplayWorker : public QObject
//{
//    Q_OBJECT
//public:
//    explicit ReplayWorker(QObject *parent = nullptr);
//    ~ReplayWorker();

//    void setEvents(const QJsonArray &events);
//    void setOptions(bool replayMouse, bool replayKeyboard);

//public slots:
//    void startReplay();    // main loop - called in worker thread
//    void stopReplay();     // request stop
//    void pauseReplay();
//    void resumeReplay();
//    void setSpeedFactor(double f); // e.g. 1.0, 2.0

//signals:
//    void replayProgress(int current, int total);
//    void stateChanged(const QString &state);
//    void finished();

//private:
//    void runLoop();
//    void simulateEvent(const QJsonObject &evt);

//    QJsonArray m_events;
//    bool m_replayMouse = true;
//    bool m_replayKeyboard = true;

//    std::atomic<bool> m_stopRequested;
//    std::atomic<bool> m_paused;
//    std::atomic<double> m_speed;

//    QMutex m_pauseMutex;
//    QWaitCondition m_pauseCond;
//};


//#endif // REPLAYWORKER_H

////----------------------------------------------------------------------------

////#ifndef REPLAYWORKER_H
////#define REPLAYWORKER_H

////#pragma once
////#include <QThread>
////#include <QJsonArray>
////#include <QJsonObject>
////#include <QElapsedTimer>
////#include <atomic>

////class ReplayWorker : public QThread {
////    Q_OBJECT
////public:
////    explicit ReplayWorker(QObject* parent = nullptr);
////    void setReplayData(const QJsonObject& data);
////    void stop();
////    void pause();
////    void resume();
////    void speedUp();

////signals:
////    void progress(int current, int total);
////    void finished();

////protected:
////    void run() override;

////private:
////    QJsonArray events_;
////    std::atomic<bool> running_{false};
////    std::atomic<bool> paused_{false};
////    double speedFactor_{1.0};
////};


////#endif // REPLAYWORKER_H
