#include <QApplication>
#include"Simplify.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    Simplify window;
    window.show();
    return app.exec();
}
