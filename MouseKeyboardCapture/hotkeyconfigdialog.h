#ifndef HOTKEYCONFIGDIALOG_H
#define HOTKEYCONFIGDIALOG_H

#pragma once
#include <QDialog>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QSettings>
#include "globalhotkeymanager.h"

class HotkeyConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HotkeyConfigDialog(QWidget *parent = nullptr);
    ~HotkeyConfigDialog();

private slots:
    void onApply();
    void loadSettings();
    void saveSettings();

private:
    QKeySequenceEdit *ksStop_;
    QKeySequenceEdit *ksPause_;
    QKeySequenceEdit *ksResume_;
    QKeySequenceEdit *ksSpeed_;
    QPushButton *btnApply_;
    QSettings settings_;
};


#endif // HOTKEYCONFIGDIALOG_H
