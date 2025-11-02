#include "recorder.h"
#include <QJsonDocument>
#include <QTextStream>
#include <QDebug>

//
// RecorderWorker (后台写入线程)
//
RecorderWorker::RecorderWorker(QObject* parent)
    : QThread(parent)
{
}

RecorderWorker::~RecorderWorker()
{
    stopWorker();
}

void RecorderWorker::setOutputFile(const QString& path)
{
    file_.setFileName(path);
}

void RecorderWorker::enqueue(const RecordedEvent& evt)
{
    QMutexLocker locker(&mutex_);
    queue_.push(evt);
    cond_.wakeOne();
}

void RecorderWorker::stopWorker()
{
    {
        QMutexLocker locker(&mutex_);
        running_ = false;
        cond_.wakeOne();
    }
    wait();
    if (file_.isOpen()) file_.close();
}

void RecorderWorker::run()
{
    if (!file_.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "RecorderWorker: 无法打开文件" << file_.fileName();
        return;
    }

    QTextStream out(&file_);
    out << "{\n  \"record_start_time\": \""
        << QDateTime::currentDateTime().toString(Qt::ISODate) << "\",\n"
        << "  \"events\": [\n";
    out.flush();

    running_ = true;
    bool first = true;

    while (true) {
        // 首先锁定 mutex_，确保对 queue_（事件队列）和 running_（运行状态）的访问线程安全。
        mutex_.lock();
        // 若队列空且线程仍在运行（running_ = true），则通过 cond_.wait(&mutex_) 释放锁并阻塞线程，避免空循环浪费 CPU。
        if (queue_.empty() && running_) {
            cond_.wait(&mutex_);
        }
        // 1 当 enqueue 被调用（新事件入队）时，cond_.wakeOne() 唤醒线程；
        // 2 当 stopWorker 被调用（停止线程）时，cond_.wakeOne() 强制唤醒线程，使其退出阻塞。

        // 若线程已停止（running_ = false）且队列空，解锁后跳出循环，准备结束线程。
        if (!running_ && queue_.empty()) {
            mutex_.unlock();
            break;
        }

        auto evt = queue_.front();
        queue_.pop();
        mutex_.unlock();

        if (!first) out << ",\n";
        QJsonDocument doc(evt.json);
        out << doc.toJson(QJsonDocument::Compact);
        first = false;
    }
    // 上述 “等待事件 → 处理事件” 的过程会反复执行，直到 running_ 被设为 false 且队列中的所有事件都被处理完毕

    out << "\n  ]\n}\n";
    out.flush();
    file_.close();

    qDebug() << "RecorderWorker stopped.";
}

//
// Recorder (单例)
//
Recorder& Recorder::instance()
{
    static Recorder inst;
    return inst;
}

Recorder::Recorder(QObject* parent)
    : QObject(parent)
{
}

Recorder::~Recorder()
{
    stopRecording();
}

void Recorder::startRecording(const QString& filePath)
{
    if (recording_) return;

    worker_ = new RecorderWorker();
    worker_->setOutputFile(filePath);
    // worker_->start() 启动线程（触发 run() 方法执行）。
    worker_->start();

    timer_.restart();
    recording_ = true;

    qDebug() << "Recorder started.";
}

void Recorder::stopRecording()
{
    if (!recording_) return;

    worker_->stopWorker();
    delete worker_;
    worker_ = nullptr;

    recording_ = false;
    qDebug() << "Recorder stopped.";
}

void Recorder::onMouseEventCaptured(const MouseEventData& e)
{
    if (!recording_ || !worker_) return;

    RecordedEvent evt;
    evt.json["category"] = "mouse";
    evt.json["x"] = e.pos.x();
    evt.json["y"] = e.pos.y();
    evt.json["type"] = (int)e.type;
    evt.json["timestamp_ms"] = static_cast<qint64>(timer_.elapsed());

    worker_->enqueue(evt);
}

void Recorder::onKeyEventCaptured(const KeyEventData& e)
{
    if (!recording_ || !worker_) return;

    RecordedEvent evt;
    evt.json["category"] = "keyboard";
    evt.json["vkCode"] = (int)e.vkCode;
    evt.json["keyDown"] = e.keyDown;
    evt.json["timestamp_ms"] = static_cast<qint64>(timer_.elapsed());

    worker_->enqueue(evt);
}



// 版本3
//#include "recorder.h"
//#include <QJsonDocument>
//#include <QTextStream>
//#include <QDebug>

//Recorder& Recorder::instance()
//{
//    static Recorder instance;
//    return instance;
//}

//Recorder::Recorder(QObject* parent)
//    : QObject(parent)
//{
//    flushTimer_.setInterval(1000); // 每1秒写一次文件
//    connect(&flushTimer_, &QTimer::timeout, this, &Recorder::flushBuffer);
//}

//Recorder::~Recorder()
//{
//    stopRecording();
//}

//void Recorder::startRecording(const QString& filePath)
//{
//    if (recording_) return;

//    file_.setFileName(filePath);
//    if (!file_.open(QIODevice::WriteOnly | QIODevice::Text)) {
//        qWarning() << "Recorder: 无法打开文件" << filePath;
//        return;
//    }

//    buffer_ = QJsonArray();
//    recording_ = true;
//    timer_.restart();

//    writeHeader();
//    flushTimer_.start();

//    qDebug() << "Recorder started, writing to" << filePath;
//}

//void Recorder::stopRecording()
//{
//    if (!recording_) return;

//    flushBuffer();   // 写出剩余缓存
//    writeFooter();

//    flushTimer_.stop();
//    file_.close();
//    recording_ = false;

//    qDebug() << "Recorder stopped.";
//}

//void Recorder::onMouseEventCaptured(const MouseEventData& e)
//{
//    if (!recording_) return;

//    QJsonObject obj;
//    obj["category"] = "mouse";
//    obj["x"] = e.pos.x();
//    obj["y"] = e.pos.y();
//    obj["type"] = (int)e.type;
//    obj["timestamp_ms"] = static_cast<qint64>(timer_.elapsed());

//    appendEvent(obj);
//}

//void Recorder::onKeyEventCaptured(const KeyEventData& e)
//{
//    if (!recording_) return;

//    QJsonObject obj;
//    obj["category"] = "keyboard";
//    obj["vkCode"] = (int)e.vkCode;
//    obj["keyDown"] = e.keyDown;
//    obj["timestamp_ms"] = static_cast<qint64>(timer_.elapsed());

//    appendEvent(obj);
//}

//void Recorder::appendEvent(const QJsonObject& obj)
//{
//    QMutexLocker locker(&mutex_);
//    buffer_.append(obj);

//    // 若缓冲超过一定数量立即写入
//    if (buffer_.size() >= 50)
//        flushBuffer();
//}

//void Recorder::flushBuffer()
//{
//    if (!recording_ || buffer_.isEmpty()) return;
//    QMutexLocker locker(&mutex_);

//    QTextStream out(&file_);
//    for (const QJsonValue& val : buffer_) {
//        QJsonDocument doc(val.toObject());
//        out << doc.toJson(QJsonDocument::Compact) << "\n";
//    }

//    buffer_ = QJsonArray();
//    out.flush();
//}

//void Recorder::writeHeader()
//{
//    QTextStream out(&file_);
//    QString startTime = QDateTime::currentDateTime().toString(Qt::ISODate);
//    out << "{\n";
//    out << "  \"record_start_time\": \"" << startTime << "\",\n";
//    out << "  \"events\": [\n";
//    out.flush();
//}

//void Recorder::writeFooter()
//{
//    QTextStream out(&file_);
//    out << "  ]\n}\n";
//    out.flush();
//}



// 版本2
//#include "recorder.h"
//#include <QDebug>

//Recorder::Recorder()
//    : m_isRecording(false), m_firstEvent(true)
//{
//}

//Recorder::~Recorder()
//{
//    stopRecording();
//}

//bool Recorder::startRecording(const QString& filePath)
//{
//    QMutexLocker locker(&m_mutex);
//    if (m_isRecording) return false;

//    m_file.setFileName(filePath);
//    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
//        qWarning() << "Recorder: Cannot open file for writing.";
//        return false;
//    }

//    m_stream.setDevice(&m_file);
//    m_stream.setCodec("UTF-8");

//    m_startTime = QDateTime::currentDateTimeUtc();
//    m_firstEvent = true;
//    m_isRecording = true;

//    // 写入文件头
//    m_stream << "{\n";
//    m_stream << "  \"record_start_time\": \"" << m_startTime.toString(Qt::ISODate) << "\",\n";
//    m_stream << "  \"events\": [\n";
//    m_stream.flush();

//    qDebug() << "[Recorder] Started recording at" << m_startTime.toString(Qt::ISODate);
//    return true;
//}

//void Recorder::stopRecording()
//{
//    QMutexLocker locker(&m_mutex);
//    if (!m_isRecording) return;

//    // 写入文件尾
//    m_stream << "\n  ]\n}\n";
//    m_stream.flush();
//    m_file.close();

//    m_isRecording = false;
//    qDebug() << "[Recorder] Stopped recording.";
//}

//qint64 Recorder::elapsedMs(const QDateTime& current) const
//{
//    return m_startTime.msecsTo(current);
//}

//void Recorder::writeEvent(const QJsonObject& event)
//{
//    if (!m_isRecording)
//        return;

//    QMutexLocker locker(&m_mutex);
//    if (!m_firstEvent)
//        m_stream << ",\n";
//    else
//        m_firstEvent = false;

//    QJsonDocument doc(event);
//    m_stream << "    " << doc.toJson(QJsonDocument::Compact);
//    m_stream.flush();
//}

//void Recorder::onMouseEventCaptured(const MouseEventData& data)
//{
//    if (!m_isRecording) return;

//    QJsonObject obj;
//    obj["category"] = "mouse";
//    obj["x"] = data.pos.x();
//    obj["y"] = data.pos.y();
//    obj["type"] = static_cast<int>(data.type);
//    obj["timestamp_ms"] = elapsedMs(data.time);

//    writeEvent(obj);
//}

//void Recorder::onKeyEventCaptured(const KeyEventData& data)
//{
//    if (!m_isRecording) return;

//    QJsonObject obj;
//    obj["category"] = "keyboard";
//    obj["vkCode"] = static_cast<int>(data.vkCode);
//    obj["keyDown"] = data.keyDown;
//    obj["timestamp_ms"] = elapsedMs(data.time);

//    writeEvent(obj);
//}



// 版本1
//#include "recorder.h"
//#include <QDebug>

//Recorder::Recorder() : m_isRecording(false) {}

//Recorder::~Recorder()
//{
//    stopRecording();
//}

//bool Recorder::startRecording(const QString& filePath)
//{
//    QMutexLocker locker(&m_mutex);
//    if (m_isRecording) return false;

//    m_file.setFileName(filePath);
//    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
//        qWarning() << "Recorder: Cannot open file for recording.";
//        return false;
//    }

//    m_stream.setDevice(&m_file);
//    m_stream.setCodec("UTF-8");
//    m_isRecording = true;

//    qDebug() << "[Recorder] Started recording to" << filePath;
//    return true;
//}

//void Recorder::stopRecording()
//{
//    QMutexLocker locker(&m_mutex);
//    if (!m_isRecording) return;

//    m_file.flush();
//    m_file.close();
//    m_isRecording = false;
//    qDebug() << "[Recorder] Stopped recording.";
//}

//void Recorder::onMouseEventCaptured(const MouseEventData& data)
//{
//    if (!m_isRecording) return;

//    QJsonObject obj;
//    obj["type"] = "mouse";

//    QString eventType;
//    switch (data.type) {
//    case WM_MOUSEMOVE: eventType = "move"; break;
//    case WM_LBUTTONDOWN: eventType = "left_down"; break;
//    case WM_LBUTTONUP: eventType = "left_up"; break;
//    case WM_RBUTTONDOWN: eventType = "right_down"; break;
//    case WM_RBUTTONUP: eventType = "right_up"; break;
//    default: eventType = "other"; break;
//    }

//    obj["event"] = eventType;
//    obj["x"] = data.pos.x();
//    obj["y"] = data.pos.y();
//    obj["timestamp"] = data.time.toString(Qt::ISODateWithMs);

//    writeJsonLine(obj);
//}

//void Recorder::onKeyEventCaptured(const KeyEventData& data)
//{
//    if (!m_isRecording) return;

//    QJsonObject obj;
//    obj["type"] = "key";
//    obj["event"] = data.keyDown ? "press" : "release";
//    obj["keycode"] = static_cast<int>(data.vkCode);
//    obj["timestamp"] = data.time.toString(Qt::ISODateWithMs);

//    writeJsonLine(obj);
//}

//void Recorder::writeJsonLine(const QJsonObject& obj)
//{
//    QMutexLocker locker(&m_mutex);
//    if (!m_isRecording) return;

//    QJsonDocument doc(obj);
//    m_stream << doc.toJson(QJsonDocument::Compact) << "\n";
//    m_stream.flush(); // 实时写入磁盘
//}



//// recorder.cpp
//#include "recorder.h"
//#include <QDebug>

//Recorder::Recorder() : m_isRecording(false) {}
//Recorder::~Recorder() { stopRecording(); }

//bool Recorder::startRecording(const QString& filePath)
//{
//    QMutexLocker locker(&m_mutex);
//    if (m_isRecording) return false;

//    m_file.setFileName(filePath);
//    if (!m_file.open(QIODevice::WriteOnly)) {
//        qWarning() << "Recorder: Cannot open file for recording.";
//        return false;
//    }

//    m_stream.setDevice(&m_file);
//    m_isRecording = true;
//    qDebug() << "Recorder started.";
//    return true;
//}

//void Recorder::stopRecording()
//{
//    QMutexLocker locker(&m_mutex);
//    if (!m_isRecording) return;

//    m_file.close();
//    m_isRecording = false;
//    qDebug() << "Recorder stopped.";
//}

//void Recorder::onMouseEventCaptured(const MouseEventData& data)
//{
//    if (!m_isRecording) return;
//    QMutexLocker locker(&m_mutex);
//    writeMouseEvent(data);
//}

//void Recorder::onKeyEventCaptured(const KeyEventData& data)
//{
//    if (!m_isRecording) return;
//    QMutexLocker locker(&m_mutex);
//    writeKeyEvent(data);
//}

//void Recorder::writeMouseEvent(const MouseEventData& data)
//{
//    m_stream << QString("Mouse") << data.pos << (quint32)data.type << data.time;
//}

//void Recorder::writeKeyEvent(const KeyEventData& data)
//{
//    m_stream << QString("Key") << (quint32)data.vkCode << data.keyDown << data.time;
//}




//#include "recorder.h"

//Recorder::Recorder(QObject *parent)
//    : QObject(parent)
//{
//    qRegisterMetaType<MouseEventData>("MouseEventData");
//    qRegisterMetaType<KeyEventData>("KeyEventData");
//}

//bool Recorder::startRecording(const QString &filePath)
//{
//    QMutexLocker locker(&m_mutex);
//    if (m_isRecording)
//        return false;

//    m_file.setFileName(filePath);
//    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
//        qWarning() << "Cannot open file for recording:" << filePath;
//        return false;
//    }

//    m_startTime = QDateTime::currentDateTime();
//    m_events = QJsonArray();
//    m_isRecording = true;
//    qDebug() << "Recording started ->" << filePath;
//    return true;
//}

//void Recorder::stopRecording()
//{
//    QMutexLocker locker(&m_mutex);
//    if (!m_isRecording)
//        return;

//    saveToFile();
//    m_file.close();
//    m_isRecording = false;
//    qDebug() << "Recording stopped.";
//}

//void Recorder::onMouseCaptured(const MouseEventData &data)
//{
//    if (!m_isRecording) return;

//    QMutexLocker locker(&m_mutex);
//    QJsonObject obj;
//    obj["category"] = "mouse";
//    obj["x"] = data.pos.x();
//    obj["y"] = data.pos.y();
//    obj["type"] = (int)data.type;
//    obj["time"] = data.time.toString(Qt::ISODate);
//    obj["timestamp_ms"] = m_startTime.msecsTo(data.time);
//    m_events.append(obj);

//    if (m_events.size() % 200 == 0)
//        saveToFile();
//}

//void Recorder::onKeyCaptured(const KeyEventData &data)
//{
//    if (!m_isRecording) return;

//    QMutexLocker locker(&m_mutex);
//    QJsonObject obj;
//    obj["category"] = "keyboard";
//    obj["vkCode"] = (int)data.vkCode;
//    obj["keyDown"] = data.keyDown;
//    obj["time"] = data.time.toString(Qt::ISODate);
//    obj["timestamp_ms"] = m_startTime.msecsTo(data.time);
//    m_events.append(obj);

//    if (m_events.size() % 200 == 0)
//        saveToFile();
//}

//void Recorder::saveToFile()
//{
//    if (!m_file.isOpen()) return;

//    QJsonObject root;
//    root["record_start_time"] = m_startTime.toString(Qt::ISODate);
//    root["event_count"] = m_events.size();
//    root["events"] = m_events;

//    QJsonDocument doc(root);
//    m_file.seek(0);
//    m_file.write(doc.toJson(QJsonDocument::Indented));
//    m_file.flush();
//}
