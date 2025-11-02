#ifndef RECORDER_H
#define RECORDER_H


#include <QObject>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QDateTime>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <queue>
#include "captureengine.h"

//struct MouseEventData {
//    QPoint pos;
//    DWORD type;
//    QDateTime time;
//};

//struct KeyEventData {
//    DWORD vkCode;
//    bool keyDown;
//    QDateTime time;
//};

struct RecordedEvent {
    QJsonObject json;
};

class RecorderWorker : public QThread {
    Q_OBJECT
public:
    explicit RecorderWorker(QObject* parent = nullptr);
    ~RecorderWorker();

    void setOutputFile(const QString& path);
    void enqueue(const RecordedEvent& evt);
    void stopWorker();

protected:
    void run() override;

private:
    QFile file_;
    bool running_ = false;
    QMutex mutex_;
    QWaitCondition cond_;
    std::queue<RecordedEvent> queue_;
};


class Recorder : public QObject
{
    Q_OBJECT
public:
    static Recorder& instance();
    void startRecording(const QString& filePath);
    void stopRecording();
    bool isRecording() const { return recording_; }

public slots:
    void onMouseEventCaptured(const MouseEventData& event);
    void onKeyEventCaptured(const KeyEventData& event);

private:
    explicit Recorder(QObject* parent = nullptr);
    ~Recorder();

private:
    RecorderWorker* worker_ = nullptr;
    bool recording_ = false;
    QElapsedTimer timer_;
};




// 版本3，已改进写入的时机，避免频繁写入降低系统性能，实现批量写入。
//#include <QObject>
//#include <QFile>
//#include <QJsonArray>
//#include <QJsonObject>
//#include <QElapsedTimer>
//#include <QTimer>
//#include <QMutex>
//#include <QDateTime>
//#include "captureengine.h"

///**
// * @brief Recorder 模块（单例）
// * 支持实时录制、分块写入、相对时间戳（毫秒）
// */
//class Recorder : public QObject
//{
//    Q_OBJECT
//public:
//    static Recorder& instance();     // 单例访问
//    void startRecording(const QString& filePath);
//    void stopRecording();

//    bool isRecording() const { return recording_; }

//public slots:
//    void onMouseEventCaptured(const MouseEventData& event);
//    void onKeyEventCaptured(const KeyEventData& event);

//private slots:
//    void flushBuffer();  // 定期写入缓冲区到文件

//private:
//    explicit Recorder(QObject* parent = nullptr);
//    ~Recorder();

//    void writeHeader();
//    void writeFooter();
//    void appendEvent(const QJsonObject& obj);

//private:
//    QFile file_;
//    bool recording_ = false;
//    QElapsedTimer timer_;       // 用于计算相对时间
//    QJsonArray buffer_;         // 临时事件缓冲区
//    QMutex mutex_;              // 线程安全保护
//    QTimer flushTimer_;         // 定期写入定时器
//};


// 上一个版本2，已更新json的格式化输出，但是还需要改进写入的时机，避免频繁写入降低系统性能，实现批量写入。
//#pragma once
//#include <QObject>
//#include <QFile>
//#include <QTextStream>
//#include <QMutex>
//#include <QDateTime>
//#include <QJsonObject>
//#include <QJsonDocument>
//#include "captureengine.h"

//class Recorder : public QObject
//{
//    Q_OBJECT
//public:
//    static Recorder& instance()
//    {
//        static Recorder instance;
//        return instance;
//    }

//    bool startRecording(const QString& filePath);
//    void stopRecording();
//    bool isRecording() const { return m_isRecording; }

//public slots:
//    void onMouseEventCaptured(const MouseEventData& data);
//    void onKeyEventCaptured(const KeyEventData& data);

//private:
//    Recorder();
//    ~Recorder();
//    Recorder(const Recorder&) = delete;
//    Recorder& operator=(const Recorder&) = delete;

//    void writeEvent(const QJsonObject& event);
//    qint64 elapsedMs(const QDateTime& current) const;

//private:
//    QFile m_file;
//    QTextStream m_stream;
//    QMutex m_mutex;
//    bool m_isRecording;
//    bool m_firstEvent;
//    QDateTime m_startTime;
//};



// 上一个版本1，已保存，已经能够写入文件，但是需要更新json的格式化输出
//#pragma once
//#include <QObject>
//#include <QFile>
//#include <QTextStream>
//#include <QMutex>
//#include <QDateTime>
//#include <QJsonObject>
//#include <QJsonDocument>
//#include "captureengine.h"

//class Recorder : public QObject
//{
//    Q_OBJECT
//public:
//    static Recorder& instance()
//    {
//        static Recorder instance;
//        return instance;
//    }

//    bool startRecording(const QString& filePath);
//    void stopRecording();
//    bool isRecording() const { return m_isRecording; }

//public slots:
//    void onMouseEventCaptured(const MouseEventData& data);
//    void onKeyEventCaptured(const KeyEventData& data);

//private:
//    Recorder();
//    ~Recorder();
//    Recorder(const Recorder&) = delete;
//    Recorder& operator=(const Recorder&) = delete;

//    void writeJsonLine(const QJsonObject& obj);

//private:
//    QFile m_file;
//    QTextStream m_stream;
//    QMutex m_mutex;
//    bool m_isRecording;
//};


//#pragma once
//#include <QObject>
//#include <QFile>
//#include <QDataStream>
//#include <QMutex>
//#include <QDateTime>
//#include "captureengine.h"

//class Recorder : public QObject
//{
//    Q_OBJECT
//public:
//    static Recorder& instance()
//    {
//        static Recorder instance;
//        return instance;
//    }

//    bool startRecording(const QString& filePath);
//    void stopRecording();
//    bool isRecording() const { return m_isRecording; }

//public slots:
//    void onMouseEventCaptured(const MouseEventData& data);
//    void onKeyEventCaptured(const KeyEventData& data);

//private:
//    Recorder();
//    ~Recorder();
//    Recorder(const Recorder&) = delete;
//    Recorder& operator=(const Recorder&) = delete;

//    void writeMouseEvent(const MouseEventData& data);
//    void writeKeyEvent(const KeyEventData& data);

//private:
//    QFile m_file;
//    QDataStream m_stream;
//    QMutex m_mutex;
//    bool m_isRecording;
//};



//#include <QObject>
//#include <QFile>
//#include <QJsonArray>
//#include <QJsonObject>
//#include <QJsonDocument>
//#include <QDateTime>
//#include <QMutex>
//#include <QDebug>

//#include "captureengine.h"

///**
// * @brief Recorder：负责将 MouseEventData / KeyEventData 写入 JSON 文件
// */
//class Recorder : public QObject
//{
//    Q_OBJECT
//public:
//    explicit Recorder(QObject *parent = nullptr);

//    bool startRecording(const QString &filePath);
//    void stopRecording();
//    bool isRecording() const { return m_isRecording; }

//public slots:
//    void onMouseCaptured(const MouseEventData &data);
//    void onKeyCaptured(const KeyEventData &data);

//private:
//    void saveToFile();

//private:
//    QFile m_file;
//    QJsonArray m_events;
//    QDateTime m_startTime;
//    QMutex m_mutex;
//    bool m_isRecording = false;
//};

#endif // RECORDER_H




//#ifndef RECORDER_H
//#define RECORDER_H

//#include <QObject>

//class recorder : public QObject
//{
//    Q_OBJECT
//public:
//    explicit recorder(QObject *parent = nullptr);

//signals:

//};

//#endif // RECORDER_H
