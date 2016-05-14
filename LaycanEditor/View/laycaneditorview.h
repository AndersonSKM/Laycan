#ifndef LAYCANEDITORVIEW_H
#define LAYCANEDITORVIEW_H

#include <QtCore>
#include <QWidget>
#include <QtGui>
#include <QStandardItemModel>
#include <QMainWindow>
#include <QShowEvent>
#include <QtXml>
#include <QMessageBox>

#include "dlgopenfile.h"

namespace Ui {
class laycaneditorview;
}

class LaycanEditorView : public QMainWindow
{
    Q_OBJECT

public:
    explicit LaycanEditorView(QWidget *parent = 0);
    ~LaycanEditorView();

    bool isInitialized() const;
    void setInitialized(bool initialized);

public slots:
    void showEvent(QShowEvent* event);

private slots:
    void on_treeMigrations_clicked(const QModelIndex &index);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    void readFile(const QDomDocument &document);
    void writeFile();
    void abort(const QString &msg = "");

    QFile m_xmlFile;
    QStandardItemModel *model;
    Ui::laycaneditorview *ui;
    bool m_initialized;
};

#endif // LAYCANEDITORVIEW_H