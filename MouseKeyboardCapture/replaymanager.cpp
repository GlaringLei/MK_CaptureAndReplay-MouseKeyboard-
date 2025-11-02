#include "replaymanager.h"
#include "replayworker.h"
#include "globalhotkeymanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QJsonObject>

ReplayManager& ReplayManager::instance()
{
    static ReplayManager inst;
    return inst;
}

ReplayManager::ReplayManager(QObject* parent)
    : QObject(parent)
{
    // connect hotkey to manager controls
    connect(&GlobalHotkeyManager::instance(), &GlobalHotkeyManager::hotkeyPressed,
            this, [this](GlobalHotkeyManager::HotkeyAction action){
        switch (action) {
        case GlobalHotkeyManager::StopReplay:   this->stopReplay(); break;
        case GlobalHotkeyManager::PauseReplay:  this->pauseReplay(); break;
        case GlobalHotkeyManager::ResumeReplay: this->resumeReplay(); break;
        case GlobalHotkeyManager::SpeedUpReplay: {
            double cur = m_speed;
            double next = (cur > 1.5) ? 1.0 : 2.0;
            setSpeedMultiplier(next);
            break;
        }
        }
    });
}

ReplayManager::~ReplayManager()
{
    stopReplay();
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}

bool ReplayManager::loadReplayFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "ReplayManager: cannot open" << path;
        return false;
    }
    QByteArray ba = f.readAll();
    f.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(ba, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << err.errorString();
        return false;
    }
    if (!doc.isObject()) return false;
    QJsonObject root = doc.object();
    if (!root.contains("events") || !root.value("events").isArray()) return false;
    m_events = root.value("events").toArray();
    return true;
}

bool ReplayManager::startReplay()
{
    if (m_replaying) return false;
    if (m_events.isEmpty()) return false;

    stopReplay(); // 保证干净状态

    m_worker = new ReplayWorker();
    m_worker->setEvents(m_events);
    m_worker->setOptions(m_replayMouse, m_replayKeyboard);
    m_worker->setSpeedFactor(m_speed);

    m_worker->moveToThread(&m_thread);

    // 当线程结束或 worker finished 时做好清理
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &ReplayWorker::finished, this, &ReplayManager::onWorkerFinished);
    connect(m_worker, &ReplayWorker::replayProgress, this, &ReplayManager::onWorkerProgress);
    connect(m_worker, &ReplayWorker::stateChanged, this, &ReplayManager::onWorkerStateChanged);

    m_thread.start();

    // 以 queued 调用 startReplay 在 worker 所在线程中开始同步 runLoop()
    QMetaObject::invokeMethod(m_worker, "startReplay", Qt::QueuedConnection);

    m_replaying = true;
    emit stateChanged("started");
    return true;


//    if (m_replaying) return false;
//    if (m_events.isEmpty()) return false;

//    // cleanup previous worker if any
//    stopReplay();

//    m_worker = new ReplayWorker();
//    m_worker->setEvents(m_events);
//    m_worker->setOptions(m_replayMouse, m_replayKeyboard);
//    m_worker->setSpeedFactor(m_speed);

//    // move to thread
//    m_worker->moveToThread(&m_thread);

//    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
//    connect(m_worker, &ReplayWorker::replayProgress, this, &ReplayManager::onWorkerProgress);
//    connect(m_worker, &ReplayWorker::stateChanged, this, &ReplayManager::onWorkerStateChanged);
//    connect(m_worker, &ReplayWorker::finished, this, &ReplayManager::onWorkerFinished);

//    m_thread.start();
//    // call startReplay() in worker thread
//    QMetaObject::invokeMethod(m_worker, "startReplay", Qt::QueuedConnection);

//    m_replaying = true;
//    emit stateChanged("started");
//    return true;
}

void ReplayManager::stopReplay()
{
    if (!m_worker) return;

    qDebug() << "[ReplayManager] stopReplay called (direct call)";

    // 1) 直接调用（线程安全的）stopReplay()，不要用 Qt::QueuedConnection
    m_worker->stopReplay(); // 立即设置原子标志并 wakeAll()

    // 2) 等待 worker 发出 finished 信号，给个超时以防死锁（例如 3 秒）
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(m_worker, &ReplayWorker::finished, &loop, &QEventLoop::quit);
    timer.start(3000); // 超时（毫秒）
    loop.exec();

    // 3) 确保线程停止并清理
    if (m_thread.isRunning()) {
        // 如果 worker 已经发 finished，thread 的退出可以由连接处理，
        // 这里仍然安全地请求退出并等待
        m_thread.quit();
        m_thread.wait(2000);
    }

    // 4) 清理指针/状态
    m_worker = nullptr;
    m_replaying = false;
    emit stateChanged("stopped");
    qDebug() << "[ReplayManager] stopReplay completed";



//    if (!m_worker) return;

//    qDebug() << "[ReplayManager] stopReplay called";

//    // 1. 请求 worker 停止（异步调用）
//    QMetaObject::invokeMethod(m_worker, "stopReplay", Qt::QueuedConnection);

//    // 2. 等待 worker 发出 finished 信号
//    QEventLoop loop;
//    connect(m_worker, &ReplayWorker::finished, &loop, &QEventLoop::quit);

//    // 加上超时保护防死锁
//    QTimer timer;
//    timer.setSingleShot(true);
//    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
//    timer.start(3000); // 最多等3秒

//    loop.exec(); // 等待线程真正结束 runLoop()

//    // 3. 等 runLoop() 完成后再关闭线程
//    if (m_thread.isRunning()) {
//        m_thread.quit();
//        m_thread.wait();
//    }

//    m_worker = nullptr;
//    m_replaying = false;
//    emit stateChanged("stopped");

//    if (!m_worker) return;
//    QMetaObject::invokeMethod(m_worker, "stopReplay", Qt::QueuedConnection);
//    // wait for worker finished and thread exit
//    if (m_thread.isRunning()) {
//        m_thread.quit();
//        m_thread.wait();
//    }
//    m_worker = nullptr;
//    m_replaying = false;
//    emit stateChanged("stopped");
}

void ReplayManager::pauseReplay()
{
    if (m_worker) m_worker->pauseReplay(); // 直接调用
//    if (m_worker) QMetaObject::invokeMethod(m_worker, "pauseReplay", Qt::QueuedConnection);
}

void ReplayManager::resumeReplay()
{
    if (m_worker) m_worker->resumeReplay();
//    if (m_worker) QMetaObject::invokeMethod(m_worker, "resumeReplay", Qt::QueuedConnection);
}

void ReplayManager::setReplayMouse(bool en) { m_replayMouse = en; }
void ReplayManager::setReplayKeyboard(bool en) { m_replayKeyboard = en; }
void ReplayManager::setSpeedMultiplier(double f)
{
    m_speed = f;
    if (m_worker) m_worker->setSpeedFactor(f);
    emit stateChanged(QString("speed=%1").arg(f));
//    m_speed = f;
//    if (m_worker) QMetaObject::invokeMethod(m_worker, "setSpeedFactor", Qt::QueuedConnection, Q_ARG(double, f));
//    emit stateChanged(QString("speed=%1").arg(f));
}

void ReplayManager::onWorkerFinished()
{
    // ensure thread stops
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
    m_worker = nullptr;
    m_replaying = false;
    emit replayFinished();
    emit stateChanged("finished");
}

void ReplayManager::onWorkerProgress(int cur, int total)
{
    emit replayProgress(cur, total);
}

void ReplayManager::onWorkerStateChanged(const QString &s)
{
    emit stateChanged(s);
}
