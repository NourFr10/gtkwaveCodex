#include "signal_tree.h"

#include <QHeaderView>
#include <QTreeWidgetItem>
#include <QString>

SignalTree::SignalTree(QWidget *parent)
    : QTreeWidget(parent)
{
    setColumnCount(3);
    setAlternatingRowColors(true);
    QStringList headers;
    headers << tr("Name") << tr("Type") << tr("Direction");
    setHeaderLabels(headers);
    header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(this, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
        const QVariant handleVariant = item->data(0, Qt::UserRole);
        if (!handleVariant.isValid() || !m_signals)
        {
            return;
        }

        const int handle = handleVariant.toInt();
        const auto it = m_signals->find(handle);
        if (it != m_signals->end())
        {
            emit signalActivated(it.value());
        }
    });
}

void SignalTree::populate(const fst::Scope &rootScope, const QMap<int, fst::Signal> &signals)
{
    clear();
    m_signals = &signals;
    if (rootScope.path.isEmpty() && rootScope.type == QLatin1String("root"))
    {
        for (const fst::Scope &child : rootScope.children)
        {
            addScopeItem(nullptr, child);
        }
    }
    else
    {
        addScopeItem(nullptr, rootScope);
    }
    expandAll();
}

void SignalTree::addScopeItem(QTreeWidgetItem *parentItem, const fst::Scope &scope)
{
    QTreeWidgetItem *scopeItem = nullptr;
    if (parentItem)
    {
        scopeItem = new QTreeWidgetItem(parentItem);
    }
    else
    {
        scopeItem = new QTreeWidgetItem(this);
    }

    scopeItem->setText(0, scope.name);
    scopeItem->setText(1, scope.type);

    if (m_signals)
    {
        for (int handle : scope.signalHandles)
        {
            const fst::Signal signal = m_signals->value(handle);
            QTreeWidgetItem *signalItem = new QTreeWidgetItem(scopeItem);
            signalItem->setText(0, signal.name);
            signalItem->setText(1, signal.type);
            signalItem->setText(2, signal.direction);
            signalItem->setData(0, Qt::UserRole, signal.handle);
        }
    }

    for (const fst::Scope &childScope : scope.children)
    {
        addScopeItem(scopeItem, childScope);
    }
}

