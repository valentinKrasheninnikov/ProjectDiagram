#pragma once

#include "stage.h"

#include <QDialog>

#include <functional>

class ProjectModel;
class QDateEdit;
class QLineEdit;
class QListWidget;
class QPushButton;

class StageFieldsDialog : public QDialog
{
public:
	using SubmitStage = std::function<bool(const Stage&, QString*)>;

	StageFieldsDialog(QWidget* parent, const QString& title, const Stage& initial,
					  const ProjectModel& project, const SubmitStage& submitStage);

private:
	Stage stageFromFields() const;
	void updateDependencyAvailability();
	void setColor(const QColor& color);

	Stage m_initial;
	QListWidget* m_dependencies = nullptr;
	QLineEdit* m_label = nullptr;
	QDateEdit* m_start = nullptr;
	QDateEdit* m_end = nullptr;
	QPushButton* m_colorButton = nullptr;
	QColor m_color;
};
