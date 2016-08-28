#pragma once

#include "Precompiled.h"
#include "ui_Preferences.h"

class Preferences : public QDialog
{
    Q_OBJECT
public:
    explicit Preferences(QWidget* parent);
    ~Preferences();

private:
    Ui::Preferences ui;
};
