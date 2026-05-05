#include "mainwindow.h"
#include <QApplication>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>

void initDatabase()
{
    //添加 SQLite 数据库
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");

    //设置数据库文件名
    db.setDatabaseName("alarm_log.db");

    // 尝试打开数据库
    if (db.open())
    {
        //创建表
        QSqlQuery query;
        bool success = query.exec(R"(
            CREATE TABLE IF NOT EXISTS alarm_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                time TEXT,
                content TEXT,
                status TEXT
            )
        )");

        if (!success)
            qDebug() << "创建表失败：" << query.lastError().text();
    }
    else
    {
        qDebug() << "数据库打开失败：" << db.lastError().text();
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    initDatabase();
    MainWindow w;
    w.show();
    return a.exec();
}