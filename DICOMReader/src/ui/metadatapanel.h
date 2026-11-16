#ifndef DICOMREADER_UI_METADATAPANEL_H
#define DICOMREADER_UI_METADATAPANEL_H

#include "core/dicommetadata.h"

#include <QWidget>

class QLineEdit;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTreeView;

/// Lista os atributos DICOM do arquivo em árvore (sequências podem ser
/// expandidas), com filtro por tag, VR, nome ou valor e cópia (Ctrl+C).
class MetadataPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MetadataPanel(QWidget *parent = nullptr);

    void setEntries(const QList<dicomreader::MetadataEntry> &entries);
    void clear();

private:
    void applyFilter(const QString &text);
    void copySelection();

    QLineEdit *m_filter = nullptr;
    QTreeView *m_view = nullptr;
    QStandardItemModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
};

#endif // DICOMREADER_UI_METADATAPANEL_H
