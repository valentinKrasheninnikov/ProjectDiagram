#pragma once

#include "stage.h"

#include <QMap>
#include <QString>

#include <optional>

class ProjectModel
{
public:
	using Stages = QMap<QString, Stage>;

	const Stages& stages() const noexcept { return m_stages; }
	const Stage* stage(const QString& id) const noexcept;
	void clear() { m_stages.clear(); }

	bool addStage(const Stage& stage, QString* error = nullptr);
	bool updateStage(const Stage& stage, QString* error = nullptr);
	bool removeSubtree(const QString& id, QString* error = nullptr);
	bool changeDates(const QString& id, const QDate& start, const QDate& end,
				 QString* error = nullptr);
	bool moveSubtree(const QString& id, int days, QString* error = nullptr);

	static bool validate(const Stages& stages, QString* error = nullptr);
	bool replaceStages(const Stages& stages, QString* error = nullptr);

private:
	static bool validateStage(const Stages& stages, const Stage& stage, QString* error);
	static bool isDescendantOf(const Stages& stages, const QString& id, const QString& ancestor);
	static bool fail(QString* error, const QString& message);
	Stages m_stages;
};
