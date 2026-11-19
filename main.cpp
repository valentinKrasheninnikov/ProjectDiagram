#include "mainwindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
	QApplication a(argc, argv);
	a.setStyleSheet(QStringLiteral("QToolTip { color: #202020; background-color: #fff8dc; border: 1px solid #767676; }"));
	MainWindow w;
	w.show();
	return a.exec();
}
