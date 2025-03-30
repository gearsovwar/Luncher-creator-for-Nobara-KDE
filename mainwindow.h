#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void refreshLauncherList();

    // Scans a dir
    void refreshDesktopList(const QString &directoryPath, class QListWidget *listWidget);

private slots:
    void on_pushButtonSelectIcon_clicked();
    void on_pushButtonSearchApps_clicked();
    void on_pushButtonCreateDesktop_clicked();
    void on_pushButtonRemoveLauncher_clicked();
    void on_lineEditSearchLaunchers_textChanged(const QString &text);

private:
    // pulls the exec
    QString getExecCommandFromDesktopFile(const QString &desktopFilePath);

    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
