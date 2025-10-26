#include "main_window.h"

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QtGlobal>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_signalTree = new SignalTree(this);
    m_waveformView = new WaveformView(this);

    QSplitter *splitter = new QSplitter(this);
    splitter->addWidget(m_signalTree);
    splitter->addWidget(m_waveformView);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    createActions();
    createMenus();
    createStatusBar();

    connect(m_signalTree, &SignalTree::signalActivated, this, &MainWindow::addSignalToWaveform);
    connect(m_waveformView, &WaveformView::cursorMoved, this, &MainWindow::updateStatusBar);

    setWindowTitle(tr("GTKWave C++ Clone"));
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
}

void MainWindow::createActions()
{
    m_openAction = new QAction(tr("&Open FST..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openFstFileDialog);

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);

    QToolBar *toolBar = addToolBar(tr("File"));
    toolBar->addAction(m_openAction);
}

void MainWindow::createStatusBar()
{
    m_primaryCursorLabel = new QLabel(tr("Cursor: -"), this);
    m_deltaLabel = new QLabel(tr("Δt: -"), this);

    statusBar()->addPermanentWidget(m_primaryCursorLabel);
    statusBar()->addPermanentWidget(m_deltaLabel);
}

void MainWindow::openFstFile(const QString &filePath)
{
    if (filePath.isEmpty())
    {
        return;
    }

    loadFstFile(filePath);
}

void MainWindow::openFstFileDialog()
{
    const QString filePath = QFileDialog::getOpenFileName(this, tr("Open FST File"), QString(), tr("FST Files (*.fst);;All Files (*.*)"));
    openFstFile(filePath);
}

void MainWindow::loadFstFile(const QString &filePath)
{
    if (!m_reader.load(filePath))
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load FST file: %1").arg(m_reader.lastError()));
        return;
    }

    m_signalTree->clear();
    m_waveformView->clearSignals();
    m_signalTree->populate(m_reader.rootScope(), m_reader.signalMap());
    const qint64 maxTime = qMax<qint64>(100, m_reader.maxTime());
    m_waveformView->setTimeRange(0, maxTime == 0 ? 100 : maxTime);
    statusBar()->showMessage(tr("Loaded %1").arg(filePath), 3000);
}

void MainWindow::addSignalToWaveform(const fst::Signal &signal)
{
    m_waveformView->addSignal(signal);
}

void MainWindow::updateStatusBar(qint64 primary, qint64 delta)
{
    if (primary < 0)
    {
        m_primaryCursorLabel->setText(tr("Cursor: -"));
    }
    else
    {
        m_primaryCursorLabel->setText(tr("Cursor: %1").arg(primary));
    }

    m_deltaLabel->setText(tr("Δt: %1").arg(delta));
}

