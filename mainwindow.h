#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "projectmodel.h"

#include <QHash>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class QTreeWidget;
class QTreeWidgetItem;
class TimelineWidget;
class QCloseEvent;
class QEvent;

class MainWindow : public QWidget
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow();

private:
	void openProject();
	bool saveProject();
	bool saveProjectAs();
	bool saveProjectTo(const QString& path);
	bool confirmUnsavedChanges();
	void setProjectFile(const QString& path);
	void closeEvent(QCloseEvent* event) override;
	void addStage(bool asChild);
	void editSelectedStage();
	void moveStage(const QString& stageId, int days);
	void showTreeContextMenu(const QPoint& position, const QPoint& globalPosition, bool onHeader);
	void refreshProjectView();
	void selectTreeStage(const QString& stageId);
	bool eventFilter(QObject* watched, QEvent* event) override;

	Ui::MainWindow* ui;
	ProjectModel m_project;
	QString m_filePath;
	QHash<QString, QTreeWidgetItem*> m_itemsById;
	QTreeWidget* m_tree = nullptr;
	TimelineWidget* m_timeline = nullptr;
};
#endif // MAINWINDOW_H
