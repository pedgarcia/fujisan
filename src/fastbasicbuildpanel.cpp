/*
 * Fujisan - Modern Atari Emulator
 * Copyright (c) 2025 Paulo Garcia (8bitrelics.com)
 *
 * Licensed under the MIT License. See LICENSE file for details.
 */

#include "fastbasicbuildpanel.h"
#include "mainwindow.h"
#include "atariemulator.h"
#include <QBoxLayout>
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QDialog>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QApplication>
#include <QStatusBar>
#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QDebug>

static const char* KEY_FOLDER = "fastbasic/buildPanelFolder";
static const char* KEY_FILE = "fastbasic/buildPanelFile";

FastbasicBuildPanel::FastbasicBuildPanel(MainWindow* mainWindow, QWidget* parent)
    : QWidget(parent)
    , m_mainWindow(mainWindow)
    , m_process(nullptr)
{
    m_fileCombo = new QComboBox(this);
    m_fileCombo->setEditable(false);
    m_fileCombo->setMinimumWidth(120);
    m_fileCombo->setMaximumWidth(200);
    m_browseButton = new QPushButton(tr("Browse..."), this);
    m_pathLabel = new QLabel(this);
    m_pathLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    m_pathLabel->setTextFormat(Qt::PlainText);
    m_pathLabel->setMinimumWidth(80);
    m_pathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_pathLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_runButton = new QPushButton(tr("RUN"), this);
    m_showOutputButton = new QPushButton(tr("Show Output"), this);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 14, 4);  // extra right margin so last button isn't flush to border
    layout->setSpacing(12);
    layout->addWidget(new QLabel(tr("Fastbasic Compiler"), this));
    layout->addWidget(m_fileCombo);
    layout->addWidget(m_browseButton);
    layout->addWidget(m_pathLabel);
    layout->addStretch(1);
    layout->addWidget(m_runButton);
    layout->addSpacing(16);  // space between Run and Show Output
    layout->addWidget(m_showOutputButton);
    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FastbasicBuildPanel::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &FastbasicBuildPanel::onProcessError);

    connect(m_browseButton, &QPushButton::clicked, this, &FastbasicBuildPanel::onBrowseFolder);
    connect(m_fileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FastbasicBuildPanel::onFileSelected);
    connect(m_runButton, &QPushButton::clicked, this, &FastbasicBuildPanel::onRun);
    connect(m_showOutputButton, &QPushButton::clicked, this, &FastbasicBuildPanel::onShowOutput);

    setMaximumHeight(40);
    loadPersistedState();
}

FastbasicBuildPanel::~FastbasicBuildPanel()
{
}

QString FastbasicBuildPanel::resolveCompilerPath() const
{
    QSettings settings("8bitrelics", "Fujisan");
    bool useBundled = settings.value("fastbasic/useBundled", true).toBool();
    if (useBundled) {
        QString base = QCoreApplication::applicationDirPath();
#ifdef Q_OS_MACOS
        return base + "/../Resources/fastbasic/fastbasic";
#elif defined(Q_OS_WIN)
        return base + "/fastbasic/fastbasic.exe";
#else
        return base + "/fastbasic/fastbasic";
#endif
    }
    return settings.value("fastbasic/compilerPath", "").toString().trimmed();
}

void FastbasicBuildPanel::persistFolder(const QString& folder)
{
    if (folder.isEmpty()) return;
    QSettings settings("8bitrelics", "Fujisan");
    settings.setValue(KEY_FOLDER, folder);
}

void FastbasicBuildPanel::persistFile(const QString& fileName)
{
    QSettings settings("8bitrelics", "Fujisan");
    settings.setValue(KEY_FILE, fileName);
}

void FastbasicBuildPanel::updatePathLabel()
{
    if (!m_pathLabel) return;
    if (m_currentFolder.isEmpty())
        m_pathLabel->setText(tr("(no folder)"));
    else
        m_pathLabel->setText(m_currentFolder);
    m_pathLabel->setToolTip(m_currentFolder);
}

void FastbasicBuildPanel::loadPersistedState()
{
    QSettings settings("8bitrelics", "Fujisan");
    m_currentFolder = settings.value(KEY_FOLDER, "").toString();
    QString lastFile = settings.value(KEY_FILE, "").toString();
    if (!m_currentFolder.isEmpty() && QDir(m_currentFolder).exists()) {
        refreshFileList();
        int idx = m_fileCombo->findText(lastFile);
        if (idx >= 0)
            m_fileCombo->setCurrentIndex(idx);
    }
    updatePathLabel();
}

void FastbasicBuildPanel::saveFolderAndFile()
{
    if (!m_currentFolder.isEmpty())
        persistFolder(m_currentFolder);
    if (m_fileCombo->currentIndex() >= 0 && !m_fileCombo->currentText().isEmpty())
        persistFile(m_fileCombo->currentText());
}

void FastbasicBuildPanel::refreshFileList()
{
    m_fileCombo->clear();
    if (m_currentFolder.isEmpty()) return;
    QDir dir(m_currentFolder);
    QStringList filters;
    filters << "*.bas" << "*.BAS";
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);
    for (const QString& f : files)
        m_fileCombo->addItem(f);
}

void FastbasicBuildPanel::onBrowseFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select folder with .bas files"),
        m_currentFolder.isEmpty() ? QDir::homePath() : m_currentFolder);
    if (dir.isEmpty()) return;
    m_currentFolder = dir;
    persistFolder(m_currentFolder);
    refreshFileList();
    updatePathLabel();
    QSettings settings("8bitrelics", "Fujisan");
    QString lastFile = settings.value(KEY_FILE, "").toString();
    int idx = m_fileCombo->findText(lastFile);
    if (idx >= 0)
        m_fileCombo->setCurrentIndex(idx);
}

void FastbasicBuildPanel::onFileSelected(int index)
{
    if (index >= 0 && index < m_fileCombo->count())
        persistFile(m_fileCombo->itemText(index));
}

void FastbasicBuildPanel::onRun()
{
    QString compiler = resolveCompilerPath();
    if (compiler.isEmpty() || !QFile::exists(compiler)) {
        QMessageBox::warning(this, tr("Fastbasic"),
            tr("Compiler not found. Set path in Settings → Emulator → Fastbasic, or use bundled Fastbasic."));
        return;
    }
    if (m_currentFolder.isEmpty() || m_fileCombo->currentIndex() < 0) {
        QMessageBox::warning(this, tr("Fastbasic"), tr("Select a folder and a .bas file first."));
        return;
    }

    QString fileName = m_fileCombo->currentText();
    QString basPath = m_currentFolder + "/" + fileName;
    if (!QFile::exists(basPath)) {
        QMessageBox::warning(this, tr("Fastbasic"), tr("File not found: %1").arg(basPath));
        return;
    }

    m_runButton->setEnabled(false);
    m_lastOutput.clear();
    m_process->setWorkingDirectory(m_currentFolder);
    m_process->start(compiler, QStringList() << basPath);
    if (!m_process->waitForStarted(5000)) {
        m_lastOutput = tr("Failed to start compiler: %1").arg(m_process->errorString());
        QMessageBox::warning(this, tr("Fastbasic"), m_lastOutput);
        m_runButton->setEnabled(true);
        return;
    }
    m_lastOutput = tr("Compiling %1...\n").arg(fileName);
    if (m_mainWindow && m_mainWindow->statusBar())
        m_mainWindow->statusBar()->showMessage(tr("Compiling %1...").arg(fileName), 0);
}

void FastbasicBuildPanel::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_runButton->setEnabled(true);
    QByteArray out = m_process->readAllStandardOutput();
    QByteArray err = m_process->readAllStandardError();
    m_lastOutput += QString::fromUtf8(out);
    if (!err.isEmpty())
        m_lastOutput += QString::fromUtf8(err);

    QString fileName = m_fileCombo->currentText();
    QFileInfo fi(fileName);
    QString stem = fi.completeBaseName();
    QString xexInSource = m_currentFolder + "/" + stem + ".xex";
    QString binDir = m_currentFolder + "/bin";
    QString xexInBin = binDir + "/" + stem + ".xex";

    if (status != QProcess::NormalExit || exitCode != 0) {
        if (m_mainWindow && m_mainWindow->statusBar())
            m_mainWindow->statusBar()->showMessage(tr("Compile failed"), 5000);
        QMessageBox::warning(this, tr("Fastbasic"), tr("Compilation failed. Use \"Show Output\" for details."));
        return;
    }
    if (!QFile::exists(xexInSource)) {
        if (m_mainWindow && m_mainWindow->statusBar())
            m_mainWindow->statusBar()->showMessage(tr("No .xex produced"), 5000);
        QMessageBox::warning(this, tr("Fastbasic"), tr("Compiler did not produce %1.xex").arg(stem));
        return;
    }

    QDir().mkpath(binDir);
    if (QFile::exists(xexInBin))
        QFile::remove(xexInBin);
    QFile::copy(xexInSource, xexInBin);

    if (!m_mainWindow || !m_mainWindow->getEmulator()) {
        if (m_mainWindow && m_mainWindow->statusBar())
            m_mainWindow->statusBar()->showMessage(tr("XEX ready in bin/"), 3000);
        return;
    }

    // Deploy: set H4 (triggers restartEmulator), settle, then load XEX.
    // When NetSIO is enabled, do FujiNet stop→start so network is ready.
    if (m_mainWindow->statusBar())
        m_mainWindow->statusBar()->showMessage(tr("Setting H4:..."), 0);
    m_mainWindow->setHardDrivePathViaTCP(4, binDir);

    QSettings settings("8bitrelics", "Fujisan");
    bool netSIOEnabled = settings.value("media/netSIOEnabled", false).toBool();

    auto doLoadXex = [this](const QString& xexPath) {
        if (!m_mainWindow || !m_mainWindow->getEmulator()) return;
        if (m_mainWindow->statusBar())
            m_mainWindow->statusBar()->showMessage(tr("Loading XEX..."), 0);
        bool okLoad = m_mainWindow->getEmulator()->loadFile(xexPath);
        if (m_mainWindow->statusBar())
            m_mainWindow->statusBar()->showMessage(okLoad ? tr("XEX loaded.") : tr("Load failed"), 3000);
        if (!okLoad)
            QMessageBox::warning(this, tr("Fastbasic"), tr("Failed to load XEX into emulator."));
    };

    if (netSIOEnabled) {
        if (m_mainWindow->statusBar())
            m_mainWindow->statusBar()->showMessage(tr("Restarting FujiNet for network (5s)..."), 0);
        QTimer::singleShot(1500, this, [this, xexInBin, doLoadXex]() {
            if (!m_mainWindow || !m_mainWindow->getEmulator()) return;
            m_mainWindow->stopFujiNetViaTCP();
            QTimer::singleShot(2000, this, [this, xexInBin, doLoadXex]() {
                if (!m_mainWindow || !m_mainWindow->getEmulator()) return;
                m_mainWindow->startFujiNetViaTCP();
                if (m_mainWindow->statusBar())
                    m_mainWindow->statusBar()->showMessage(tr("Waiting for FujiNet (2s), then loading XEX..."), 0);
                QTimer::singleShot(2000, this, [this, xexInBin, doLoadXex]() { doLoadXex(xexInBin); });
            });
        });
    } else {
        if (m_mainWindow->statusBar())
            m_mainWindow->statusBar()->showMessage(tr("Waiting 1.5s, then loading XEX..."), 0);
        QTimer::singleShot(1500, this, [this, xexInBin, doLoadXex]() { doLoadXex(xexInBin); });
    }
}

void FastbasicBuildPanel::onProcessError(QProcess::ProcessError error)
{
    m_runButton->setEnabled(true);
    m_lastOutput += tr("Process error: %1").arg(m_process->errorString());
    QMessageBox::warning(this, tr("Fastbasic"), m_process->errorString());
    if (m_mainWindow && m_mainWindow->statusBar())
        m_mainWindow->statusBar()->showMessage(tr("Compile error"), 5000);
    (void)error;
}

void FastbasicBuildPanel::onShowOutput()
{
    QDialog* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Fastbasic Output"));
    QVBoxLayout* layout = new QVBoxLayout(dlg);
    QTextEdit* te = new QTextEdit(dlg);
    te->setReadOnly(true);
    te->setPlainText(m_lastOutput.isEmpty() ? tr("(No output yet)") : m_lastOutput);
    layout->addWidget(te);
    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    connect(box, &QDialogButtonBox::rejected, dlg, &QDialog::accept);
    layout->addWidget(box);
    dlg->resize(600, 400);
    dlg->exec();
    dlg->deleteLater();
}
