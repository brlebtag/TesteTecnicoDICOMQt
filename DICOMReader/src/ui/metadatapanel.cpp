#include "metadatapanel.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

using dicomreader::MetadataEntry;

namespace {

enum Column { TagColumn, VrColumn, NameColumn, ValueColumn, ColumnCount };

QList<QStandardItem *> createRow(const MetadataEntry &entry)
{
    QList<QStandardItem *> row{new QStandardItem(entry.tag), new QStandardItem(entry.vr),
                               new QStandardItem(entry.name), new QStandardItem(entry.value)};
    row[ValueColumn]->setToolTip(entry.value);
    return row;
}

/// Percorre a árvore na ordem de exibição e formata as linhas selecionadas
/// como texto separado por tabulações.
void appendSelectedRows(const QAbstractItemModel &model, const QItemSelectionModel &selection,
                        const QModelIndex &parent, QStringList &lines)
{
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const QModelIndex index = model.index(row, 0, parent);
        if (selection.isRowSelected(row, parent)) {
            QStringList cells;
            for (int column = 0; column < ColumnCount; ++column)
                cells.append(index.siblingAtColumn(column).data().toString());
            lines.append(cells.join(QLatin1Char('\t')));
        }
        appendSelectedRows(model, selection, index, lines);
    }
}

} // namespace

MetadataPanel::MetadataPanel(QWidget *parent)
    : QWidget(parent)
    , m_filter(new QLineEdit(this))
    , m_view(new QTreeView(this))
    , m_model(new QStandardItemModel(0, ColumnCount, this))
    , m_proxy(new QSortFilterProxyModel(this))
{
    m_model->setHorizontalHeaderLabels(
        {QStringLiteral("Tag"), QStringLiteral("VR"), QStringLiteral("Nome"), QStringLiteral("Valor")});

    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);          // procura em todas as colunas
    m_proxy->setRecursiveFilteringEnabled(true); // mantém os pais dos itens encontrados

    m_filter->setPlaceholderText(QStringLiteral("Filtrar por tag, nome ou valor"));
    m_filter->setClearButtonEnabled(true);

    m_view->setModel(m_proxy);
    m_view->setUniformRowHeights(true);
    m_view->setAlternatingRowColors(true);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->header()->setStretchLastSection(true);

    auto *copyAction = new QAction(QStringLiteral("Copiar"), m_view);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setShortcutContext(Qt::WidgetShortcut);
    m_view->addAction(copyAction);
    m_view->setContextMenuPolicy(Qt::ActionsContextMenu);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_filter);
    layout->addWidget(m_view);

    connect(m_filter, &QLineEdit::textChanged, this, &MetadataPanel::applyFilter);
    connect(copyAction, &QAction::triggered, this, &MetadataPanel::copySelection);
}

void MetadataPanel::setEntries(const QList<MetadataEntry> &entries)
{
    clear();

    // As entradas chegam em ordem, com a profundidade de cada uma: o pai de um
    // item de profundidade d é o último item visto na profundidade d - 1.
    std::vector<QStandardItem *> lastItemAtDepth;
    for (const MetadataEntry &entry : entries) {
        const auto depth = static_cast<std::size_t>(std::max(entry.depth, 0));
        QStandardItem *parent = (depth == 0 || depth > lastItemAtDepth.size())
                                    ? m_model->invisibleRootItem()
                                    : lastItemAtDepth[depth - 1];
        const QList<QStandardItem *> row = createRow(entry);
        parent->appendRow(row);

        lastItemAtDepth.resize(depth + 1);
        lastItemAtDepth[depth] = row.first();
    }

    for (int column = TagColumn; column < ValueColumn; ++column)
        m_view->resizeColumnToContents(column);
}

void MetadataPanel::clear()
{
    m_model->removeRows(0, m_model->rowCount());
}

void MetadataPanel::applyFilter(const QString &text)
{
    m_proxy->setFilterFixedString(text);
    if (!text.isEmpty())
        m_view->expandAll();
}

void MetadataPanel::copySelection()
{
    QStringList lines;
    appendSelectedRows(*m_proxy, *m_view->selectionModel(), QModelIndex(), lines);
    if (!lines.isEmpty())
        QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}
