#include <QDialog>
#include <QApplication>
#include <QtGlobal>
#include <QDomNode>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QMessageBox>

#include "laycan.h"

Laycan::Laycan()
{
    m_progressVisible = true;
    m_tablesChanged = 0;
    m_verifiedTables = 0;
    m_createdTables = 0;
}

Laycan::~Laycan()
{

}

void Laycan::Migrate(const QString xmlPath)
{
    InitXML(xmlPath);
    loadMigrationsFromXML();
    executeMigrations();
}

void Laycan::setProgressVisible(bool visible)
{
    m_progressVisible = visible;
}

void Laycan::setTablesChanged(int count)
{
    m_tablesChanged = count;
}

void Laycan::setVerifiedTables(int count)
{
    m_verifiedTables = count;
}

void Laycan::setCreatedTables(int count)
{
    m_createdTables = count;
}

bool Laycan::progressVisible()
{
    return m_progressVisible;
}

int Laycan::tablesChanged()
{
    return m_tablesChanged;
}

int Laycan::verifiedTables()
{
    return m_verifiedTables;
}

int Laycan::createTables()
{
    return m_createdTables;
}

void Laycan::addTablesChanged()
{
    m_tablesChanged++;
}

void Laycan::addVerifiedTables()
{
    m_verifiedTables++;
}

void Laycan::addCreatedTables()
{
    m_createdTables++;
}

Table Laycan::tableByName(QString tableName)
{
    Table tb;
    foreach (tb, Tables) {
        if (tb.name().toUpper() == tableName.toUpper())
            return tb;
    }

    return Table();
}

bool Laycan::writeMigrationLog(Migration &script)
{
    QSqlQuery query;
    query.prepare("insert into schema_version"
                  " (version, description, script) "
                  "values "
                  " (:v , :d, :s)");
    query.bindValue(0, script.version());
    query.bindValue(1, script.description());
    query.bindValue(2, script.SQL());

    return query.exec();
}

void Laycan::executeMigrations()
{
    dlg = new MigrationProgress();
    dlg->setWindowFlags( Qt::CustomizeWindowHint );
    dlg->setMaximum(Migrations.count());

    if (progressVisible())
        dlg->show();

    setVerifiedTables(0);
    setCreatedTables(0);
    setVerifiedTables(0);

    QSqlQuery query;
    if (!QSqlDatabase::database().tables().contains("schema_version")) {
        qDebug() << "[Criando tabela de versão]";

        QSqlDatabase::database().transaction();

        query.exec("CREATE TABLE schema_version ("
                   "   version INT NULL, "
                   "   description VARCHAR(200) NULL,"
                   "   script TEXT NULL"
                   ");");

        QSqlDatabase::database().commit();
    }

    query.clear();
    query.exec("select max(version) from schema_version");
    query.next();
    int dbSchemaVersion = query.value(0).toInt();

    qDebug() << "[Versão do schema atual: " << dbSchemaVersion << "]";

    Migration script;
    foreach (script, Migrations) {
        if (script.version() > dbSchemaVersion) {
            qDebug() << "[Migrando a versão do schema para: " << script.version() << "]";
            dlg->setStatus("Verificando Migração: " + script.description());

            QSqlDatabase::database().transaction();

            bool executed = query.exec(script.SQL());
            if (executed) {
                dlg->setStatus("Executando sql: " + script.description(),"red");

                executed = writeMigrationLog(script);
            }

            if (executed) {
                QSqlDatabase::database().commit();
            } else {
                qDebug() << "[ERRO] Erro ao executar sql: " << script.description() << endl
                         << "Erro: " << query.lastError().text() << endl
                         << " SQL: " << query.lastQuery();
                QSqlDatabase::database().rollback();
            }
        }
        dlg->setProgress(dlg->progress()+1);
        addVerifiedTables();
    }
    qDebug() << "[Finalizando Migração]";
    qDebug() << "[Migrações verificadas: " << verifiedTables() << "]";
    qDebug() << "[Migrações aplicadas: "   << createTables() << "]";

    delete dlg;
}

void Laycan::compareTables()
{
    dlg = new MigrationProgress();
    dlg->setWindowFlags( Qt::CustomizeWindowHint );
    dlg->setMaximum(Tables.count());

    if (progressVisible())
        dlg->show();

    setVerifiedTables(0);
    setCreatedTables(0);
    setVerifiedTables(0);

    QSqlQuery query;
    qDebug() << "[Verificando Lista de Tabelas....]";

    for (int i = 0; i != Tables.count(); i++) {
        Table table = Tables.at(i);

        dlg->setStatus("Verificando tabela: "+table.name());

        if ( !QSqlDatabase::database().tables().contains( table.name() ) ) {
            QSqlDatabase::database().transaction();

            if (query.exec(generateSQL(table))) {
                qDebug() << "[Criando tabela " << table.name() << "]";
                dlg->setStatus("Criando tabela: "+table.name(),"red");

                addCreatedTables();
            } else {
                qDebug() << "[Erro ao criar tablela: " << table.name()
                         << "Erro: " << query.lastError().text()
                         << " SQL: " << query.lastQuery() << "]";
			}

            QSqlDatabase::database().commit();
        } else {
            compareFields(table);
        }

        dlg->setProgress(dlg->progress()+1);
        addVerifiedTables();
    }
    qDebug() << "[Finalizando Verificação de Tabelas....]";
    qDebug() << "[Tabelas verificadas: " << verifiedTables() << "]";
    qDebug() << "[Tabelas criadas: " << createTables() << "]";
    qDebug() << "[Tabelas alteradas:" << tablesChanged() << "]";

    delete dlg;
}

void Laycan::compareFields(Table &table)
{
    Fields field;
    foreach (field, table.fields) {
        if (!columnExists(table.name(), field.name())) {
            dlg->setStatus("Alterando tabela: "+table.name(),"green");

            QSqlDatabase::database().transaction();

            QSqlQuery q;
            if (!q.exec(generateAddColumnSQL(table.name(),field)) ) {
                qDebug() << "Erro ao adicionar coluna: " << field.name()
                         << "Erro:" << q.lastError().text()
                         << "SQL: " << q.lastQuery();
            }

            QSqlDatabase::database().commit();
            addTablesChanged();
        }
    }
}

bool Laycan::columnExists(QString tableName, QString columnName)
{
    /* Verifica se a coluna existe na tabela */
    QSqlQuery q;
    q.prepare("SELECT * "
              "FROM information_schema.columns "
              "WHERE table_schema = :dbName "
              "  AND table_name   = :tbName"
              "  AND column_name  = :colName");
    q.bindValue(0,QSqlDatabase::database().databaseName());
    q.bindValue(1,tableName);
    q.bindValue(2,columnName);
    q.exec();

    if (q.next())
        return true;

    return false;
}

QString Laycan::generateAddColumnSQL(QString tableName, Fields field)
{
    QString SQL = "ALTER TABLE " + tableName + " ADD " + field.toSQL();
    return SQL;
}

QString Laycan::generateSQL(Table &table)
{
    //Gera o SQL create da tabela passada como parametro *
    QString SQL = "CREATE TABLE " + table.name() + "( ";

    for (auto i = table.fields.begin(); i != table.fields.end(); ++i) {
        Fields &field = *i;
        SQL += field.toSQL();
        SQL += ",";
    }

    for (auto i = table.fields.begin(); i != table.fields.end(); ++i) {
        Fields &field = *i;

        if ( field.isPrimaryKey() )
            SQL += "PRIMARY KEY(" + field.name() + ")";
    }

    SQL += ");";

    return SQL;
}

/* XML Functions */

void Laycan::loadMigrationsFromXML(void)
{
    qDebug() << "[Carregando scripts do arquivo " << filePath << "]";
    QDomNodeList root = xml.elementsByTagName("SQL");

    if (root.isEmpty()) {
        qDebug() << "[Nenhuma migração encontrada no XML]";
        return;
    }

    for (int i = 0; i != root.count(); i++) {
        QDomNode migrationNode = root.at(i);
        Migration script;

        if (migrationNode.isElement()) {
            QDomElement migration = migrationNode.toElement();
            QString version = migration.attribute("version");

            script.setVersion(version.toInt());
            script.setDescription(migration.attribute("id", "Update schema to version " + version));
        }

        // SQL in comment node
        QDomNode sql = migrationNode.firstChild();
        if (sql.nodeType() == 8) {
            script.setSQL(sql.nodeValue());
        }

        Migrations.append(script);
    }

    qDebug() << "[Migrações carregadas: " << root.count() << "]";
}

void Laycan::loadTablesFromXML()
{
    qDebug() << "[Carregando tabelas do arquivo " << filePath << "]";
    QDomNodeList root = xml.elementsByTagName("Table");

    for (int i = 0; i != root.count(); i++) {
        QDomNode tableNode = root.at(i);

        if (tableNode.isElement()) {
            QDomElement tableElement = tableNode.toElement();
            Table tb;
            tb.setName(tableElement.attribute("Name"));

            QDomNodeList fieldNodes = tableNode.childNodes();

            /*
            QDomNode fieldNode;
            foreach (fieldNode, fieldNodes) {
                QDomElement fieldElement = fieldNode.toElement();
                Fields f;
                f.setName(fieldElement.attribute("Name"));

                QString type = fieldElement.attribute("Type");
                if (type.toUpper() == "INTEGER")
                    f.setType(ftInteger);
                else if (type.toUpper() == "VARCHAR")
                    f.setType(ftVarchar);
                else if (type.toUpper() == "BOOLEAN")
                    f.setType(ftBoolean);
                else if (type.toUpper() == "FLOAT")
                    f.setType(ftFloat);

                f.setSize(fieldElement.attribute("Size").toInt());
                f.setIsNull(fieldElement.attribute("Null","1").toInt());
                f.setPrimaryKey(fieldElement.attribute("Pk","0").toInt());
                f.setDefaultValue(fieldElement.attribute("Default"));
                f.setExtra(fieldElement.attribute("Extra"));

                tb.fields.append(f);
            }
            */

            Tables.append(tb);
        }
    }
}

void Laycan::InitXML(QString xmlPath)
{
    filePath = xmlPath;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        QMessageBox::information(NULL,"","Erro ao abrir arquivo XML");

    if (!xml.setContent(&file)) {
        file.close();
        QMessageBox::information(NULL,"","Erro ao setar XML");
    }

    file.close();
}

/* End XML Functions */

void Migration::setVersion(int version)
{
    m_version = version;
}

void Migration::setDescription(QString description)
{
    m_description = description;
}

void Migration::setSQL(QString sql)
{
    m_sql = sql;
}

int Migration::version(void)
{
    return m_version;
}

QString Migration::description(void)
{
    return m_description;
}

QString Migration::SQL(void)
{
    return m_sql;
}

// ** Table class implementation **

void Table::setName(QString name)
{
    t_name = name;
}

QString Table::name(void)
{
    return t_name;
}

Fields Table::fieldByName(QString fieldName)
{
    Fields f;
    foreach (f, fields) {
        if (f.name().toUpper() == fieldName.toUpper())
            return f;
    }

    return Fields();
}

// ** Fields class implementation **

Fields::Fields(QString name, DataTypes type, int size, bool isNull,
    bool pk, QVariant def, bool autoIncKey)
{
    f_name = name;
    f_type = type;
    f_size = size;
    f_null = isNull;
    f_primaryKey = pk;
    f_defaultValue = def;
    f_extra = autoIncKey ? "AUTO_INCREMENT" : "";
}

QString Fields::toSQL(void)
{
    QString SQL;
    SQL += name();
    SQL += " " + typeToSQL(type());
    SQL += size() != 0 ? "(" + QString::number(size()) + ")" : "";

    if (isAutoIncKey()) {
        SQL += " NOT NULL ";
        SQL += " AUTO_INCREMENT";
    } else {
        SQL += isNull() ? " NULL" : " NOT NULL";
        SQL += " DEFAULT ";

        if (! (defaultValue() == "")) {
            if (type() == ftVarchar)
                SQL += "'" + defaultValue().toString() + "'";
            else
                SQL += defaultValue().toString();
        } else {
            SQL += "NULL";
		}
    }

    return SQL;
}

QString Fields::typeToSQL(DataTypes type)
{
    switch (type)
    {
        case ftInteger:
            return "INT";

        case ftVarchar:
            return "VARCHAR";

        case ftBoolean:
            return "BOOLEAN";

        case ftDateTime:
            return "DATETIME";

        case ftFloat:
            return "FLOAT";

        default:
            return "VARCHAR";
    }
}

void Fields::setName(QString name)
{
    f_name = name;
}

void Fields::setType(DataTypes type)
{
    f_type = type;
}

void Fields::setSize(int size)
{
    f_size = size;
}

void Fields::setIsNull(bool isNull)
{
    f_null = isNull;
}

void Fields::setPrimaryKey(bool pk)
{
    f_primaryKey = pk;
}

void Fields::setDefaultValue(QVariant def)
{
    f_defaultValue = def;
}

void Fields::setExtra(QString extra)
{
    f_extra = extra;
}

void Fields::setAutoIncKey(bool autoInc)
{
    f_extra = autoInc ? "AUTO_INCREMENT" : "";
}

QString Fields::name()
{
    return f_name;
}

DataTypes Fields::type()
{
    return f_type;
}

int Fields::size()
{
    return f_size;
}

bool Fields::isNull()
{
    return f_null;
}

bool Fields::isPrimaryKey()
{
    return f_primaryKey;
}

QVariant Fields::defaultValue()
{
    return f_defaultValue;
}

QString Fields::extra()
{
    return f_extra;
}

bool Fields::isAutoIncKey()
{
    return (f_extra.toUpper().trimmed() == "AUTO_INCREMENT");
}

/* End Fields Class */

/* Index Class Implementation */

Index::Index(QString name, IndexType type)
{
    i_name = name;
    i_type = type;
}

void Index::setName(QString name)
{
    i_name = name;
}

void Index::setType(IndexType type)
{
    i_type = type;
}

QString Index::name()
{
    return i_name;
}

IndexType Index::type()
{
    return i_type;
}

/* End Index Class */

/* Constraint Class Implementation*/

Constraint::Constraint(Table *parent, Fields column, Table referTable,Fields referColumn) :
    c_column(column), c_referTable(referTable), c_referColumns(referColumn)
{
    c_name = "fk_" + parent->name() + "_" + referTable.name() + "_" + column.name();
}

void Constraint::setName(QString name)
{
    c_name = name;
}

void Constraint::setColumn(Fields column)
{
    c_column = column;
}

void Constraint::setReferTable(Table table)
{
    c_referTable = table;
}

void Constraint::setReferColumn(Fields referColumn)
{
    c_referColumns = referColumn;
}

QString Constraint::name(void)
{
    return c_name;
}

Fields Constraint::column(void)
{
    return c_column;
}

Table Constraint::referTable(void)
{
    return c_referTable;
}

Fields Constraint::referColumns(void)
{
    return c_referColumns;
}

/* End Constriant Class */
