#include <QApplication>
#include <QFont>
#include <QLibraryInfo>
#include <QLocale>
#include <QPalette>
#include <QStyleFactory>
#include <QTranslator>

#include "mainwindow.h"

int main(int argc, char** argv) {
    // 应用启动入口：设置样式、字体和调色板，然后显示主窗口。
    QApplication app(argc, argv);
    app.setApplicationName("Excel Mowan");
    app.setApplicationDisplayName("Excel Mowan");
    app.setStyle(QStyleFactory::create("Fusion"));

    if (QLocale::system().language() == QLocale::Chinese) {
        auto* qt_translator = new QTranslator(&app);
        const QString translations_dir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
        const bool loaded =
            qt_translator->load(QLocale::system(), QStringLiteral("qtbase"), QStringLiteral("_"), translations_dir) ||
            qt_translator->load(
                QLocale(QLocale::Chinese, QLocale::China),
                QStringLiteral("qtbase"),
                QStringLiteral("_"),
                translations_dir
            );
        if (loaded) {
            app.installTranslator(qt_translator);
        } else {
            delete qt_translator;
        }
    }

    QFont font("Microsoft YaHei UI", 10);
    app.setFont(font);

    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, QColor("#eef3ec"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#f7fbf6"));
    palette.setColor(QPalette::Text, QColor("#1f3425"));
    palette.setColor(QPalette::ButtonText, QColor("#1f3425"));
    palette.setColor(QPalette::Highlight, QColor("#217346"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    app.setPalette(palette);

    MainWindow w;
    w.show();
    return app.exec();
}
