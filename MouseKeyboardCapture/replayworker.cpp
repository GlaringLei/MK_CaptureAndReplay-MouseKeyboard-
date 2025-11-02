#include "replayworker.h"
#include <QJsonObject>
#include <QDebug>
#include <QThread>
#include <chrono>

#define NOMINMAX
#ifdef Q_OS_WIN
#include <windows.h>
#endif

ReplayWorker::ReplayWorker(QObject *parent)
    : QObject(parent)
{
    qDebug() << "[ReplayWorker] created";
}

ReplayWorker::~ReplayWorker()
{
    qDebug() << "[ReplayWorker] destroyed";
    stopReplay();
}

void ReplayWorker::setEvents(const QJsonArray &events)
{
    m_events = events;
}

void ReplayWorker::setOptions(bool replayMouse, bool replayKeyboard)
{
    m_replayMouse = replayMouse;
    m_replayKeyboard = replayKeyboard;
}

void ReplayWorker::setSpeedFactor(double f)
{
    if (f > 0.0) {
        m_speed.store(f);
        qDebug() << "[ReplayWorker] Speed factor set to" << f;
    }
}

void ReplayWorker::stopReplay()
{
    qDebug() << "[ReplayWorker] Stop requested by manager.";

    m_stopRequested.store(true);

    {
        QMutexLocker locker(&m_pauseMutex);
        m_paused.store(false);
        m_pauseCond.wakeAll();
    }

    // 唤醒 sleep 等待
    {
        QMutexLocker locker(&m_waitMutex);
        m_waitCond.wakeAll();
    }

    emit stateChanged("stopping");
}

void ReplayWorker::pauseReplay()
{
    if (!m_paused.load()) {
        m_paused.store(true);
        emit stateChanged("paused");
        qDebug() << "[ReplayWorker] Paused.";
    }
}

void ReplayWorker::resumeReplay()
{
    if (m_paused.load()) {
        QMutexLocker locker(&m_pauseMutex);
        m_paused.store(false);
        m_pauseCond.wakeAll();
        emit stateChanged("resumed");
        qDebug() << "[ReplayWorker] Resumed.";
    }
}

void ReplayWorker::startReplay()
{
    if (m_events.isEmpty()) {
        qDebug() << "[ReplayWorker] No events loaded, finish immediately.";
        emit stateChanged("finished");
        emit finished();
        return;
    }

    qDebug() << "[ReplayWorker] Start replaying with" << m_events.size() << "events";
    m_stopRequested.store(false);
    runLoop();
}

void ReplayWorker::runLoop()
{
    const int total = m_events.size()-2;
    qint64 last_ts = 0;

    // 已经更新：不再回放操作的最后两个事件（按下左键和松开左键），也就是结束录制这一步，不会被回放，避免在回放过程中的误触。
    for (int i = 0; i < total; ++i)
    {
        // 立即检查 stop
        if (m_stopRequested.load()) {
            qDebug() << "[ReplayWorker] Stop flag detected (begin loop).";
            break;
        }

        // 处理 pause
        if (m_paused.load()) {
            QMutexLocker locker(&m_pauseMutex);
            while (m_paused.load() && !m_stopRequested.load()) {
                m_pauseCond.wait(&m_pauseMutex, 50);
            }
            if (m_stopRequested.load()) {
                qDebug() << "[ReplayWorker] Stop detected during pause.";
                break;
            }
        }

        // 取事件
        QJsonObject evt = m_events.at(i).toObject();
        qint64 ts = evt.value("timestamp_ms").toVariant().toLongLong();
        qint64 delta = (i == 0) ? ts : (ts - last_ts);
        last_ts = ts;

        // 延时计算（支持倍速）
        double speed = m_speed.load();
        if (speed <= 0.0) speed = 1.0;
        qint64 waitMs = static_cast<qint64>(delta / speed);

        // 分片 sleep + 可中断等待
        qint64 slept = 0;
        const qint64 slice = 50;
        while (slept < waitMs) {
            if (m_stopRequested.load() || m_paused.load())
                break;

            qint64 remain = waitMs - slept;
            qint64 toWait = std::min(remain, slice);

            QMutexLocker locker(&m_waitMutex);
            m_waitCond.wait(&m_waitMutex, static_cast<unsigned long>(toWait));

            if (m_stopRequested.load() || m_paused.load())
                break;

            slept += toWait;
        }

        if (m_stopRequested.load()) {
            qDebug() << "[ReplayWorker] Stop detected after sleep.";
            break;
        }

        if (m_paused.load()) continue;

        // 执行事件（带二次检查）
        if (m_stopRequested.load()) break;

        QString cat = evt.value("category").toString();
        bool doMouse = (m_replayMouse && cat == "mouse");
        bool doKey   = (m_replayKeyboard && cat == "keyboard");

        if ((doMouse || doKey) && !m_stopRequested.load()) {
            simulateEvent(evt);
        }

        emit replayProgress(i + 1, total);
    }

    QString finalState = m_stopRequested.load() ? "stopped" : "finished";
    qDebug() << "[ReplayWorker] Replay loop exited with state:" << finalState;
    emit stateChanged(finalState);
    emit finished();
}

void ReplayWorker::simulateEvent(const QJsonObject &evt)
{
    if (m_stopRequested.load()) return;

#ifdef Q_OS_WIN
    QString cat = evt.value("category").toString();
    if (cat == "mouse") {
        int x = evt.value("x").toInt();
        int y = evt.value("y").toInt();
        int type = evt.value("type").toInt();

        if (m_stopRequested.load()) return;
        SetCursorPos(x, y);

        INPUT in; ZeroMemory(&in, sizeof(in));
        in.type = INPUT_MOUSE;

        if (type == 513) in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        else if (type == 514) in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        else if (type == 516) in.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        else if (type == 517) in.mi.dwFlags = MOUSEEVENTF_RIGHTUP;

        if (!m_stopRequested.load()) SendInput(1, &in, sizeof(in));
    }
    else if (cat == "keyboard") {
        int vk = evt.value("vkCode").toInt();
        bool down = evt.value("keyDown").toBool();
        if (m_stopRequested.load()) return;

        INPUT in; ZeroMemory(&in, sizeof(in));
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = static_cast<WORD>(vk & 0xFFFF);
        if (!down) in.ki.dwFlags = KEYEVENTF_KEYUP;
        if (!m_stopRequested.load()) SendInput(1, &in, sizeof(in));
    }
#else
    Q_UNUSED(evt)
#endif
}



//--------------------------------------------------------------------

//#include "replayworker.h"
//#include <QJsonObject>
//#include <QElapsedTimer>
//#include <QThread>
//#include <QDebug>

//#define NOMINMAX
//#ifdef Q_OS_WIN
//#include <windows.h>
//#endif

//#include <chrono>

//ReplayWorker::ReplayWorker(QObject *parent)
//    : QObject(parent)
//{
//    qDebug() << "[ReplayWorker] created";
//}

//ReplayWorker::~ReplayWorker()
//{
//    qDebug() << "[ReplayWorker] destroyed";
//    // 不直接调用 stopReplay()，避免析构时干扰线程生命周期
//    m_stopRequested = true;
//    m_pauseCond.wakeAll();
//}

//void ReplayWorker::setEvents(const QJsonArray &events)
//{
//    m_events = events;
//}

//void ReplayWorker::setOptions(bool replayMouse, bool replayKeyboard)
//{
//    m_replayMouse = replayMouse;
//    m_replayKeyboard = replayKeyboard;
//}

//void ReplayWorker::setSpeedFactor(double factor)
//{
//    if (factor > 0.0) {
//        m_speed.store(factor);
//        qDebug() << "[ReplayWorker] Speed factor set to" << factor;
//    }
//}

//void ReplayWorker::startReplay()
//{
//    if (m_events.isEmpty()) {
//        emit stateChanged("empty");
//        emit finished();
//        return;
//    }

//    m_stopRequested = false;
//    m_paused = false;

//    emit stateChanged("started");
//    qDebug() << "[ReplayWorker] Start replaying with" << m_events.size() << "events";

//    runLoop();
//}

//void ReplayWorker::stopReplay()
//{
//    m_stopRequested.store(true);
//    {
//        QMutexLocker locker(&m_pauseMutex);
//        m_paused = false;
//        m_pauseCond.wakeAll(); // 唤醒暂停状态
//    }
//    m_waitCond.wakeAll(); // 唤醒sleep状态

////    if (!m_stopRequested.exchange(true)) {
////        qDebug() << "[ReplayWorker] Stop flag set.";
////        m_paused = false;
////        m_pauseCond.wakeAll();
////    }
//}

//void ReplayWorker::pauseReplay()
//{
//    if (!m_paused.exchange(true)) {
//        qDebug() << "[ReplayWorker] Paused.";
//        emit stateChanged("paused");
//    }
//}

//void ReplayWorker::resumeReplay()
//{
//    if (m_paused.exchange(false)) {
//        qDebug() << "[ReplayWorker] Resumed.";
//        {
//            QMutexLocker locker(&m_pauseMutex);
//            m_pauseCond.wakeAll();
//        }
//        emit stateChanged("resumed");
//    }
//}

//void ReplayWorker::runLoop()
//{
//    const int total = m_events.size();
//    qint64 last_ts = 0;

//    for (int i = 0; i < total; ++i) {

//        if (m_stopRequested.load()) {
//            qDebug() << "[ReplayWorker] Stop requested. Break loop.";
//            break;
//        }

//        // Pause handling
//        if (m_paused.load()) {
//            QMutexLocker locker(&m_pauseMutex);
//            while (m_paused.load() && !m_stopRequested.load()) {
//                m_pauseCond.wait(&m_pauseMutex, 50);
//            }
//        }
//        if (m_stopRequested.load()) break;

//        // 获取事件时间戳
//        const QJsonObject evt = m_events.at(i).toObject();
//        const qint64 ts = evt.value("timestamp_ms").toVariant().toLongLong();
//        const qint64 delta = (i == 0) ? ts : (ts - last_ts);
//        last_ts = ts;

//        // 等待间隔（受speed影响）
//        double speed = m_speed.load();
//        if (speed <= 0.0) speed = 1.0;
//        const qint64 waitMs = static_cast<qint64>(delta / speed);

//        // 分片 sleep 检查 stop/pause
//        qint64 slept = 0;
//        const qint64 slice = 50;
//        while (slept < waitMs && !m_stopRequested.load() && !m_paused.load()) {
//            qint64 remain = waitMs - slept;
//            qint64 toSleep = std::min(remain, slice);
//            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<unsigned long>(toSleep)));
//            slept += toSleep;
//        }

//        if (m_stopRequested.load()) break;
//        if (m_paused.load()) continue;

//        // 事件模拟
//        simulateEvent(evt);
//        emit replayProgress(i + 1, total);
//    }

//    // 状态汇报
//    QString state = m_stopRequested.load() ? "stopped" : "finished";
//    emit stateChanged(state);
//    emit finished();

//    qDebug() << "[ReplayWorker] Replay loop exited with state:" << state;
//}

//void ReplayWorker::simulateEvent(const QJsonObject &evt)
//{
//    if (m_stopRequested.load()) return;

//#ifdef Q_OS_WIN
//    QString cat = evt.value("category").toString();
//    if (cat == "mouse" && m_replayMouse) {
//        int x = evt.value("x").toInt();
//        int y = evt.value("y").toInt();
//        int type = evt.value("type").toInt();
//        SetCursorPos(x, y);

//        INPUT in; ZeroMemory(&in, sizeof(in));
//        in.type = INPUT_MOUSE;
//        switch (type) {
//        case 513: in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; break;
//        case 514: in.mi.dwFlags = MOUSEEVENTF_LEFTUP; break;
//        case 516: in.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; break;
//        case 517: in.mi.dwFlags = MOUSEEVENTF_RIGHTUP; break;
//        default: return;
//        }
//        SendInput(1, &in, sizeof(in));
//    }
//    else if (cat == "keyboard" && m_replayKeyboard) {
//        int vk = evt.value("vkCode").toInt();
//        bool down = evt.value("keyDown").toBool();
//        INPUT in; ZeroMemory(&in, sizeof(in));
//        in.type = INPUT_KEYBOARD;
//        in.ki.wVk = static_cast<WORD>(vk & 0xFFFF);
//        if (!down) in.ki.dwFlags = KEYEVENTF_KEYUP;
//        SendInput(1, &in, sizeof(in));
//    }
//#else
//    Q_UNUSED(evt)
//#endif
//}

//--------------------------------------------------------------------

//#include "replayworker.h"
//#include <QJsonObject>
//#include <QElapsedTimer>
//#include <QThread>
//#include <QJsonDocument>
//#include <QDebug>
//#ifdef Q_OS_WIN
//#include <windows.h>
//#endif
//#include <chrono>

//ReplayWorker::ReplayWorker(QObject *parent)
//    : QObject(parent),
//      m_stopRequested(false),
//      m_paused(false),
//      m_speed(1.0)
//{}

//ReplayWorker::~ReplayWorker()
//{
//    stopReplay();
//}

//void ReplayWorker::setEvents(const QJsonArray &events) { m_events = events; }
//void ReplayWorker::setOptions(bool replayMouse, bool replayKeyboard) {
//    m_replayMouse = replayMouse;
//    m_replayKeyboard = replayKeyboard;
//}

//void ReplayWorker::startReplay()
//{
//    emit stateChanged("started");
//    runLoop();
//}

//void ReplayWorker::stopReplay()
//{
//    m_stopRequested = true;
//    // wake if paused
//    {
//        m_paused = false;
//        m_pauseCond.wakeAll();
//    }
//    qDebug() << "[ReplayWorker] Stop flag set.";
//}

//void ReplayWorker::pauseReplay()
//{
//    m_paused = true;
//    emit stateChanged("paused");
//}

//void ReplayWorker::resumeReplay()
//{
//    if (m_paused) {
//        {
//            QMutexLocker locker(&m_pauseMutex);
//            m_paused = false;
//            m_pauseCond.wakeAll();
//        }
//        emit stateChanged("resumed");
//    }
//}

//void ReplayWorker::setSpeedFactor(double f)
//{
//    if (f > 0.0) m_speed = f;
//    qDebug() << "[ReplayWorker] speed set to" << m_speed.load();
//}

//void ReplayWorker::runLoop()
//{
//    if (m_events.isEmpty()) {
//        emit stateChanged("finished");
//        emit finished();
//        return;
//    }

//    const int total = m_events.size();
//    qint64 last_ts = 0;

//    for (int i = 0; i < total; ++i) {
//        // 立即响应 stop
//        {
//            QMutexLocker locker(&m_pauseMutex);
//            if (m_stopRequested.load()) {
//                qDebug() << "[ReplayWorker] Stop requested, breaking loop.";
//                break;
//            }
//        }

//        // pause handling
//        if (m_paused.load()) {
//            QMutexLocker locker(&m_pauseMutex);
//            while (m_paused.load() && !m_stopRequested.load()) {
//                m_pauseCond.wait(&m_pauseMutex, 50); // 分片等待
//            }
//            if (m_stopRequested.load()) break;
//        }

//        // 读取事件
//        QJsonObject evt = m_events.at(i).toObject();
//        qint64 ts = evt.value("timestamp_ms").toVariant().toLongLong();
//        qint64 delta = (i == 0) ? ts : (ts - last_ts);
//        last_ts = ts;

//        // 计算等待时间（支持倍速）
//        double speed = m_speed.load();
//        if (speed <= 0.0) speed = 1.0;
//        qint64 waitMs = static_cast<qint64>(delta / speed);

//        // 分片 sleep 并随时检查 stop/pause
//        qint64 slept = 0;
//        const qint64 slice = 50;
//        while (slept < waitMs) {
//            if (m_stopRequested.load()) break;
//            if (m_paused.load()) break;

//            qint64 remain = waitMs - slept;
//            qint64 toSleep = (remain > slice) ? slice : remain;
//            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<unsigned long>(toSleep)));
//            slept += toSleep;
//        }
//        if (m_stopRequested.load()) break;
//        if (m_paused.load()) continue;

//        // 条件回放前再次检查 stop
//        if (m_stopRequested.load()) break;

//        // 条件回放
//        QString cat = evt.value("category").toString();
//        bool doMouse = (m_replayMouse && cat == "mouse");
//        bool doKey   = (m_replayKeyboard && cat == "keyboard");

//        if ((doMouse || doKey) && !m_stopRequested.load()) {
//            simulateEvent(evt); // simulateEvent 内最好也加 if (m_stopRequested.load()) return;
//        }

//        emit replayProgress(i + 1, total);
//    }

//    // 正确发出结束状态
//    if (m_stopRequested.load())
//        emit stateChanged("stopped");
//    else
//        emit stateChanged("finished");

//    emit finished();
//}

//static WORD vkFromInt(int vk)
//{
//    return static_cast<WORD>(vk & 0xFFFF);
//}

//void ReplayWorker::simulateEvent(const QJsonObject &evt)
//{
//    if (m_stopRequested.load()) return; // <-- 立即中断

//#ifdef Q_OS_WIN
//    QString cat = evt.value("category").toString();

//    if (m_stopRequested.load()) return; // <-- 二次保险

//    if (cat == "mouse") {
//        int x = evt.value("x").toInt();
//        int y = evt.value("y").toInt();
//        int type = evt.value("type").toInt();
//        SetCursorPos(x, y);
//        INPUT in; ZeroMemory(&in, sizeof(in)); in.type = INPUT_MOUSE;
//        if (type == 513) { in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; SendInput(1, &in, sizeof(in)); }
//        else if (type == 514) { in.mi.dwFlags = MOUSEEVENTF_LEFTUP; SendInput(1, &in, sizeof(in)); }
//        else if (type == 516) { in.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; SendInput(1, &in, sizeof(in)); }
//        else if (type == 517) { in.mi.dwFlags = MOUSEEVENTF_RIGHTUP; SendInput(1, &in, sizeof(in)); }
//    }
//    else if (cat == "keyboard") {
//        int vk = evt.value("vkCode").toInt();
//        bool down = evt.value("keyDown").toBool();
//        INPUT in; ZeroMemory(&in, sizeof(in)); in.type = INPUT_KEYBOARD;
//        in.ki.wVk = vkFromInt(vk);
//        if (!down) in.ki.dwFlags = KEYEVENTF_KEYUP;
//        SendInput(1, &in, sizeof(in));
//    }
//#else
//    Q_UNUSED(evt)
//#endif
//}
