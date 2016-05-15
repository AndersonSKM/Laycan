#include "laycaneditorview.h"
#include "ui_laycaneditorview.h"

#include <QtXml>

LaycanEditorView::LaycanEditorView(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::laycaneditorview)
{
    ui->setupUi(this);
    m_initialized = false;

    //Create a model
    model = new QStandardItemModel(0,2,this);
    model->setHeaderData(0,Qt::Horizontal,"Verison");
    model->setHeaderData(1,Qt::Horizontal,"Description");
}

LaycanEditorView::~LaycanEditorView()
{
    delete ui;
    delete model;
}

void LaycanEditorView::setMode(const EditorModes &mode)
{
    //Configure window by mode
    switch (mode) {
        case View:
            setEditButtonsEnabled(false);
            setActionsButtonsEnabled(true);
            setEditsEnabled(false);
        break;

        case Edit:
            setEditButtonsEnabled(true);
            setActionsButtonsEnabled(false);
            setEditsEnabled(true);
        break;

        case New:
            setEditButtonsEnabled(true);
            setActionsButtonsEnabled(false);
            setEditsEnabled(true);
        break;
    }
    m_mode = mode;
}

EditorModes LaycanEditorView::mode() const
{
    return m_mode;
}

void LaycanEditorView::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_initialized) return;

    //Create a dialog to get Migration file
    DlgOpenFile dlg;
    if (dlg.exec() == QDialog::Rejected) {
        return abort();
    }

    m_xmlFile.setFileName(dlg.xml());
    if (!m_xmlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return abort(m_xmlFile.errorString());
    }

    QDomDocument document;
    if (!document.setContent(&m_xmlFile)) {
        m_xmlFile.close();
        return abort("Invalid XML file");
    }

    readFile(document);

    ui->treeMigrations->setModel(model);
    ui->treeMigrations->sortByColumn(0,Qt::AscendingOrder);
    ui->treeMigrations->expandAll();
    ui->treeMigrations->header()->resizeSection(0,85);

    //Filter shortcut keys
    ui->treeMigrations->installEventFilter(this);

    setMode(View);
    m_initialized = true;
}

void LaycanEditorView::writeFile()
{
  //Save changes on the disk
}

void LaycanEditorView::abort(const QString &msg)
{
    this->close();
    if (!msg.isEmpty()) {
        QMessageBox::critical(this,"Error",msg);
    }
}

void LaycanEditorView::setEditsEnabled(const bool value)
{
    ui->edDescription->setEnabled(value);
    ui->edVersion->setEnabled(value);
    ui->tbDownVersion->setEnabled(value);
    ui->tbUpVersion->setEnabled(value);
    ui->lbDescription->setEnabled(value);
    ui->lbVersion->setEnabled(value);

    ui->treeMigrations->setEnabled(!value);
}

void LaycanEditorView::setActionsButtonsEnabled(const bool value)
{
    ui->btnEdit->setEnabled(value);
    ui->btnNew->setEnabled(value);
    ui->btnDelete->setEnabled(value);
}

void LaycanEditorView::setEditButtonsEnabled(const bool value)
{
    ui->btnSave->setEnabled(value);
    ui->btnCancel->setEnabled(value);
}

void LaycanEditorView::readFile(const QDomDocument &document)
{
    //Parse Xml File
    QStandardItem *root = new QStandardItem("Migrations");
    model->appendRow(root);

    //Get the xml root element
    QDomElement xmlRoot = document.firstChildElement();

    //Read all migrations
    QDomNodeList migrations = xmlRoot.elementsByTagName("Migration");
    for (auto i = 0; i < migrations.count(); i++) {
        QDomElement migration =  migrations.at(i).toElement();

        //Add all propertys of migration in one item
        QList<QStandardItem*> items;
        items.append(new QStandardItem(migration.attribute("version")));
        items.append(new QStandardItem(migration.attribute("id","Update version")));

        //Up SQL in comment node
        QDomElement upVersion = migration.firstChildElement("UpVersion");
        QDomNode sqlUp = upVersion.firstChild();
        if (sqlUp.nodeType() == 8) {
            QString valueUp = sqlUp.nodeValue().remove("-->").remove("<--");
            items.append(new QStandardItem(valueUp));
        }

        //Down SQL in comment node
        QDomElement downVersion = migration.firstChildElement("DownVersion");
        QDomNode sqlDown = downVersion.firstChild();
        if (sqlDown.nodeType() == 8) {
            QString valueDown = sqlDown.nodeValue().remove("-->").remove("<--");
            items.append(new QStandardItem(valueDown));
        }

        //Append row in all migrations
        root->appendRow(items);
    }
}

bool LaycanEditorView::isInitialized() const
{
    return m_initialized;
}

void LaycanEditorView::setInitialized(bool initialized)
{
    m_initialized = initialized;
}

void LaycanEditorView::on_treeMigrations_clicked(const QModelIndex &index)
{
    int row = index.row();
    QModelIndex parent = index.parent();   
    QString version = model->data(model->index(row, 0, parent), Qt::EditRole).toString();
    QString description = model->data(model->index(row, 1, parent), Qt::EditRole).toString();
    QString upVersionSQL = model->data(model->index(row, 2, parent), Qt::EditRole).toString();
    QString downVersionSQL = model->data(model->index(row, 3, parent), Qt::EditRole).toString();

    ui->edVersion->setText(version);
    ui->edDescription->setText(description);
    ui->tbUpVersion->setText(upVersionSQL);
    ui->tbDownVersion->setText(downVersionSQL);
}

bool LaycanEditorView::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        QTreeView *tree = static_cast<QTreeView *>(obj);

        if (keyEvent->key() == Qt::Key_Up) {
            QModelIndex index = tree->indexAbove(tree->currentIndex());
            if (index.isValid()) {
                tree->setCurrentIndex(index);
                tree->clicked(index);
                return true;
            }
        }

        if (keyEvent->key() == Qt::Key_Down) {
            QModelIndex index = tree->indexBelow(tree->currentIndex());
            if (index.isValid()) {
                tree->setCurrentIndex(index);
                tree->clicked(index);
                return true;
            }
        }
    }

    // standard event processing
    return QObject::eventFilter(obj, event);
}

void LaycanEditorView::on_btnEdit_clicked()
{
    setMode(Edit);
}

void LaycanEditorView::on_btnCancel_clicked()
{
    if (QMessageBox::question(this,"","Discart Changes ?") == QMessageBox::No)
        return;

   setMode(View);
}
