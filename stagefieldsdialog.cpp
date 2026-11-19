#include "stagefieldsdialog.h"

#include "projectmodel.h"

#include <QColorDialog>
#include <QDateEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>

StageFieldsDialog::StageFieldsDialog(QWidget* parent, const QString& title, const Stage& initial,
                                     const ProjectModel& project, const SubmitStage& submitStage)
	: QDialog(parent), m_initial(initial)
{
	setWindowTitle(title);
	auto* form = new QFormLayout(this);
	m_label = new QLineEdit(initial.label, this);
	m_start = new QDateEdit(this);
	m_end = new QDateEdit(this);
	m_start->setDateRange(Stage::MinIsoDate, Stage::MaxIsoDate);
	m_end->setDateRange(Stage::MinIsoDate, Stage::MaxIsoDate);
	m_start->setDate(initial.startDate);
	m_end->setDate(initial.endDate);
	m_start->setCalendarPopup(true);
	m_end->setCalendarPopup(true);
	m_start->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
	m_end->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
	m_colorButton = new QPushButton(this);
	m_colorButton->setAutoDefault(false);
	m_colorButton->setAccessibleName(QStringLiteral("Цвет этапа"));
	m_colorButton->setToolTip(QStringLiteral("Нажмите, чтобы выбрать цвет этапа"));
	m_colorButton->setMinimumHeight(30);
	setColor(initial.color);
	connect(m_colorButton, &QPushButton::clicked, this, [this] {
		const QColor selected = QColorDialog::getColor(m_color, this, QStringLiteral("Цвет этапа"));
		if (selected.isValid()) setColor(selected);
	});
	form->addRow(QStringLiteral("Название"), m_label);
	form->addRow(QStringLiteral("Дата начала"), m_start);
	form->addRow(QStringLiteral("Дата окончания"), m_end);
	form->addRow(QStringLiteral("Цвет"), m_colorButton);
	m_dependencies = new QListWidget(this);
	m_dependencies->setMaximumHeight(120);
	for (const Stage& stage : project.stages()) {
		if (stage.id == initial.id) continue;
		auto* item = new QListWidgetItem(
			QStringLiteral("%1 [%2]  %3 — %4").arg(stage.label, stage.id,
				stage.startDate.toString(QStringLiteral("dd.MM.yyyy")),
				stage.endDate.toString(QStringLiteral("dd.MM.yyyy"))), m_dependencies);
		item->setData(Qt::UserRole, stage.id);
		item->setData(Qt::UserRole + 1, stage.endDate);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(initial.dependsOn.contains(stage.id) ? Qt::Checked : Qt::Unchecked);
	}
	form->addRow(QStringLiteral("Зависит от"), m_dependencies);
	auto* dependencyHint = new QLabel(QStringLiteral("Выберите этапы, которые должны завершиться не позже начала этого этапа."), this);
	dependencyHint->setWordWrap(true);
	form->addRow(QString(), dependencyHint);
	connect(m_start, &QDateEdit::dateChanged, this, [this] { updateDependencyAvailability(); });
	connect(m_dependencies, &QListWidget::itemChanged, this, [this] { updateDependencyAvailability(); });
	updateDependencyAvailability();
	auto* actions = new QHBoxLayout;
	actions->addStretch();
	auto* okButton = new QPushButton(QStringLiteral("Ок"), this);
	auto* cancelButton = new QPushButton(QStringLiteral("Отмена"), this);
	okButton->setDefault(true);
	cancelButton->setAutoDefault(false);
	actions->addWidget(okButton);
	actions->addWidget(cancelButton);
	form->addRow(actions);
	connect(okButton, &QPushButton::clicked, this, [this, submitStage] {
		QString error;
		if (!submitStage(stageFromFields(), &error)) {
			QMessageBox::warning(this, QStringLiteral("Нельзя сохранить этап"), error);
			return;
		}
		accept();
	});
	connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

Stage StageFieldsDialog::stageFromFields() const
{
	Stage result = m_initial;
	result.label = m_label->text().trimmed();
	result.startDate = m_start->date();
	result.endDate = m_end->date();
	result.color = m_color;
	result.dependsOn.clear();
	for (int i = 0; i < m_dependencies->count(); ++i)
		if (m_dependencies->item(i)->checkState() == Qt::Checked)
			result.dependsOn.append(m_dependencies->item(i)->data(Qt::UserRole).toString());
	return result;
}

void StageFieldsDialog::updateDependencyAvailability()
{
	const QSignalBlocker blocker(m_dependencies);
	for (int i = 0; i < m_dependencies->count(); ++i) {
		QListWidgetItem* item = m_dependencies->item(i);
		const bool fits = item->data(Qt::UserRole + 1).toDate() <= m_start->date();
		const bool checked = item->checkState() == Qt::Checked;
		Qt::ItemFlags flags = item->flags();
		flags.setFlag(Qt::ItemIsEnabled, fits || checked);
		item->setFlags(flags);
		item->setToolTip(fits ? QString() : QStringLiteral("Этап заканчивается позже начала текущего этапа."));
	}
}

void StageFieldsDialog::setColor(const QColor& color)
{
	m_color = color;
	m_colorButton->setText(QString());
	m_colorButton->setStyleSheet(QStringLiteral(
		"QPushButton { background-color: %1; border: 1px solid %2; border-radius: 4px; }"
		"QPushButton:hover { background-color: %3; }"
		"QPushButton:pressed { background-color: %4; }")
		.arg(color.name(QColor::HexRgb))
		.arg(color.darker(150).name(QColor::HexRgb))
		.arg(color.darker(115).name(QColor::HexRgb))
		.arg(color.darker(130).name(QColor::HexRgb)));
}
