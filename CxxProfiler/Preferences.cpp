#include "Preferences.h"
#include "Utils.h"

Preferences::Preferences(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    QSettings settings(GetSettingsFile(), QSettings::IniFormat);
    if (settings.contains("Preferences/VS2013"))
    {
        QString path = settings.value("Preferences/VS2013").toString();
        ui.txtLocation2013->setText(QDir::toNativeSeparators(path));
    }
    if (settings.contains("Preferences/VS2015"))
    {
        QString path = settings.value("Preferences/VS2015").toString();
        ui.txtLocation2015->setText(QDir::toNativeSeparators(path));
    }
    if (settings.contains("Preferences/SDK10"))
    {
        QString path = settings.value("Preferences/SDK10").toString();
        ui.txtLocationSdk10->setText(QDir::toNativeSeparators(path));
    }

    QObject::connect(ui.btnLocation2013, &QPushButton::clicked, this, [this]()
    {
        QString dir = ui.txtLocation2013->text();
        if (dir.isEmpty())
        {
            dir = "C:\\";
        }

        dir = QFileDialog::getExistingDirectory(this, "Choose VS2013 location (VC folder)", dir);
        if (!dir.isNull())
        {
            dir = QDir::toNativeSeparators(dir);
            if (QFileInfo(dir + "\\crt\\src").isDir())
            {
                ui.txtLocation2013->setText(dir);
            }
            else
            {
                QMessageBox::warning(this, "Error", QString("'%1' location does not seem to be VS2013 VC folder. Please try again!").arg(dir));
            }
        }
    });

    QObject::connect(ui.btnLocation2015, &QPushButton::clicked, this, [this]()
    {
        QString dir = ui.txtLocation2015->text();
        if (dir.isEmpty())
        {
            dir = "C:\\";
        }

        dir = QFileDialog::getExistingDirectory(this, "Choose VS2015 location (VC folder)", dir);
        if (!dir.isNull())
        {
            dir = QDir::toNativeSeparators(dir);
            if (QFileInfo(dir + "\\crt\\src").isDir())
            {
                ui.txtLocation2015->setText(dir);
            }
            else
            {
                QMessageBox::warning(this, "Error", QString("'%1' location does not seem to be VS2015 VC folder. Please try again!").arg(dir));
            }
        }
    });

    QObject::connect(ui.btnLocationSdk10, &QPushButton::clicked, this, [this]()
    {
        QString dir = ui.txtLocationSdk10->text();
        if (dir.isEmpty())
        {
            dir = "C:\\";
        }

        dir = QFileDialog::getExistingDirectory(this, "Choose Windows 10 SDK source location (10.0.xxxxx.0 folder)", dir);
        if (!dir.isNull())
        {
            dir = QDir::toNativeSeparators(dir);
            if (QFileInfo(dir + "\\ucrt").isDir())
            {
                ui.txtLocationSdk10->setText(dir);
            }
            else
            {
                QMessageBox::warning(this, "Error", QString("'%1' location does not seem to be Windows SDK source folder. Please try again!").arg(dir));
            }
        }
    });

    QObject::connect(this, &QDialog::accepted, this, [this]()
    {
        QSettings settings(GetSettingsFile(), QSettings::IniFormat);
        settings.setValue("Preferences/VS2013", QDir::fromNativeSeparators(ui.txtLocation2013->text()));
        settings.setValue("Preferences/VS2015", QDir::fromNativeSeparators(ui.txtLocation2015->text()));
        settings.setValue("Preferences/SDK10", QDir::fromNativeSeparators(ui.txtLocationSdk10->text()));
    });
}

Preferences::~Preferences()
{
}
