#include "timelinewidget.h"

#include "projectmodel.h"

#include <QApplication>
#include <QHash>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTreeWidget>
#include <QVector>
#include <QWheelEvent>
#include <QtMath>

namespace {
constexpr int HeaderHeight = 42;
constexpr int MinRowHeight = 48;
constexpr int MinPixelsPerDay = 12;
constexpr int LeftMargin = 12;
constexpr int RightMargin = 24;
constexpr int BarHeight = 18;

struct StageRow
{
	const Stage* stage;
	QRectF bar;
	bool hasChildren;
};

void drawDependency(QPainter& painter, const QRectF& predecessor, const QRectF& successor, const QRect& viewport)
{
	const QPointF from(predecessor.right(), predecessor.center().y());
	const QPointF to(successor.left() - 2, successor.center().y());
	const qreal bendX = from.x() + qMax<qreal>(5, (to.x() - from.x()) / 2);
	QPolygonF path;
	if (bendX <= to.x() - 6) {
		path << from << QPointF(bendX, from.y()) << QPointF(bendX, to.y()) << to;
	} else {
		const qreal betweenRows = (from.y() + to.y()) / 2;
		path << from << QPointF(bendX, from.y()) << QPointF(bendX, betweenRows)
			 << QPointF(to.x() - 8, betweenRows) << QPointF(to.x() - 8, to.y()) << to;
	}

	const QRectF drawBounds = viewport.adjusted(-12, -12, 12, 12);
	for (QPointF& point : path) {
		point.setX(qBound(drawBounds.left(), point.x(), drawBounds.right()));
		point.setY(qBound(drawBounds.top(), point.y(), drawBounds.bottom()));
	}
	painter.drawPolyline(path);
	if (drawBounds.contains(to)) {
		QPolygonF arrow;
		arrow << to << QPointF(to.x() - 6, to.y() - 3) << QPointF(to.x() - 6, to.y() + 3);
		painter.drawPolygon(arrow);
	}
}
}

TimelineWidget::TimelineWidget(QScrollBar* horizontalScrollBar, QWidget* parent)
	: QWidget(parent), m_horizontalScrollBar(horizontalScrollBar)
{
	setMinimumHeight(HeaderHeight + MinRowHeight);
	setMouseTracking(true);
	connect(m_horizontalScrollBar, &QScrollBar::valueChanged, this, QOverload<>::of(&TimelineWidget::update));
}

void TimelineWidget::setTree(QTreeWidget* tree)
{
	m_tree = tree;
	connect(tree->verticalScrollBar(), &QScrollBar::valueChanged, this, QOverload<>::of(&TimelineWidget::update));
	connect(tree, &QTreeWidget::itemExpanded, this, [this] { update(); });
	connect(tree, &QTreeWidget::itemCollapsed, this, [this] { update(); });
	connect(tree, &QTreeWidget::itemSelectionChanged, this, QOverload<>::of(&TimelineWidget::update));
}

void TimelineWidget::setProject(const ProjectModel* project)
{
	m_project = project;
	m_axisStart = {};
	m_axisDays = 0;
	if (project && !project->stages().isEmpty()) {
		QDate first = project->stages().first().startDate;
		QDate last = project->stages().first().endDate;
		for (const Stage& stage : project->stages()) {
			first = qMin(first, stage.startDate);
			last = qMax(last, stage.endDate);
		}
		m_axisStart = QDate(first.year(), first.month(), 1);
		const QDate endOfLastMonth(last.year(), last.month(), last.daysInMonth());
		m_axisDays = m_axisStart.daysTo(endOfLastMonth) + 1;
	}
	updateHorizontalScroll();
	update();
}

void TimelineWidget::resetHorizontalScroll()
{
	m_horizontalScrollBar->setValue(0);
}

void TimelineWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	updateHorizontalScroll();
}

void TimelineWidget::updateHorizontalScroll()
{
	const int contentWidth = LeftMargin + m_axisDays * MinPixelsPerDay + RightMargin;
	const int scrollLimit = m_axisDays > 0 ? qMax(0, contentWidth - width()) : 0;
	m_horizontalScrollBar->setRange(0, scrollLimit);
	m_horizontalScrollBar->setPageStep(width());
	m_horizontalScrollBar->setSingleStep(MinPixelsPerDay);
	m_horizontalScrollBar->setVisible(scrollLimit > 0);
}

qreal TimelineWidget::pixelsPerDay() const
{
	return qMax<qreal>(MinPixelsPerDay, (width() - LeftMargin - RightMargin) / static_cast<qreal>(m_axisDays));
}

qreal TimelineWidget::dateX(const QDate& date) const
{
	return LeftMargin + m_axisStart.daysTo(date) * pixelsPerDay() - m_horizontalScrollBar->value();
}

QRectF TimelineWidget::stageBar(const Stage& stage, const QRect& row) const
{
	const qreal startX = dateX(stage.startDate);
	const qreal endX = dateX(stage.endDate.addDays(1));
	return QRectF(startX, row.y() + (row.height() - BarHeight) / 2.0, qMax<qreal>(8, endX - startX - 1), BarHeight);
}

QString TimelineWidget::stageAt(const QPoint& position) const
{
	if (!m_axisDays || !m_tree || position.y() < HeaderHeight || !rect().contains(position))
		return {};

	QWidget* viewport = m_tree->viewport();
	const QPoint treeOrigin = mapFromGlobal(viewport->mapToGlobal(QPoint()));
	const int treeY = position.y() - treeOrigin.y();
	const auto* item = m_tree->itemAt(QPoint(0, treeY));
	if (!item)
		return {};
	const Stage* stage = m_project->stage(item->data(0, Qt::UserRole).toString());
	const QRect row = m_tree->visualItemRect(item).translated(treeOrigin);
	return stage && stageBar(*stage, row).contains(position) ? stage->id : QString();
}

void TimelineWidget::wheelEvent(QWheelEvent* event)
{
	if (event->angleDelta().x()) {
		m_horizontalScrollBar->setValue(m_horizontalScrollBar->value() - event->angleDelta().x());
		event->accept();
		return;
	}
	if (m_tree && event->angleDelta().y()) {
		auto* scrollBar = m_tree->verticalScrollBar();
		scrollBar->setValue(scrollBar->value() - event->angleDelta().y());
		event->accept();
		return;
	}
	QWidget::wheelEvent(event);
}

void TimelineWidget::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton) {
		m_draggedStage = stageAt(event->pos());
		m_pressPosition = event->pos() + QPoint(m_horizontalScrollBar->value(), 0);
		m_dragging = false;
		if (!m_draggedStage.isEmpty())
			emit stageSelected(m_draggedStage);
	}
	QWidget::mousePressEvent(event);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (!m_draggedStage.isEmpty() && (event->buttons() & Qt::LeftButton)) {
		const QPoint position = event->pos() + QPoint(m_horizontalScrollBar->value(), 0);
		if ((position - m_pressPosition).manhattanLength() >= QApplication::startDragDistance())
			m_dragging = true;
		setCursor(m_dragging ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
	} else {
		setCursor(stageAt(event->pos()).isEmpty() ? Qt::ArrowCursor : Qt::OpenHandCursor);
	}
	QWidget::mouseMoveEvent(event);
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton && !m_draggedStage.isEmpty() && m_dragging && m_axisDays) {
		const int releaseX = event->pos().x() + m_horizontalScrollBar->value();
		const int days = qRound((releaseX - m_pressPosition.x()) / pixelsPerDay());
		if (days != 0)
			emit stageMoveRequested(m_draggedStage, days);
	}
	m_draggedStage.clear();
	m_dragging = false;
	setCursor(Qt::ArrowCursor);
	QWidget::mouseReleaseEvent(event);
}

void TimelineWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	const QString id = stageAt(event->pos());
	if (!id.isEmpty())
		emit stageActivated(id);
	QWidget::mouseDoubleClickEvent(event);
}

void TimelineWidget::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.fillRect(rect(), Qt::white);
	if (!m_axisDays) {
		painter.setPen(QColor(105, 116, 130));
		painter.drawText(rect().adjusted(12, HeaderHeight, -24, 0), Qt::AlignLeft | Qt::AlignVCenter,
						 QStringLiteral("Откройте JSON-файл проекта, чтобы увидеть временную шкалу."));
		return;
	}
	drawDateAxis(painter);
	if (m_tree)
		drawStages(painter);
}

void TimelineWidget::drawDateAxis(QPainter& painter) const
{
	painter.fillRect(QRect(0, 0, width(), HeaderHeight), QColor(245, 247, 250));
	painter.setFont(QFont(QStringLiteral("Segoe UI"), 9, QFont::DemiBold));
	const QLocale russian(QLocale::Russian);
	const qreal dayWidth = pixelsPerDay();
	const int firstVisibleDay = qMax(0, qFloor((m_horizontalScrollBar->value() - LeftMargin) / dayWidth));
	const QDate firstVisibleDate = m_axisStart.addDays(firstVisibleDay);
	const QDate axisEnd = m_axisStart.addDays(m_axisDays - 1);
	for (QDate month(firstVisibleDate.year(), firstVisibleDate.month(), 1);
		 month <= axisEnd && dateX(month) < width(); month = month.addMonths(1)) {
		const qreal x = dateX(month);
		const qreal nextX = dateX(month.addMonths(1));
		QString monthName = russian.standaloneMonthName(month.month(), QLocale::LongFormat).left(3);
		monthName[0] = monthName.at(0).toUpper();
		const QString label = QStringLiteral("%1 %2").arg(monthName).arg(month.year());
		painter.setPen(QColor(55, 65, 81));
		painter.drawLine(QPointF(x, 0), QPointF(x, height()));
		painter.drawText(QRectF(x + 8, 0, qMax<qreal>(0, nextX - x - 12), HeaderHeight - 2),
						 Qt::AlignVCenter, label);
	}
	painter.setPen(QColor(240, 242, 245));
	for (int day = (firstVisibleDay / 7) * 7; day < m_axisDays; day += 7) {
		const qreal x = dateX(m_axisStart.addDays(day));
		if (x >= width())
			break;
		painter.drawLine(QPointF(x, HeaderHeight), QPointF(x, height()));
	}
}

void TimelineWidget::drawStages(QPainter& painter) const
{
	const QRect rowViewport(0, HeaderHeight, width(), height() - HeaderHeight);
	const QPoint treeOrigin = mapFromGlobal(m_tree->viewport()->mapToGlobal(QPoint()));
	painter.save();
	painter.setClipRect(rowViewport);
	QVector<StageRow> rows;
	QHash<QString, QRectF> barsById;
	for (QTreeWidgetItem* item = m_tree->topLevelItem(0); item; item = m_tree->itemBelow(item)) {
		const Stage* stage = m_project->stage(item->data(0, Qt::UserRole).toString());
		if (!stage)
			continue;
		QRect band = m_tree->visualItemRect(item).translated(treeOrigin);
		const QRectF bar = stageBar(*stage, band);
		band.setLeft(0);
		band.setWidth(width());
		rows.append(StageRow{stage, bar, item->childCount() > 0});
		barsById.insert(stage->id, bar);

		if (!band.intersects(rowViewport))
			continue;
		if (item->isSelected())
			painter.fillRect(band, QColor(225, 235, 247));
		painter.setPen(QColor(230, 233, 238));
		painter.drawLine(0, band.bottom() + 1, width(), band.bottom() + 1);
	}

	painter.setPen(QPen(QColor(90, 105, 125), 1.4));
	painter.setBrush(QColor(90, 105, 125));
	for (const StageRow& row : rows) {
		for (const QString& dependency : row.stage->dependsOn) {
			const auto predecessor = barsById.constFind(dependency);
			if (predecessor != barsById.cend())
				drawDependency(painter, *predecessor, row.bar, rowViewport);
		}
	}

	for (const StageRow& row : rows) {
		if (!row.bar.intersects(rowViewport))
			continue;
		QRectF bar = row.bar;
		bar.setLeft(qMax<qreal>(bar.left(), -4));
		bar.setRight(qMin<qreal>(bar.right(), width() + 4));
		painter.setPen(row.stage->color.darker(125));
		painter.setBrush(row.stage->color);
		if (row.hasChildren) {
			const QRectF summary(bar.left(), bar.center().y() - 3, bar.width(), 6);
			painter.drawRect(summary);
			painter.drawLine(summary.bottomLeft(), summary.bottomLeft() + QPointF(0, 4));
			painter.drawLine(summary.bottomRight(), summary.bottomRight() + QPointF(0, 4));
		} else {
			painter.drawRoundedRect(bar, 3, 3);
		}
	}
	painter.restore();
}

