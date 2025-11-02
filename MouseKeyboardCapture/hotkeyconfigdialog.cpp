#include "hotkeyconfigdialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>

HotkeyConfigDialog::HotkeyConfigDialog(QWidget *parent)
    : QDialog(parent), settings_("MyCompany","MouseKeyboardCapture")
{
    ksStop_ = new QKeySequenceEdit;
    ksPause_ = new QKeySequenceEdit;
    ksResume_ = new QKeySequenceEdit;
    ksSpeed_ = new QKeySequenceEdit;
    btnApply_ = new QPushButton(tr("Apply and Register"));

    QFormLayout *form = new QFormLayout;
    form->addRow(tr("end replay:"), ksStop_);
    form->addRow(tr("stop replay:"), ksPause_);
    form->addRow(tr("resume replay:"), ksResume_);
    form->addRow(tr("speed up replay:"), ksSpeed_);

    QVBoxLayout *main = new QVBoxLayout;
    main->addLayout(form);
    main->addWidget(btnApply_);
    setLayout(main);

    loadSettings();
    connect(btnApply_, &QPushButton::clicked, this, &HotkeyConfigDialog::onApply);
}

HotkeyConfigDialog::~HotkeyConfigDialog()
{
}

void HotkeyConfigDialog::loadSettings()
{
    ksStop_->setKeySequence(QKeySequence::fromString(settings_.value("hotkeys/stop","Ctrl+Alt+S").toString()));
    ksPause_->setKeySequence(QKeySequence::fromString(settings_.value("hotkeys/pause","Ctrl+Alt+P").toString()));
    ksResume_->setKeySequence(QKeySequence::fromString(settings_.value("hotkeys/resume","Ctrl+Alt+R").toString()));
    ksSpeed_->setKeySequence(QKeySequence::fromString(settings_.value("hotkeys/speed","Ctrl+Alt+F").toString()));
}

void HotkeyConfigDialog::saveSettings()
{
    settings_.setValue("hotkeys/stop", ksStop_->keySequence().toString());
    settings_.setValue("hotkeys/pause", ksPause_->keySequence().toString());
    settings_.setValue("hotkeys/resume", ksResume_->keySequence().toString());
    settings_.setValue("hotkeys/speed", ksSpeed_->keySequence().toString());
    settings_.sync();
}

void HotkeyConfigDialog::onApply()
{
    // unregister all first
    GlobalHotkeyManager::instance().unregisterAll();

    bool anyFailed = false;
    if (!ksStop_->keySequence().isEmpty()) {
        if (!GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::StopReplay, ksStop_->keySequence())) anyFailed = true;
    }
    if (!ksPause_->keySequence().isEmpty()) {
        if (!GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::PauseReplay, ksPause_->keySequence())) anyFailed = true;
    }
    if (!ksResume_->keySequence().isEmpty()) {
        if (!GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::ResumeReplay, ksResume_->keySequence())) anyFailed = true;
    }
    if (!ksSpeed_->keySequence().isEmpty()) {
        if (!GlobalHotkeyManager::instance().registerHotkey(GlobalHotkeyManager::SpeedUpReplay, ksSpeed_->keySequence())) anyFailed = true;
    }

    saveSettings();

    if (anyFailed) {
        QMessageBox::warning(this, tr("prompt"), tr("Some hotkey registrations failed (they may have been taken by other programs)."));
    }
    else {
        QMessageBox::information(this, tr("prompt"), tr("The hotkey has been registered."));
    }
}

