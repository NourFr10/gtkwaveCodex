#include "main_window.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QColor>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

namespace
{
constexpr int kDefaultWidth = 1280;
constexpr int kDefaultHeight = 720;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    applyDarkPalette();
    setDockNestingEnabled(true);

    QWidget *leftPane = createLeftPane();
    m_waveformView = new WaveformView(this);

    QSplitter *splitter = new QSplitter(this);
    splitter->setObjectName(QStringLiteral("workspaceSplitter"));
    splitter->addWidget(leftPane);
    splitter->addWidget(m_waveformView);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    connect(m_signalTree, &SignalTree::signalActivated, this, &MainWindow::addSignalToWaveform);
    connect(m_waveformView, &WaveformView::cursorMoved, this, &MainWindow::updateStatusBar);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &MainWindow::filterSignals);

    auto *focusSearchShortcut = new QShortcut(QKeySequence::Find, this);
    connect(focusSearchShortcut, &QShortcut::activated, this, [this]() {
        if (m_filterEdit)
        {
            m_filterEdit->setFocus(Qt::ShortcutFocusReason);
            m_filterEdit->selectAll();
        }
    });

    setWindowTitle(tr("NovaWave – FST Waveform Studio"));
    resize(kDefaultWidth, kDefaultHeight);
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
}

void MainWindow::createActions()
{
    m_openAction = new QAction(QIcon::fromTheme(QStringLiteral("document-open")), tr("&Open FST…"), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openFstFileDialog);

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    m_zoomInAction = new QAction(QIcon::fromTheme(QStringLiteral("zoom-in")), tr("Zoom &In"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, m_waveformView, &WaveformView::zoomIn);

    m_zoomOutAction = new QAction(QIcon::fromTheme(QStringLiteral("zoom-out")), tr("Zoom &Out"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, m_waveformView, &WaveformView::zoomOut);

    m_resetViewAction = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Reset View"), this);
    m_resetViewAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_resetViewAction, &QAction::triggered, m_waveformView, &WaveformView::resetView);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_resetViewAction);
}

void MainWindow::createToolBars()
{
    QToolBar *fileBar = addToolBar(tr("File"));
    fileBar->setMovable(false);
    fileBar->addAction(m_openAction);

    QToolBar *viewBar = addToolBar(tr("View"));
    viewBar->setMovable(false);
    viewBar->addAction(m_zoomInAction);
    viewBar->addAction(m_zoomOutAction);
    viewBar->addAction(m_resetViewAction);
}

void MainWindow::createStatusBar()
{
    statusBar()->setStyleSheet(QStringLiteral("QStatusBar { background: #2b2b2b; color: #f0f0f0; }"));
    m_primaryCursorLabel = new QLabel(tr("Time: —"), this);
    m_referenceCursorLabel = new QLabel(tr("Baseline: —"), this);
    m_deltaLabel = new QLabel(tr("Δ: —"), this);

    statusBar()->addPermanentWidget(m_primaryCursorLabel);
    statusBar()->addPermanentWidget(m_referenceCursorLabel);
    statusBar()->addPermanentWidget(m_deltaLabel);
}

QWidget *MainWindow::createLeftPane()
{
    QWidget *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("designPanel"));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("Design Browser"), panel);
    title->setObjectName(QStringLiteral("designTitle"));
    title->setStyleSheet(QStringLiteral("#designTitle { font-size: 14px; font-weight: 600; color: #f0f0f0; }"));
    layout->addWidget(title);

    m_filterEdit = new QLineEdit(panel);
    m_filterEdit->setPlaceholderText(tr("Filter modules or signals"));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setObjectName(QStringLiteral("filterEdit"));
    layout->addWidget(m_filterEdit);

    m_signalTree = new SignalTree(panel);
    m_signalTree->setObjectName(QStringLiteral("signalTree"));
    layout->addWidget(m_signalTree, 1);

    panel->setStyleSheet(QStringLiteral(
        "#designPanel { background-color: #202125; border-right: 1px solid #3b3b3b; }"
        "#filterEdit { background-color: #2a2d33; border: 1px solid #3e434d; padding: 4px 8px; color: #e0e0e0; }"
        "#filterEdit:focus { border-color: #448aff; }"
        "QTreeWidget#signalTree { background-color: #1c1d21; border: 1px solid #323437; color: #e0e0e0; }"
        "QTreeWidget::item:selected { background-color: #3a4254; }"
        "QHeaderView::section { background-color: #2a2c30; color: #d0d0d0; border: none; padding: 4px; }"));

    return panel;
}

void MainWindow::applyDarkPalette()
{
    QPalette palette = qApp->palette();
    palette.setColor(QPalette::Window, QColor(28, 28, 32));
    palette.setColor(QPalette::Base, QColor(30, 31, 35));
    palette.setColor(QPalette::AlternateBase, QColor(40, 42, 48));
    palette.setColor(QPalette::Text, QColor(224, 224, 224));
    palette.setColor(QPalette::WindowText, QColor(240, 240, 240));
    palette.setColor(QPalette::Button, QColor(40, 40, 45));
    palette.setColor(QPalette::ButtonText, QColor(230, 230, 230));
    palette.setColor(QPalette::Highlight, QColor(68, 138, 255));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    qApp->setPalette(palette);
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
    const QString filePath = QFileDialog::getOpenFileName(this, tr("Open FST File"), QString(), tr("FST Files (*.fst);;VCD Files (*.vcd);;All Files (*.*)"));
    openFstFile(filePath);
}

void MainWindow::loadFstFile(const QString &filePath)
{
    if (!m_reader.load(filePath))
    {
        QMessageBox::critical(this, tr("Unable to Load"), tr("Failed to load trace file: %1").arg(m_reader.lastError()));
        return;
    }

    m_signalTree->clear();
    m_waveformView->clearSignals();
    m_signalTree->populate(m_reader.rootScope(), m_reader.signalMap());
    const qint64 maxTime = qMax<qint64>(100, m_reader.maxTime());
    m_waveformView->setTimeRange(0, maxTime == 0 ? 100 : maxTime);
    m_lastPrimaryTime = -1;
    m_lastReferenceTime = -1;
    updateStatusBar(-1, 0);
    statusBar()->showMessage(tr("Loaded %1").arg(filePath), 4000);
}

void MainWindow::addSignalToWaveform(const fst::Signal &signal)
{
    m_waveformView->addSignal(signal);
}

void MainWindow::updateStatusBar(qint64 primary, qint64 delta)
{
    m_lastPrimaryTime = primary;
    const qint64 reference = m_waveformView->referenceCursor();
    if (reference >= 0)
    {
        m_lastReferenceTime = reference;
    }

    if (primary < 0)
    {
        m_primaryCursorLabel->setText(tr("Time: —"));
    }
    else
    {
        m_primaryCursorLabel->setText(tr("Time: %1").arg(primary));
    }

    if (m_lastReferenceTime < 0)
    {
        m_referenceCursorLabel->setText(tr("Baseline: —"));
    }
    else
    {
        m_referenceCursorLabel->setText(tr("Baseline: %1").arg(m_lastReferenceTime));
    }

    if (primary < 0)
    {
        m_deltaLabel->setText(tr("Δ: —"));
    }
    else
    {
        m_deltaLabel->setText(tr("Δ: %1").arg(delta));
    }
}

void MainWindow::filterSignals(const QString &text)
{
    if (m_signalTree)
    {
        m_signalTree->filter(text);
    }
}

void MainWindow::clearFilter()
{
    if (m_filterEdit)
    {
        m_filterEdit->clear();
    }
}
