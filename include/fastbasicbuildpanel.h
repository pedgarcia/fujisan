/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#ifndef FASTBASICBUILDPANEL_H
#define FASTBASICBUILDPANEL_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QProcess>
#include <QString>
#include <QLabel>

class MainWindow;

class FastbasicBuildPanel : public QWidget
{
    Q_OBJECT

public:
    explicit FastbasicBuildPanel(MainWindow* mainWindow, QWidget* parent = nullptr);
    ~FastbasicBuildPanel() override;

    void refreshFileList();
    void loadPersistedState();
    void saveFolderAndFile();

private slots:
    void onBrowseFolder();
    void onFileSelected(int index);
    void onRun();
    void onShowOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    QString resolveCompilerPath() const;
    void persistFolder(const QString& folder);
    void persistFile(const QString& fileName);
    void updatePathLabel();
    void rescanFileListKeepingSelection();
    void showOutputDialog();

    MainWindow* m_mainWindow;
    QComboBox* m_fileCombo;
    QPushButton* m_browseButton;
    QLabel* m_pathLabel;
    QPushButton* m_runButton;
    QPushButton* m_showOutputButton;
    QProcess* m_process;
    QString m_lastOutput;
    QString m_currentFolder;
};

#endif // FASTBASICBUILDPANEL_H
