#pragma once

#include <QTreeWidget>

#include "simple_fst_reader.h"

class SignalTree : public QTreeWidget
{
    Q_OBJECT
public:
    explicit SignalTree(QWidget *parent = nullptr);

    void populate(const fst::Scope &rootScope, const QMap<int, fst::Signal> &signalMap);
    void filter(const QString &text);

signals:
    void signalActivated(const fst::Signal &signal);

private:
    void addScopeItem(QTreeWidgetItem *parentItem, const fst::Scope &scope);
    bool filterItem(QTreeWidgetItem *item, const QString &pattern);

    const QMap<int, fst::Signal> *m_signals = nullptr;
};

