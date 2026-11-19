#pragma once

#include <QDate>
#include <QWidget>

class ProjectModel;
struct Stage;
class QMouseEvent;
class QPainter;
class QResizeEvent;
class QScrollBar;
class QTreeWidget;
class QWheelEvent;

class TimelineWidget : public QWidget
{
	Q_OBJECT
public:
	explicit TimelineWidget(QScrollBar* horizontalScrollBar, QWidget* parent = nullptr);
	void setProject(const ProjectModel* project);
	void setTree(QTreeWidget* tree);
	void resetHorizontalScroll();

signals:
	void stageMoveRequested(const QString& stageId, int days);
	void stageActivated(const QString& stageId);
	void stageSelected(const QString& stageId);

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;

private:
	QString stageAt(const QPoint& position) const;
	qreal pixelsPerDay() const;
	qreal dateX(const QDate& date) const;
	QRectF stageBar(const Stage& stage, const QRect& row) const;
	void updateHorizontalScroll();
	void drawDateAxis(QPainter& painter) const;
	void drawStages(QPainter& painter) const;

	QScrollBar* m_horizontalScrollBar;
	QTreeWidget* m_tree = nullptr;
	const ProjectModel* m_project = nullptr;
	QDate m_axisStart;
	int m_axisDays = 0;
	QString m_draggedStage;
	QPoint m_pressPosition;
	bool m_dragging = false;
};
