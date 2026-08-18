#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Создаем экземпляр приложения Qt
    QApplication app(argc, argv);

    // Устанавливаем стиль приложения для современного внешнего вида
    app.setStyle("Fusion");

    // Создаем главное окно программы
    MainWindow window;

    // Показываем окно на весь экран
    window.showMaximized();

    // Запускаем основной цикл обработки событий
    return app.exec();
}
