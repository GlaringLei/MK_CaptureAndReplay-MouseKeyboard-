#ifndef CAPTUREENGINE_H
#define CAPTUREENGINE_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QPoint>
#include <QDateTime>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <Windows.h>

/**
 * @brief 鼠标事件数据结构
 */
struct MouseEventData {
    QPoint pos;        // 鼠标坐标
    DWORD type;        // 消息类型（WM_MOUSEMOVE 等）
    QDateTime time;    // 捕获时间
};

/**
 * @brief 键盘事件数据结构
 */
struct KeyEventData {
    DWORD vkCode;      // 虚拟键码
    bool keyDown;      // true=按下，false=抬起
    QDateTime time;
};

class CaptureEngine : public QObject
{
    Q_OBJECT
public:
    static CaptureEngine& instance();

    bool start();       // 启动钩子
    void stop();        // 停止钩子

    // 鼠标事件入队列
    void enqueueMouseEvent(const MouseEventData& data);
    // 键盘事件入队列
    void enqueueKeyEvent(const KeyEventData& data);

signals:
    void mouseEventCaptured(const MouseEventData& data);
    void keyEventCaptured(const KeyEventData& data);

private:
    CaptureEngine();
    ~CaptureEngine();

    void workerLoop(); // 后台处理线程函数

    static LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

private:
    HHOOK mouseHook_;
    HHOOK keyboardHook_;
    std::atomic<bool> running_;

    std::thread workerThread_;
    std::mutex queueMutex_;
    std::condition_variable cv_;

    std::queue<MouseEventData> mouseQueue_;
    std::queue<KeyEventData> keyQueue_;

    QPoint lastMousePos_;
    QDateTime lastMouseTime_;
};

#endif // CAPTUREENGINE_H




// 版本1，以上版本2为了解决鼠标事件捕获卡顿问题，优化钩子函数，引入异步队列处理
//#ifndef CAPTUREENGINE_H
//#define CAPTUREENGINE_H

//#pragma once
//#include <QObject>
//#include <QThread>
//#include <QDateTime>
//#include <Windows.h>
//#include <QKeySequence>

//// 事件类型
//enum class InputEventType {
//    MouseMove,
//    MouseClick,
//    MouseWheel,
//    KeyDown,
//    KeyUp
//};

//// 通用事件结构体 所有捕获到的输入都会封装成这个结构体并通过信号发出
//struct InputEvent {
//    InputEventType type;
//    QPoint position;      // 鼠标坐标
//    int keyCode = 0;      // 键盘虚拟键码
//    QString keyName;
//    qint64 timestamp;     // 时间戳(ms)
//    QString extraInfo;    // 附加说明
//};

//// CaptureEngine 类定义
//class CaptureEngine : public QObject {
//    Q_OBJECT
//public:
//    explicit CaptureEngine(QObject *parent = nullptr);
//    ~CaptureEngine();

//    // 安装钩子，开始监听
//    bool start();
//    // 卸载钩子，停止监听
//    void stop();
//    // 返回当前是否正在运行
//    bool isRunning() const { return m_running; }

//signals:
//    // 每捕获一个输入事件，就会发出这个信号，外部（如 MainWindow）可以连接并处理
//    void eventCaptured(const InputEvent &event);

//private:
//    // 因为 Windows 钩子回调必须是 全局函数或静态成员函数，所以需要静态存储
//    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
//    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

//    // 使用 静态变量 保存钩子句柄和单例指针
//    // 静态回调没有 this，只好把 this 先存到全局静态指针 s_instance，才能在钩子回调里把事件发回给正确的对象。
//    static HHOOK s_mouseHook;
//    static HHOOK s_keyboardHook;
//    static CaptureEngine *s_instance;

//    bool m_running = false;
//};

//#endif // CAPTUREENGINE_H
