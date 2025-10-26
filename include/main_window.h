#pragma once

#include <QMainWindow>
#include <memory>

#include "signal_tree.h"
#include "waveform_view.h"
#include "simple_fst_reader.h"

class QAction;
class QActionGroup;
class QSplitter;
class QLabel;
class QLineEdit;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void openFstFile(const QString &filePath);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openFstFileDialog();
    void addSignalToWaveform(const fst::Signal &signal);
    void updateStatusBar(qint64 primary, qint64 delta);
    void filterSignals(const QString &text);
    void clearFilter();

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    QWidget *createLeftPane();
    void applyDarkPalette();
    void loadFstFile(const QString &filePath);

    SignalTree *m_signalTree = nullptr;
    WaveformView *m_waveformView = nullptr;
    fst::SimpleFstReader m_reader;

    QAction *m_openAction = nullptr;
    QAction *m_exitAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_resetViewAction = nullptr;
    QLabel *m_primaryCursorLabel = nullptr;
    QLabel *m_deltaLabel = nullptr;
    QLabel *m_referenceCursorLabel = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    qint64 m_lastReferenceTime = -1;
    qint64 m_lastPrimaryTime = -1;
};

