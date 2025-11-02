#ifndef GLOBALHOTKEYMANAGER_H
#define GLOBALHOTKEYMANAGER_H

#pragma once
#include <QObject>
#include <QKeySequence>
#include <QHash>
#include <QMutex>
#include <QAbstractNativeEventFilter>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class GlobalHotkeyManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    enum HotkeyAction {
        StopReplay,
        PauseReplay,
        ResumeReplay,
        SpeedUpReplay
    };
    Q_ENUM(HotkeyAction)

    static GlobalHotkeyManager& instance();

    // 注册热键（如果已经为该 action 注册会先注销旧的）
    // 返回 true 表示注册成功，false 表示失败（可能被其它程序占用）
    bool registerHotkey(HotkeyAction action, const QKeySequence& seq);
    void unregisterHotkey(HotkeyAction action);
    void unregisterAll();

signals:
    void hotkeyPressed(GlobalHotkeyManager::HotkeyAction action);

protected:
    // Qt5 签名：long* result
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

private:
    explicit GlobalHotkeyManager(QObject* parent = nullptr);
    ~GlobalHotkeyManager();

    int registerNativeHotkey(const QKeySequence &seq, HotkeyAction action);
    void unregisterNativeHotkey(int id);

private:
    QHash<int, HotkeyAction> m_idToAction;     // nativeId -> action
    QHash<HotkeyAction, int> m_actionToId;     // action -> nativeId
    QMutex m_mutex;
    int m_nextId = 1;
};


#endif // GLOBALHOTKEYMANAGER_H

//------------------------------------------------------------------------------------


//#pragma once
//#include <QObject>
//#include <QAbstractNativeEventFilter>
//#include <QMap>
//#include <QKeySequence>
//#include <QDebug>
//#include "hotkeycommand.h"

//#ifdef _WIN32
//#include <windows.h>
//#endif

//class GlobalHotkeyManager : public QObject, public QAbstractNativeEventFilter {
//    Q_OBJECT
//public:
//    static GlobalHotkeyManager& instance();
//    bool registerHotkey(int id, HotkeyCommand cmd, const QKeySequence& sequence);
//    void unregisterAll();

//signals:
//    void hotkeyActivated(HotkeyCommand cmd);

//protected:
//    bool nativeEventFilter(const QByteArray& eventType, void* message, long* result) override;

//private:
//    explicit GlobalHotkeyManager(QObject* parent = nullptr);
//    ~GlobalHotkeyManager();
//    QMap<int, HotkeyCommand> hotkeyMap_;
//};



//------------------------------------------------------------------------------------

//#pragma once
//#include <QObject>
//#include <QAbstractNativeEventFilter>
//#include <QApplication>
//#include <QMap>
//#include <QString>
//#include <windows.h>

//class GlobalHotkeyManager : public QObject, public QAbstractNativeEventFilter {
//    Q_OBJECT

//public:
//    explicit GlobalHotkeyManager(QObject *parent = nullptr);
//    ~GlobalHotkeyManager();

//    bool registerHotkey(const QString &action, int modifiers, int key);
//    void unregisterHotkeys();
//    void reloadHotkeysFromConfig();

//signals:
//    void stopTriggered();
//    void pauseTriggered();
//    void continueTriggered();
//    void speedUpTriggered();

//protected:
//    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;

//private:
//    struct HotkeyInfo {
//        int id;
//        int modifiers;
//        int key;
//        QString action;
//    };

//    QMap<int, HotkeyInfo> hotkeys_;
//    int nextId_ = 1;

//    bool tryRegisterHotkey(int id, int modifiers, int key, const QString &action);
//};

//------------------------------------------------------------------------------------

//#ifndef GLOBALHOTKEYMANAGER_H
//#define GLOBALHOTKEYMANAGER_H

//#include <QObject>
//#include <QAbstractNativeEventFilter>
//#include <QApplication>
//#include <windows.h>
//#include "replaymanager.h"

//class GlobalHotkeyManager : public QObject, public QAbstractNativeEventFilter
//{
//    Q_OBJECT
//public:
//    static GlobalHotkeyManager& instance();

//    void registerHotkeys();   // 注册所有热键
//    void unregisterHotkeys(); // 注销所有热键

//    //为了解决后期的兼容性问题
//    #if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
//    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;
//    #else
//    // 拦截全局消息
//    bool nativeEventFilter(const QByteArray& eventType, void* message, long* result) override;
//    #endif

//private:
//    explicit GlobalHotkeyManager(QObject* parent = nullptr);
//    ~GlobalHotkeyManager();

//    void handleHotkey(WPARAM wParam);

//private:
//    // 热键 ID 定义
//    enum {
//        HOTKEY_STOP = 1,
//        HOTKEY_PAUSE = 2,
//        HOTKEY_CONTINUE = 3,
//        HOTKEY_SPEEDUP = 4
//    };
//};

//#endif // GLOBALHOTKEYMANAGER_H

