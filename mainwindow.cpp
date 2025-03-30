#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QListWidgetItem>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QSet>
#include <QIcon>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    refreshLauncherList();
}

MainWindow::~MainWindow()
{
    delete ui;
}

QString MainWindow::getExecCommandFromDesktopFile(const QString &desktopFilePath)
{
    QString execCommand;
    QFile file(desktopFilePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith("Exec=")) {
                execCommand = line.mid(5).trimmed(); // Remove prefix if it does showup
                break;
            }
        }
        file.close();
    }
    return execCommand;
}

void MainWindow::on_pushButtonSelectIcon_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select Icon"),
                                                    QDir::homePath(),
                                                    tr("Images (*.png *.jpg *.jpeg *.ico)"));
    if (!fileName.isEmpty())
    {
        ui->labelIconOutput->setText(fileName);
    }
}

void MainWindow::on_pushButtonSearchApps_clicked()
{
    // Create a dialog
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Select Application(s)"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // Add a search bar
    QLineEdit *searchEdit = new QLineEdit(&dialog);
    searchEdit->setPlaceholderText(tr("Search applications..."));
    layout->addWidget(searchEdit);

    // Create a list widget amd add checkboxes so users can check them instead of ctrl+click
    QListWidget *listWidget = new QListWidget(&dialog);
    listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(listWidget);

    // Dialog buttons.
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Directories to scan might add the option to configure additional ones
    QStringList directories;
    directories << QDir::homePath() + "/.local/share/applications"
                << "/usr/share/applications"
                << QDir::homePath() + "/.local/share/flatpak/exports/share/applications"
                << "/var/lib/flatpak/exports/share/applications";
    QSet<QString> seen;
    QList<QListWidgetItem*> allItems;  // Store items for filtering

    // Scan each directory.
    for (const QString &dirPath : directories) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;
        QStringList desktopFiles = dir.entryList(QStringList() << "*.desktop", QDir::Files);
        for (const QString &fileName : desktopFiles) {
            QString filePath = dir.absoluteFilePath(fileName);
            if (seen.contains(filePath))
                continue;
            seen.insert(filePath);

            bool isFlatpak = dirPath.contains("flatpak", Qt::CaseInsensitive);
            QString displayText = fileName;
            displayText += isFlatpak ? " [Flatpak]" : " [Standard]";

            // Extract the icon field, doesnt need to be done but i think it looks cleaner
            QString iconName;
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                while (!in.atEnd()){
                    QString line = in.readLine();
                    if(line.startsWith("Icon=")){
                        iconName = line.mid(5).trimmed();
                        break;
                    }
                }
                file.close();
            }
            QIcon icon;
            if (QFile::exists(iconName))
                icon = QIcon(iconName);
            else
                icon = QIcon::fromTheme(iconName);

            // Create a checkable list widget item.
            QListWidgetItem *item = new QListWidgetItem(displayText);
            if (!icon.isNull())
                item->setIcon(icon);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            item->setData(Qt::UserRole, filePath);
            listWidget->addItem(item);
            allItems.append(item);
        }
    }

    // Filter based on search
    connect(searchEdit, &QLineEdit::textChanged, [&allItems](const QString &text){
        for (QListWidgetItem *item : allItems) {
            bool match = item->text().contains(text, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    });

    // Execute the dialog.
    if (dialog.exec() == QDialog::Accepted) {
        QStringList selectedFiles;
        for (int i = 0; i < listWidget->count(); i++) {
            QListWidgetItem *item = listWidget->item(i);
            if (item->checkState() == Qt::Checked)
                selectedFiles << item->data(Qt::UserRole).toString();
        }
        if(!selectedFiles.isEmpty())
            ui->textEditAppOutput->setPlainText(selectedFiles.join("\n"));
    }
}


void MainWindow::on_pushButtonCreateDesktop_clicked()
{
    QString desktopName = ui->lineEditDesktopName->text().trimmed();
    if(desktopName.isEmpty()) {
        ui->labelCreationOutput->setText("Please enter a name for your launcher.");
        return;
    }

    QString iconPath = ui->labelIconOutput->text().trimmed();
    if(iconPath == "No icon selected")
        iconPath = "";

    QString appList = ui->textEditAppOutput->toPlainText().trimmed();
    if(appList.isEmpty() || appList == "No application selected") {
        ui->labelCreationOutput->setText("Please select at least one application.");
        return;
    }

    QString homeDir = QDir::homePath();

    // Create a shell script that launches the selected applications.
    QString scriptFilePath = homeDir + "/launch_" + desktopName + ".sh";
    QFile scriptFile(scriptFilePath);
    if(scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&scriptFile);
        out << "#!/bin/bash\n";
        QStringList desktopFiles = appList.split("\n", Qt::SkipEmptyParts);
        for (const QString &desktopPath : desktopFiles) {
            QString execCommand = getExecCommandFromDesktopFile(desktopPath);
            execCommand.replace("%U", "");
            execCommand.replace("%u", "");
            execCommand.replace("%F", "");
            execCommand.replace("%f", "");
            execCommand = execCommand.trimmed();
            if (!execCommand.isEmpty())
                out << execCommand << " &\n";
        }
        scriptFile.close();
        scriptFile.setPermissions(scriptFile.permissions() | QFileDevice::ExeOwner);
    } else {
        ui->labelCreationOutput->setText("Error: Could not create shell script.");
        return;
    }

    // Create the .desktop file in ~/.local/share/applications. Only applies to current user possibly change
    //it in the future so it saves to the global applications directory
    QString applicationsDir = QDir(homeDir + "/.local/share/applications").absolutePath();
    QDir().mkpath(applicationsDir);
    QString desktopFilePath = QDir(applicationsDir).filePath(desktopName + ".desktop");
    QFile desktopFile(desktopFilePath);
    if(desktopFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&desktopFile);
        out << "[Desktop Entry]\n";
        out << "Type=Application\n";
        out << "Name=" << desktopName << "\n";
        out << "Exec=" << scriptFilePath << "\n";
        if(!iconPath.isEmpty())
            out << "Icon=" << iconPath << "\n";
        out << "Terminal=false\n";
        out << "Categories=Utility;\n";
        // Mark the launcher as created by our tool.
        out << "X-Created-By=LauncherCreator\n";
        desktopFile.close();
        desktopFile.setPermissions(desktopFile.permissions() | QFileDevice::ExeOwner);
    } else {
        ui->labelCreationOutput->setText("Error: Could not create .desktop file.");
        return;
    }

    ui->labelCreationOutput->setText("Desktop file created at: " + desktopFilePath);
    refreshLauncherList();
}

void MainWindow::refreshLauncherList()
{
    // Directories to scan: standard and Flatpak locations maybe add more
    QStringList directories;
    directories << QDir::homePath() + "/.local/share/applications"
                << "/usr/share/applications"
                << QDir::homePath() + "/.local/share/flatpak/exports/share/applications"
                << "/var/lib/flatpak/exports/share/applications";

    ui->listWidgetLaunchers->clear();
    QSet<QString> seen;  // To avoid duplicates

    // Iterate over each directory.
    for (const QString &dirPath : directories) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;
        QStringList desktopFiles = dir.entryList(QStringList() << "*.desktop", QDir::Files);
        for (const QString &fileName : desktopFiles) {
            QString filePath = dir.absoluteFilePath(fileName);
            if (seen.contains(filePath))
                continue;
            seen.insert(filePath);

            // Determine if this file is from a Flatpak location will work for 90% of flatpaks
            bool isFlatpak = dirPath.contains("flatpak", Qt::CaseInsensitive);
            QString displayText = fileName;
            displayText += isFlatpak ? " [Flatpak]" : " [Standard]";

            // Extract icon field from the .desktop file.
            QString iconName;
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                while (!in.atEnd()){
                    QString line = in.readLine();
                    if(line.startsWith("Icon=")){
                        iconName = line.mid(5).trimmed();
                        break;
                    }
                }
                file.close();
            }
            QIcon icon;
            if (QFile::exists(iconName))
                icon = QIcon(iconName);
            else
                icon = QIcon::fromTheme(iconName);

            // Create the list widget item.
            QListWidgetItem *item = new QListWidgetItem(displayText);
            if (!icon.isNull())
                item->setIcon(icon);
            item->setData(Qt::UserRole, filePath);
            ui->listWidgetLaunchers->addItem(item);
        }
    }
}


void MainWindow::refreshDesktopList(const QString &directoryPath, QListWidget *listWidget)
{
    QDir dir(directoryPath);
    QStringList desktopFiles = dir.entryList(QStringList() << "*.desktop", QDir::Files);
    listWidget->clear();
    QSet<QString> seen;

    for (const QString &fileName : desktopFiles) {
        QString filePath = dir.absoluteFilePath(fileName);
        if(seen.contains(filePath))
            continue;
        seen.insert(filePath);

        bool isFlatpak = directoryPath.contains("flatpak", Qt::CaseInsensitive);
        QString displayText = fileName;
        displayText += isFlatpak ? " [Flatpak]" : " [Standard]";

        // Extract icon field.
        QString iconName;
        QFile file(filePath);
        if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while(!in.atEnd()){
                QString line = in.readLine();
                if(line.startsWith("Icon=")){
                    iconName = line.mid(5).trimmed();
                    break;
                }
            }
            file.close();
        }
        QIcon icon;
        if(QFile::exists(iconName))
            icon = QIcon(iconName);
        else
            icon = QIcon::fromTheme(iconName);

        QListWidgetItem *item = new QListWidgetItem(displayText);
        if(!icon.isNull())
            item->setIcon(icon);
        item->setData(Qt::UserRole, filePath);
        listWidget->addItem(item);
    }
}

void MainWindow::on_pushButtonRemoveLauncher_clicked()
{
    QListWidgetItem *item = ui->listWidgetLaunchers->currentItem();
    if (!item) {
        ui->labelCreationOutput->setText("No launcher selected for removal.");
        return;
    }

    QString filePath = item->data(Qt::UserRole).toString();
    if(QFile::remove(filePath))
        ui->labelCreationOutput->setText("Launcher removed: " + filePath);
    else
        ui->labelCreationOutput->setText("Failed to remove launcher: " + filePath);

    refreshLauncherList();
}

void MainWindow::on_lineEditSearchLaunchers_textChanged(const QString &text)
{
    for (int i = 0; i < ui->listWidgetLaunchers->count(); i++) {
        QListWidgetItem *item = ui->listWidgetLaunchers->item(i);
        if (item->text().contains(text, Qt::CaseInsensitive))
            item->setHidden(false);
        else
            item->setHidden(true);
    }
}
