#include "mainwindow.h"
#include "projectjson.h"
#include "stagefieldsdialog.h"
#include "timelinewidget.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {
QIcon stageColorIcon(const QColor& color)
{
	QPixmap swatch(16, 16);
	swatch.fill(Qt::transparent);
	QPainter painter(&swatch);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(color.darker(145), 1));
	painter.setBrush(color);
	painter.drawRoundedRect(QRectF(2.5, 2.5, 11, 11), 3, 3);
	return QIcon(swatch);
}
}

MainWindow::MainWindow(QWidget* parent) : QWidget(parent), ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	setProjectFile({});
	resize(1180, 720);

	auto* rootLayout = new QVBoxLayout(this);
	rootLayout->setContentsMargins(8, 8, 8, 8);
	rootLayout->setSpacing(6);
	auto* commandBar = new QHBoxLayout;
	auto* openButton = new QPushButton(QStringLiteral("Открыть"), this);
	auto* saveButton = new QPushButton(QStringLiteral("Сохранить"), this);
	auto* saveAsButton = new QPushButton(QStringLiteral("Сохранить как"), this);
	commandBar->addWidget(openButton);
	commandBar->addWidget(saveButton);
	commandBar->addWidget(saveAsButton);
	commandBar->addStretch();
	rootLayout->addLayout(commandBar);

	auto* splitter = new QSplitter(Qt::Horizontal, this);
	m_tree = new QTreeWidget(splitter);
	m_tree->setColumnCount(1);
	m_tree->setHeaderLabel(QStringLiteral("Этап"));
	m_tree->header()->setFixedHeight(42);
	m_tree->setMinimumWidth(220);
	m_tree->setMaximumWidth(420);
	m_tree->setUniformRowHeights(true);
	m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	m_tree->setStyleSheet(QStringLiteral("QTreeWidget::item { height: 33px; }"));
	m_tree->viewport()->installEventFilter(this);
	m_tree->header()->installEventFilter(this);

	auto* timelineScrollBar = new QScrollBar(Qt::Horizontal, this);
	m_timeline = new TimelineWidget(timelineScrollBar, splitter);
	m_timeline->setTree(m_tree);
	splitter->addWidget(m_tree);
	splitter->addWidget(m_timeline);
	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({280, 850});
	rootLayout->addWidget(splitter, 1);
	rootLayout->addWidget(timelineScrollBar);

	connect(openButton, &QPushButton::clicked, this, &MainWindow::openProject);
	connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveProject);
	connect(saveAsButton, &QPushButton::clicked, this, &MainWindow::saveProjectAs);
	connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem*, int) { editSelectedStage(); });
	connect(m_timeline, &TimelineWidget::stageSelected, this, &MainWindow::selectTreeStage);
	connect(m_timeline, &TimelineWidget::stageActivated, this, [this](const QString& id) {
		selectTreeStage(id);
		editSelectedStage();
	});
	connect(m_timeline, &TimelineWidget::stageMoveRequested, this, &MainWindow::moveStage);
	refreshProjectView();
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::openProject()
{
	const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Открыть проект"), m_filePath,
										  QStringLiteral("Файлы проекта (*.json);;Все файлы (*)"));
	if (path.isEmpty() || !confirmUnsavedChanges())
		return;
	QString error;
	if (!ProjectJson::loadFile(path, &m_project, &error)) {
		QMessageBox::critical(this, QStringLiteral("Ошибка открытия проекта"), error);
		return;
	}
	setProjectFile(path);
	m_timeline->resetHorizontalScroll();
	refreshProjectView();
}

bool MainWindow::saveProject()
{
	return m_filePath.isEmpty() ? saveProjectAs() : saveProjectTo(m_filePath);
}

bool MainWindow::saveProjectAs()
{
	QFileDialog dialog(this, QStringLiteral("Сохранить проект как"), {},
					   QStringLiteral("Файлы проекта (*.json);;Все файлы (*)"));
	dialog.setAcceptMode(QFileDialog::AcceptSave);
	dialog.setDefaultSuffix(QStringLiteral("json"));
	if (!m_filePath.isEmpty())
		dialog.selectFile(m_filePath);
	if (dialog.exec() != QDialog::Accepted)
		return false;
	return saveProjectTo(dialog.selectedFiles().first());
}

bool MainWindow::saveProjectTo(const QString& path)
{
	QString error;
	if (!ProjectJson::saveFile(path, m_project, &error)) {
		QMessageBox::critical(this, QStringLiteral("Ошибка сохранения проекта"), error);
		return false;
	}
	setProjectFile(path);
	return true;
}

bool MainWindow::confirmUnsavedChanges()
{
	if (!isWindowModified())
		return true;

	QMessageBox prompt(QMessageBox::Warning, QStringLiteral("Несохранённые изменения"),
					   QStringLiteral("Сохранить изменения проекта?"),
					   QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
	prompt.setDefaultButton(QMessageBox::Save);
	prompt.setEscapeButton(QMessageBox::Cancel);
	prompt.button(QMessageBox::Save)->setText(QStringLiteral("Сохранить"));
	prompt.button(QMessageBox::Discard)->setText(QStringLiteral("Не сохранять"));
	prompt.button(QMessageBox::Cancel)->setText(QStringLiteral("Отмена"));
	const int choice = prompt.exec();
	if (choice == QMessageBox::Save)
		return saveProject();
	return choice == QMessageBox::Discard;
}

void MainWindow::setProjectFile(const QString& path)
{
	m_filePath = path;
	const QString title = QStringLiteral("Диаграмма проекта");
	setWindowTitle(path.isEmpty() ? title + QStringLiteral("[*]")
								 : QStringLiteral("%1 — %2[*]").arg(title, path));
	setWindowModified(false);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	event->setAccepted(confirmUnsavedChanges());
}

void MainWindow::addStage(bool asChild)
{
	const QString parentId = asChild && m_tree->currentItem()
		? m_tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
	if (asChild && parentId.isEmpty()) {
		QMessageBox::information(this, QStringLiteral("Этап не выбран"), QStringLiteral("Выберите родительский этап в дереве."));
		return;
	}
	const Stage* parent = parentId.isEmpty() ? nullptr : m_project.stage(parentId);

	Stage draft;
	draft.id = QStringLiteral("stage-1");
	for (int suffix = 1; m_project.stages().contains(QStringLiteral("stage-%1").arg(suffix)); ++suffix)
		draft.id = QStringLiteral("stage-%1").arg(suffix + 1);
	draft.label = QStringLiteral("Новый этап");
	draft.startDate = parent ? parent->startDate : QDate::currentDate();
	draft.endDate = qMin(draft.startDate.addDays(1), parent ? parent->endDate : Stage::MaxIsoDate);
	if (parent) draft.parentId = parentId;
	StageFieldsDialog dialog(this, parent ? QStringLiteral("Добавить подэтап") : QStringLiteral("Добавить этап"),
							 draft, m_project, [this](const Stage& stage, QString* error) {
		return m_project.addStage(stage, error);
	});
	if (dialog.exec() != QDialog::Accepted)
		return;
	setWindowModified(true);
	refreshProjectView();
	selectTreeStage(draft.id);
}

void MainWindow::editSelectedStage()
{
	if (!m_tree->currentItem()) {
		QMessageBox::information(this, QStringLiteral("Этап не выбран"), QStringLiteral("Выберите этап в дереве или на диаграмме."));
		return;
	}
	const QString id = m_tree->currentItem()->data(0, Qt::UserRole).toString();
	const Stage* current = m_project.stage(id);
	if (!current)
		return;
	StageFieldsDialog dialog(this, QStringLiteral("Редактировать этап"), *current, m_project,
							 [this](const Stage& stage, QString* error) {
		return m_project.updateStage(stage, error);
	});
	if (dialog.exec() != QDialog::Accepted)
		return;
	setWindowModified(true);
	refreshProjectView();
	selectTreeStage(id);
}

void MainWindow::moveStage(const QString& stageId, int days)
{
	QString error;
	if (!m_project.moveSubtree(stageId, days, &error)) {
		QMessageBox::warning(this, QStringLiteral("Перенос отклонён"), error);
		return;
	}
	setWindowModified(true);
	refreshProjectView();
	selectTreeStage(stageId);
}

void MainWindow::showTreeContextMenu(const QPoint& position, const QPoint& globalPosition, bool onHeader)
{
	QTreeWidgetItem* item = !onHeader && m_tree->viewport()->rect().contains(position)
		? m_tree->itemAt(position) : nullptr;
	if (item)
		m_tree->setCurrentItem(item);
	else
		m_tree->setCurrentItem(nullptr);

	QMenu menu(this);
	QAction* addRoot = menu.addAction(QStringLiteral("Добавить этап"));
	QAction* addChild = nullptr;
	QAction* edit = nullptr;
	QAction* remove = nullptr;
	if (item) {
		addChild = menu.addAction(QStringLiteral("Добавить подэтап"));
		edit = menu.addAction(QStringLiteral("Редактировать этап"));
		remove = menu.addAction(QStringLiteral("Удалить этап"));
	}
	QAction* selected = menu.exec(globalPosition);
	if (!selected)
		return;
	if (selected == addRoot)
		addStage(false);
	else if (selected == addChild)
		addStage(true);
	else if (selected == edit)
		editSelectedStage();
	else if (remove && selected == remove) {
		const QString id = item->data(0, Qt::UserRole).toString();
		const QString label = item->text(0);
		const QString message = QStringLiteral("Удалить этап «%1» и все его подэтапы?\nЗависимости других этапов от удалённых также будут удалены.").arg(label);
		if (QMessageBox::question(this, QStringLiteral("Удаление этапа"), message,
				QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;
		QString error;
		if (!m_project.removeSubtree(id, &error)) {
			QMessageBox::warning(this, QStringLiteral("Нельзя удалить этап"), error);
			return;
		}
		setWindowModified(true);
		refreshProjectView();
	}
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
	if (m_tree && (watched == m_tree->viewport() || watched == m_tree->header())
		&& event->type() == QEvent::ContextMenu) {
		const auto* contextEvent = static_cast<QContextMenuEvent*>(event);
		const QPoint position = m_tree->viewport()->mapFromGlobal(contextEvent->globalPos());
		const bool onHeader = watched == m_tree->header()
			|| m_tree->header()->rect().contains(m_tree->header()->mapFromGlobal(contextEvent->globalPos()));
		showTreeContextMenu(position, contextEvent->globalPos(), onHeader);
		return true;
	}
	return QWidget::eventFilter(watched, event);
}

void MainWindow::refreshProjectView()
{
	m_itemsById.clear();
	m_tree->clear();
	for (const Stage& stage : m_project.stages()) {
		auto* item = new QTreeWidgetItem;
		item->setText(0, stage.label);
		item->setIcon(0, stageColorIcon(stage.color));
		item->setData(0, Qt::UserRole, stage.id);
		item->setToolTip(0, QStringLiteral("%1 — %2").arg(stage.startDate.toString(Qt::ISODate), stage.endDate.toString(Qt::ISODate)));
		m_itemsById.insert(stage.id, item);
	}
	for (const Stage& stage : m_project.stages()) {
		auto* item = m_itemsById.value(stage.id);
		if (stage.parentId)
			m_itemsById.value(*stage.parentId)->addChild(item);
		else
			m_tree->addTopLevelItem(item);
	}
	m_tree->expandAll();
	m_timeline->setProject(&m_project);
}

void MainWindow::selectTreeStage(const QString& stageId)
{
	if (QTreeWidgetItem* item = m_itemsById.value(stageId)) {
		m_tree->setCurrentItem(item);
		m_tree->scrollToItem(item);
	}
}
