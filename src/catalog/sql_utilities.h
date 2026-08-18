#pragma once

#include "sql_pragmas.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

namespace remustwo::CatalogSql {

inline QStringList splitSqlStatements(const QString &content) {
    QStringList statements;
    QString current;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool inLineComment = false;

    for (int i = 0; i < content.size(); ++i) {
        const QChar ch = content.at(i);
        const QChar next = (i + 1 < content.size()) ? content.at(i + 1) : QChar();

        if (inLineComment) {
            if (ch == QChar('\n'))
                inLineComment = false;
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote && ch == QChar('-') && next == QChar('-')) {
            inLineComment = true;
            ++i;
            continue;
        }

        if (ch == QChar('\'') && !inDoubleQuote) {
            current.append(ch);
            if (inSingleQuote && next == QChar('\'')) {
                current.append(next);
                ++i;
            } else {
                inSingleQuote = !inSingleQuote;
            }
            continue;
        }

        if (ch == QChar('"') && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            current.append(ch);
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote && ch == QChar(';')) {
            const QString statement = current.trimmed();
            if (!statement.isEmpty())
                statements.append(statement);
            current.clear();
            continue;
        }

        current.append(ch);
    }

    const QString trailing = current.trimmed();
    if (!trailing.isEmpty())
        statements.append(trailing);

    return statements;
}

inline bool executeSqlScript(QSqlDatabase &database, const QString &path, QString &error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QStringLiteral("Failed to open %1: %2").arg(path, file.errorString());
        return false;
    }

    const QStringList statements = splitSqlStatements(QString::fromUtf8(file.readAll()));
    for (const QString &statement : statements) {
        if (statement.isEmpty())
            continue;
        QSqlQuery query(database);
        if (!query.exec(statement)) {
            error = QStringLiteral("Failed to execute %1: %2").arg(path, query.lastError().text());
            return false;
        }
    }
    return true;
}

inline QString catalogDataRoot() {
    return qEnvironmentVariableIsSet("REMUSTWO_CATALOG_DATA")
        ? QString::fromLocal8Bit(qgetenv("REMUSTWO_CATALOG_DATA"))
        : QStringLiteral(REMUSTWO_CATALOG_DATA_DIR);
}

inline QString migrationsDir() {
    return QDir(catalogDataRoot()).filePath(QStringLiteral("migrations"));
}

inline QString seedsDir() {
    return QDir(catalogDataRoot()).filePath(QStringLiteral("seeds"));
}

inline QStringList loadMigrationManifest(const QString &manifestPath, QString &error) {
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Failed to open migration manifest: %1").arg(manifestPath);
        return { };
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        error = QStringLiteral("Invalid migration manifest JSON: %1").arg(manifestPath);
        return { };
    }

    QStringList migrations;
    const QJsonArray arr = doc.object().value(QStringLiteral("migrations")).toArray();
    for (const QJsonValue &value : arr) {
        const QString name = value.toString();
        if (!name.isEmpty())
            migrations.append(name);
    }
    return migrations;
}

} // namespace remustwo::CatalogSql
