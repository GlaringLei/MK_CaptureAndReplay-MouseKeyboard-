#include "captureengine.h"
#include <QDebug>
#include <QCoreApplication>
#include <windowsx.h>

#pragma comment(lib, "user32.lib")

// ======================== 单例实现 ========================
// 整个进程只有一个引擎，任何地方都能 instance() 拿到它
CaptureEngine& CaptureEngine::instance()
{
    // 单例对象 inst 依然在第一次调用 instance() 时被创建，靠“函数内静态变量”本身就能保证唯一实例；
    // 钩子回调里再次 CaptureEngine::instance() 即可拿到同一个对象，
    // 因此根本不需要、也不应该在构造函数里把 this 额外保存到另一个静态指针
    static CaptureEngine inst;
    return inst;
}

CaptureEngine::CaptureEngine()
    : mouseHook_(nullptr),
      keyboardHook_(nullptr),
      running_(false)
{}

CaptureEngine::~CaptureEngine()
{
    stop();
}

// ======================== 启动与停止 ========================
bool CaptureEngine::start()
{
    if (running_) return true;

    HINSTANCE hInst = GetModuleHandle(nullptr);
    mouseHook_ = SetWindowsHookEx(WH_MOUSE_LL, mouseProc, hInst, 0);
    keyboardHook_ = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardProc, hInst, 0);

    if (!mouseHook_ || !keyboardHook_) {
        qWarning() << "Failed to install hooks!";
        return false;
    }

    running_ = true;
    workerThread_ = std::thread(&CaptureEngine::workerLoop, this);

    qDebug() << "CaptureEngine started.";
    return true;
}

void CaptureEngine::stop()
{
    // 确保引擎处于运行状态，避免重复停止
    if (!running_) return;
    // 设置 running_ = false，告知 workerLoop 循环应终止
    running_ = false;

    // 调用 cv_.notify_all() 唤醒可能处于阻塞等待状态的 workerLoop 线程，使其立即检查 running_ 标志
    cv_.notify_all();

    // 通过 workerThread_.join() 阻塞当前线程（调用 stop() 的线程），直到 workerLoop 函数执行完毕、工作线程终止。
    // 这一步是线程同步的关键，确保工作线程的资源（如局部变量）被正确释放，避免主线程提前销毁对象导致的内存访问错误
    if (workerThread_.joinable())
        workerThread_.join();

    // 卸载鼠标和键盘钩子（UnhookWindowsHookEx），并将钩子句柄置空，避免资源泄漏
    if (mouseHook_)
        UnhookWindowsHookEx(mouseHook_);
    if (keyboardHook_)
        UnhookWindowsHookEx(keyboardHook_);

    mouseHook_ = nullptr;
    keyboardHook_ = nullptr;

    qDebug() << "CaptureEngine stopped.";
}

// ======================== 鼠标回调 ========================
// 钩子线程只负责“最快地”把事件塞进队列；
LRESULT CALLBACK CaptureEngine::mouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && lParam) {
        MSLLHOOKSTRUCT* pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        CaptureEngine& engine = CaptureEngine::instance();

        // ====== 节流策略：减少高频WM_MOUSEMOVE ======
        QPoint pos(pMouse->pt.x, pMouse->pt.y);
        QDateTime now = QDateTime::currentDateTime();

        if (wParam == WM_MOUSEMOVE) {
            // 若移动距离小于3像素，或间隔小于5ms，则忽略
            if ((pos - engine.lastMousePos_).manhattanLength() < 3 &&
                engine.lastMouseTime_.msecsTo(now) < 5)
                return CallNextHookEx(nullptr, nCode, wParam, lParam);

            engine.lastMousePos_ = pos;
            engine.lastMouseTime_ = now;
        }

        MouseEventData data{ pos, static_cast<DWORD>(wParam), now };
        engine.enqueueMouseEvent(data);
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// ======================== 键盘回调 ========================
LRESULT CALLBACK CaptureEngine::keyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && lParam) {
        KBDLLHOOKSTRUCT* pKey = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        CaptureEngine& engine = CaptureEngine::instance();

        bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        KeyEventData data{ pKey->vkCode, isDown, QDateTime::currentDateTime() };
        engine.enqueueKeyEvent(data);
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// ======================== 队列操作 ========================
void CaptureEngine::enqueueMouseEvent(const MouseEventData& data)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        mouseQueue_.push(data);
    }
    // 唤醒工作线程（无需等待 5 毫秒超时），提高事件处理的实时性
    cv_.notify_one();
}

void CaptureEngine::enqueueKeyEvent(const KeyEventData& data)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        keyQueue_.push(data);
    }
    // 唤醒工作线程（可选，避免等待超时）
    cv_.notify_one();
}

// ======================== 后台处理线程 ========================
// 工作线程负责批量取出并用 emit 发信号；
void CaptureEngine::workerLoop()
{
    // 循环以 running_ 为标志
    while (running_) {
        // 条件变量 + 互斥锁实现阻塞等待，避免线程空转浪费 CPU,
        // 首先锁定互斥锁 queueMutex_，确保对 mouseQueue_、keyQueue_、running_ 的访问是线程安全的
        std::unique_lock<std::mutex> lock(queueMutex_);
        // 阻塞当前工作线程，等待以下三种情况之一发生：1 mouseQueue_ 或 keyQueue_ 不为空（有新事件需要处理）；
        // 2 running_ 被设为 false（线程需要退出）；3 超时（5 毫秒，避免永久阻塞）
        cv_.wait_for(lock, std::chrono::milliseconds(5), [this] {
            return !mouseQueue_.empty() || !keyQueue_.empty() || !running_;
        });
        // 等待期间，lock 会自动释放互斥锁，允许其他线程（如钩子回调线程）修改队列；当等待被唤醒或超时时，lock 会重新锁定互斥锁，确保后续操作的线程安全。


        // 使用 while 而非 if，确保一次性处理队列中所有积压的事件
        while (!mouseQueue_.empty()) {
            // 取出队首事件
            MouseEventData data = mouseQueue_.front();
            // 移除事件
            mouseQueue_.pop();
            // 临时释放锁，避免发送信号时长期占用锁，若持有锁，会阻塞钩子线程向队列中添加新事件，导致事件积压。
            lock.unlock();
            // 异步发送信号给Qt主线程，避免了直接在工作线程中操作 UI 导致的线程安全问题（Qt UI 操作必须在主线程）
            emit mouseEventCaptured(data);
            // 重新锁定，准备处理下一个事件
            lock.lock();
        }

        while (!keyQueue_.empty()) {
            KeyEventData data = keyQueue_.front();
            keyQueue_.pop();
            lock.unlock();
            emit keyEventCaptured(data);
            lock.lock();
        }
    }
}



//#include "captureengine.h"
//#include <QDebug>
//#include <QCoreApplication>
//#include <windowsx.h>

//HHOOK CaptureEngine::s_mouseHook = nullptr;
//HHOOK CaptureEngine::s_keyboardHook = nullptr;
//CaptureEngine* CaptureEngine::s_instance = nullptr;

//// 构造函数，保存 this 到 s_instance，供静态回调使用
//// 为什么要使用静态实例指针来保存this呢，这是因为 Windows 要求低层钩子（WH_MOUSE_LL / WH_KEYBOARD_LL）的回调函数必须是
//// 普通的 C 函数或类的静态成员函数，而静态成员函数没有 this 指针。静态函数里拿不到 this
//CaptureEngine::CaptureEngine(QObject *parent) : QObject(parent) {
//    s_instance = this;
//}

//CaptureEngine::~CaptureEngine() {
//    stop();
//    s_instance = nullptr;
//}

//bool CaptureEngine::start() {
//    if (m_running) return true;

//    // 使用 SetWindowsHookEx 安装 低层鼠标钩子 和 低层键盘钩子
//    // WH_MOUSE_LL 和 WH_KEYBOARD_LL 是 全局钩子，可以捕获整个系统的输入
//    // 回调函数为 MouseProc 和 KeyboardProc，必须是 静态成员函数。
//    s_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(nullptr), 0);
//    s_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(nullptr), 0);

//    // 确保两个钩子函数都成功安装，否则启动失败。
//    if (!s_mouseHook || !s_keyboardHook) {
//        qWarning() << "Failed to install hooks!";
//        return false;
//    }

//    m_running = true;
//    qDebug() << "CaptureEngine started.";
//    return true;
//}

//void CaptureEngine::stop() {
//    if (!m_running) return;

//    // 卸载钩子，释放系统资源
//    if (s_mouseHook) UnhookWindowsHookEx(s_mouseHook);
//    if (s_keyboardHook) UnhookWindowsHookEx(s_keyboardHook);

//    s_mouseHook = nullptr;
//    s_keyboardHook = nullptr;
//    m_running = false;
//    qDebug() << "CaptureEngine stopped.";
//}

//// 每次鼠标事件（移动、点击、滚轮）都会触发这个函数
//LRESULT CALLBACK CaptureEngine::MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
//    if (nCode >= 0 && s_instance) {
//        MSLLHOOKSTRUCT *pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
//        InputEvent evt;
//        evt.timestamp = QDateTime::currentMSecsSinceEpoch();
//        evt.position = QPoint(pMouse->pt.x, pMouse->pt.y);

//        // 根据 wParam 判断事件类型（如 WM_MOUSEMOVE, WM_LBUTTONDOWN 等）
//        switch (wParam) {
//        case WM_MOUSEMOVE: evt.type = InputEventType::MouseMove; evt.extraInfo = "Move"; break;
//        case WM_LBUTTONDOWN: evt.type = InputEventType::MouseClick; evt.extraInfo = "LeftDown"; break;
//        case WM_LBUTTONUP: evt.type = InputEventType::MouseClick; evt.extraInfo = "LeftUp"; break;
//        case WM_RBUTTONDOWN: evt.type = InputEventType::MouseClick; evt.extraInfo = "RightDown"; break;
//        case WM_RBUTTONUP: evt.type = InputEventType::MouseClick; evt.extraInfo = "RightUp"; break;
//        case WM_MOUSEWHEEL: evt.type = InputEventType::MouseWheel; evt.extraInfo = "Wheel"; break;
//        default: return CallNextHookEx(nullptr, nCode, wParam, lParam);
//        }

//        // 使用在静态存储区保存的单例对象来去执行该对象的成员函数，或者信号发出。
//        emit s_instance->eventCaptured(evt);
//    }
//    return CallNextHookEx(nullptr, nCode, wParam, lParam);
//}

//// 每次键盘按下/释放都会触发
//LRESULT CALLBACK CaptureEngine::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
//    if (nCode >= 0 && s_instance) {
//        KBDLLHOOKSTRUCT *pKey = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
//        InputEvent evt;
//        evt.keyCode = static_cast<int>(pKey->vkCode);
//        evt.timestamp = QDateTime::currentMSecsSinceEpoch();

//        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
//            evt.type = InputEventType::KeyDown;
//            evt.keyName = QKeySequence(evt.keyCode).toString();
//            evt.extraInfo = "Down";
//        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
//            evt.type = InputEventType::KeyUp;
//            evt.keyName = QKeySequence(evt.keyCode).toString();
//            evt.extraInfo = "Up";
//        } else return CallNextHookEx(nullptr, nCode, wParam, lParam);

//        emit s_instance->eventCaptured(evt);
//    }
//    return CallNextHookEx(nullptr, nCode, wParam, lParam);
//}
