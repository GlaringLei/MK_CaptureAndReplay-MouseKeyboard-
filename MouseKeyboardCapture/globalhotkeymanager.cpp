#include "globalhotkeymanager.h"
#include <QApplication>
#include <QDebug>

GlobalHotkeyManager& GlobalHotkeyManager::instance()
{
    static GlobalHotkeyManager inst(qApp);
    return inst;
}

GlobalHotkeyManager::GlobalHotkeyManager(QObject* parent)
    : QObject(parent)
{
    qApp->installNativeEventFilter(this);
}

GlobalHotkeyManager::~GlobalHotkeyManager()
{
    unregisterAll();
    qApp->removeNativeEventFilter(this);
}

bool GlobalHotkeyManager::registerHotkey(HotkeyAction action, const QKeySequence& seq)
{
    QMutexLocker locker(&m_mutex);
    // if previously registered, remove first
    if (m_actionToId.contains(action)) {
        unregisterNativeHotkey(m_actionToId[action]);
        m_actionToId.remove(action);
    }

    int nativeId = registerNativeHotkey(seq, action);
    if (nativeId == 0) {
        qWarning() << "[GlobalHotkeyManager] 注册热键失败:" << seq.toString();
        return false;
    }

    m_idToAction[nativeId] = action;
    m_actionToId[action] = nativeId;
    qDebug() << "[GlobalHotkeyManager] 注册热键成功:" << seq.toString() << "id=" << nativeId;
    return true;
}

void GlobalHotkeyManager::unregisterHotkey(HotkeyAction action)
{
    QMutexLocker locker(&m_mutex);
    if (!m_actionToId.contains(action)) return;
    int id = m_actionToId[action];
    unregisterNativeHotkey(id);
    m_actionToId.remove(action);
    m_idToAction.remove(id);
}

void GlobalHotkeyManager::unregisterAll()
{
    QMutexLocker locker(&m_mutex);
    QList<int> ids = m_idToAction.keys();
    for (int id : ids) unregisterNativeHotkey(id);
    m_idToAction.clear();
    m_actionToId.clear();
}

int GlobalHotkeyManager::registerNativeHotkey(const QKeySequence &seq, HotkeyAction /*action*/)
{
#ifdef Q_OS_WIN
    if (seq.isEmpty()) return 0;

    // Qt5: use seq[0]
    int keyInt = seq[0]; // contains modifiers in high bits
    Qt::KeyboardModifiers mods = Qt::KeyboardModifiers(keyInt & Qt::KeyboardModifierMask);
    int qtKey = keyInt & ~Qt::KeyboardModifierMask;

    UINT fsModifiers = 0;
    if (mods & Qt::ControlModifier) fsModifiers |= MOD_CONTROL;
    if (mods & Qt::AltModifier)     fsModifiers |= MOD_ALT;
    if (mods & Qt::ShiftModifier)   fsModifiers |= MOD_SHIFT;
    if (mods & Qt::MetaModifier)    fsModifiers |= MOD_WIN;

    // Convert Qt key to virtual-key. For common keys the numeric value matches VK_.
    // Fallback to qtKey value.
    UINT vk = static_cast<UINT>(qtKey);
    // Special-case: Qt::Key_Space is 0x20, Qt::Key_A = 0x41 etc. Should work.
    // RegisterHotKey expects a VK code.
    int id = m_nextId++;
    if (!RegisterHotKey(nullptr, id, fsModifiers, vk)) {
        DWORD err = GetLastError();
        Q_UNUSED(err)
        return 0;
    }
    return id;
#else
    Q_UNUSED(seq)
    return 0;
#endif
}

void GlobalHotkeyManager::unregisterNativeHotkey(int id)
{
#ifdef Q_OS_WIN
    UnregisterHotKey(nullptr, id);
#else
    Q_UNUSED(id)
#endif
}

bool GlobalHotkeyManager::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            int id = static_cast<int>(msg->wParam);
            QMutexLocker locker(&m_mutex);
            if (m_idToAction.contains(id)) {
                HotkeyAction act = m_idToAction.value(id);
                emit hotkeyPressed(act);
                if (result) *result = 0;
                return true;
            }
        }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return false;
}


//------------------------------------------------------------------------------------


//#include "globalhotkeymanager.h"
//#include <QCoreApplication>

//GlobalHotkeyManager::GlobalHotkeyManager(QObject* parent) : QObject(parent) {
//    qApp->installNativeEventFilter(this);
//}

//GlobalHotkeyManager::~GlobalHotkeyManager() {
//    unregisterAll();
//}

//GlobalHotkeyManager& GlobalHotkeyManager::instance() {
//    static GlobalHotkeyManager mgr;
//    return mgr;
//}

//bool GlobalHotkeyManager::registerHotkey(int id, HotkeyCommand cmd, const QKeySequence& seq) {
//#ifdef _WIN32
//    UINT mod = 0;
//    UINT vk = 0;

//    QString keyStr = seq.toString().toUpper();
//    if (keyStr.contains("CTRL")) mod |= MOD_CONTROL;
//    if (keyStr.contains("SHIFT")) mod |= MOD_SHIFT;
//    if (keyStr.contains("ALT")) mod |= MOD_ALT;

//    QStringList parts = keyStr.split('+');
//    QString lastKey = parts.last().trimmed();
//    if (lastKey.length() == 1)
//        vk = VkKeyScan(lastKey.at(0).toLatin1()) & 0xFF;
//    else
//        vk = VkKeyScan(lastKey.at(0).toLatin1()) & 0xFF;

//    if (!RegisterHotKey(NULL, id, mod, vk)) {
//        qWarning() << "[GlobalHotkeyManager] Failed to register hotkey: " << seq.toString()
//                   << "May be occupied by another program";
//        return false;
//    }

//    hotkeyMap_[id] = cmd;
//    qDebug() << "[GlobalHotkeyManager] Registration successful:" << seq.toString();
//#endif
//    return true;
//}

//void GlobalHotkeyManager::unregisterAll() {
//#ifdef _WIN32
//    for (auto id : hotkeyMap_.keys()) {
//        UnregisterHotKey(NULL, id);
//    }
//    hotkeyMap_.clear();
//#endif
//}

//bool GlobalHotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, long* result) {
//#ifdef _WIN32
//    MSG* msg = static_cast<MSG*>(message);
//    if (msg->message == WM_HOTKEY) {
//        int id = (int)msg->wParam;
//        if (hotkeyMap_.contains(id))
//            emit hotkeyActivated(hotkeyMap_[id]);
//    }
//#endif
//    Q_UNUSED(eventType)
//    Q_UNUSED(result)
//    return false;
//}



//------------------------------------------------------------------------------------

//#include "globalhotkeymanager.h"
//#include <QDebug>
//#include <QSettings>
//#include <windows.h>

//GlobalHotkeyManager::GlobalHotkeyManager(QObject *parent)
//    : QObject(parent)
//{
//    qApp->installNativeEventFilter(this);

//    // 默认热键
//    registerHotkey("stop", MOD_CONTROL | MOD_SHIFT, 'E');
//    registerHotkey("pause", MOD_CONTROL | MOD_SHIFT, 'S');
//    registerHotkey("continue", MOD_CONTROL | MOD_SHIFT, 'C');
//    registerHotkey("speedup", MOD_CONTROL | MOD_SHIFT, 'A');
//}

//GlobalHotkeyManager::~GlobalHotkeyManager()
//{
//    unregisterHotkeys();
//}

//bool GlobalHotkeyManager::tryRegisterHotkey(int id, int modifiers, int key, const QString &action)
//{
//    if (!RegisterHotKey(nullptr, id, modifiers, key)) {
//        DWORD err = GetLastError();
//        if (err == ERROR_HOTKEY_ALREADY_REGISTERED) {
//            qWarning() << QString("[警告] 快捷键注册失败：%1 已被占用 (mod=%2 key=%3)")
//                          .arg(action).arg(modifiers).arg(key);
//        } else {
//            qWarning() << QString("[错误] 注册快捷键失败：%1, 错误代码=%2")
//                          .arg(action).arg(err);
//        }
//        return false;
//    }
//    return true;
//}

//bool GlobalHotkeyManager::registerHotkey(const QString &action, int modifiers, int key)
//{
//    int id = nextId_++;
//    if (tryRegisterHotkey(id, modifiers, key, action)) {
//        hotkeys_[id] = {id, modifiers, key, action};
//        qDebug() << QString("[注册成功] %1 (%2+%3)").arg(action).arg(modifiers).arg(key);
//        return true;
//    }
//    return false;
//}

//void GlobalHotkeyManager::unregisterHotkeys()
//{
//    for (auto it = hotkeys_.begin(); it != hotkeys_.end(); ++it) {
//        UnregisterHotKey(nullptr, it->id);
//    }
//    hotkeys_.clear();
//    qDebug() << "[信息] 所有全局热键已注销";
//}

//bool GlobalHotkeyManager::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
//{
//    if (eventType == "windows_generic_MSG") {
//        MSG *msg = static_cast<MSG*>(message);
//        if (msg->message == WM_HOTKEY) {
//            int id = static_cast<int>(msg->wParam);
//            if (hotkeys_.contains(id)) {
//                QString action = hotkeys_[id].action;
//                if (action == "stop") emit stopTriggered();
//                else if (action == "pause") emit pauseTriggered();
//                else if (action == "continue") emit continueTriggered();
//                else if (action == "speedup") emit speedUpTriggered();
//            }
//            *result = 0;
//            return true;
//        }
//    }
//    return false;
//}

//void GlobalHotkeyManager::reloadHotkeysFromConfig()
//{
//    QSettings settings("config.ini", QSettings::IniFormat);
//    unregisterHotkeys();

//    QStringList actions = {"stop", "pause", "continue", "speedup"};
//    for (const QString &action : actions) {
//        int mod = settings.value(action + "/modifiers", MOD_CONTROL | MOD_SHIFT).toInt();
//        int key = settings.value(action + "/key", 0).toInt();
//        if (key > 0) registerHotkey(action, mod, key);
//        else qWarning() << "[警告] 无效的热键定义：" << action;
//    }
//}

//------------------------------------------------------------------------------------

//#include "globalhotkeymanager.h"
//#include <QDebug>

//GlobalHotkeyManager& GlobalHotkeyManager::instance()
//{
//    static GlobalHotkeyManager inst;
//    return inst;
//}

//GlobalHotkeyManager::GlobalHotkeyManager(QObject* parent)
//    : QObject(parent)
//{
//    qApp->installNativeEventFilter(this);
//}

//GlobalHotkeyManager::~GlobalHotkeyManager()
//{
//    unregisterHotkeys();
//}

//void GlobalHotkeyManager::registerHotkeys()
//{
//    // Ctrl+Shift+E -> Stop
//    RegisterHotKey(nullptr, HOTKEY_STOP, MOD_CONTROL | MOD_SHIFT, 0x45);
//    // Ctrl+Shift+S -> Pause
//    RegisterHotKey(nullptr, HOTKEY_PAUSE, MOD_CONTROL | MOD_SHIFT, 0x53);
//    // Ctrl+Shift+C -> Continue
//    RegisterHotKey(nullptr, HOTKEY_CONTINUE, MOD_CONTROL | MOD_SHIFT, 0x43);
//    // Ctrl+Shift+A -> Speed x2
//    RegisterHotKey(nullptr, HOTKEY_SPEEDUP, MOD_CONTROL | MOD_SHIFT, 0x41);

//    qDebug() << "[GlobalHotkeyManager] Global hotkey has been registered";
//}

//void GlobalHotkeyManager::unregisterHotkeys()
//{
//    UnregisterHotKey(nullptr, HOTKEY_STOP);
//    UnregisterHotKey(nullptr, HOTKEY_PAUSE);
//    UnregisterHotKey(nullptr, HOTKEY_CONTINUE);
//    UnregisterHotKey(nullptr, HOTKEY_SPEEDUP);
//    qDebug() << "[GlobalHotkeyManager] 全局热键已注销";
//}

//bool GlobalHotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, long* result)
//{
//#ifdef Q_OS_WIN
//    if (eventType == "windows_generic_MSG") {
//        MSG* msg = static_cast<MSG*>(message);
//        if (msg->message == WM_HOTKEY) {
//            handleHotkey(msg->wParam);
//            *result = 0;
//            return true;
//        }
//    }
//#else
//    Q_UNUSED(eventType)
//    Q_UNUSED(message)
//    Q_UNUSED(result)
//#endif
//    return false;
//}

//void GlobalHotkeyManager::handleHotkey(WPARAM wParam)
//{
//    auto& replay = ReplayManager::instance();
//    switch (wParam) {
//    case HOTKEY_STOP:
//        qDebug() << "[GlobalHotkey] Ctrl+Shift+E -> Stop";
//        replay.stopReplay();
//        break;
//    case HOTKEY_PAUSE:
//        qDebug() << "[GlobalHotkey] Ctrl+Shift+S -> Pause";
//        replay.pauseReplay();
//        break;
//    case HOTKEY_CONTINUE:
//        qDebug() << "[GlobalHotkey] Ctrl+Shift+C -> Continue";
//        replay.resumeReplay();
//        break;
//    case HOTKEY_SPEEDUP:
//        qDebug() << "[GlobalHotkey] Ctrl+Shift+A -> Speed x2";
//        replay.setSpeedMultiplier(2.0);
//        break;
//    default:
//        break;
//    }
//}

