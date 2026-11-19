#include "projectmodel.h"

#include <QSet>
#include <functional>

const Stage* ProjectModel::stage(const QString& id) const noexcept
{
	const auto it = m_stages.constFind(id);
	return it == m_stages.cend() ? nullptr : &it.value();
}

bool ProjectModel::fail(QString* error, const QString& message)
{
	if (error)
		*error = message;
	return false;
}

bool ProjectModel::isDescendantOf(const Stages& stages, const QString& id, const QString& ancestor)
{
	QSet<QString> visited;
	auto current = stages.constFind(id);
	while (current != stages.cend() && current->parentId) {
		const QString parent = *current->parentId;
		if (parent == ancestor)
			return true;
		if (visited.contains(parent))
			return false;
		visited.insert(parent);
		current = stages.constFind(parent);
	}
	return false;
}

bool ProjectModel::validateStage(const Stages& stages, const Stage& item, QString* error)
{
	if (item.id.trimmed().isEmpty())
		return fail(error, QStringLiteral("ID этапа не может быть пустым."));
	if (item.label.trimmed().isEmpty())
		return fail(error, QStringLiteral("Название этапа не может быть пустым."));
	if (!item.startDate.isValid() || !item.endDate.isValid() || item.startDate > item.endDate)
		return fail(error, QStringLiteral("Дата начала должна быть не позже даты окончания."));
	if (item.startDate < Stage::MinIsoDate || item.endDate > Stage::MaxIsoDate)
		return fail(error, QStringLiteral("Даты этапа «%1» должны быть в диапазоне 0001-01-01 — 9999-12-31.").arg(item.label));
	if (!item.color.isValid())
		return fail(error, QStringLiteral("У этапа задан некорректный цвет."));
	if (item.parentId) {
		if (*item.parentId == item.id || !stages.contains(*item.parentId))
			return fail(error, QStringLiteral("Родительский этап не существует или совпадает с этапом."));
		if (isDescendantOf(stages, *item.parentId, item.id))
			return fail(error, QStringLiteral("В иерархии этапов обнаружен цикл."));
		const Stage& parent = stages[*item.parentId];
		if (item.startDate < parent.startDate || item.endDate > parent.endDate)
			return fail(error, QStringLiteral("Дочерний этап должен целиком входить в даты родителя."));
	}
	QSet<QString> seenDependencies;
	for (const QString& id : item.dependsOn) {
		if (id == item.id || !stages.contains(id) || seenDependencies.contains(id))
			return fail(error, QStringLiteral("Некорректная или повторная зависимость этапа %1.").arg(item.id));
		seenDependencies.insert(id);
		if (stages[id].endDate > item.startDate)
			return fail(error, QStringLiteral("Этап «%1» зависит от этапа «%2»: окончание предшественника должно быть не позже начала зависимого этапа.").arg(item.label, stages[id].label));
	}
	return true;
}

bool ProjectModel::validate(const Stages& stages, QString* error)
{
	for (auto it = stages.cbegin(); it != stages.cend(); ++it) {
		if (it.key() != it->id)
			return fail(error, QStringLiteral("Ключ модели должен совпадать с ID этапа."));
		if (!validateStage(stages, it.value(), error))
			return false;
	}
	QSet<QString> visiting, visited;
	std::function<bool(const QString&)> hasCycle = [&](const QString& id) {
		if (visiting.contains(id)) return true;
		if (visited.contains(id)) return false;
		visiting.insert(id);
		for (const QString& dependency : stages[id].dependsOn)
			if (hasCycle(dependency)) return true;
		visiting.remove(id);
		visited.insert(id);
		return false;
	};
	for (auto it = stages.cbegin(); it != stages.cend(); ++it)
		if (hasCycle(it.key())) return fail(error, QStringLiteral("В зависимостях этапов обнаружен цикл."));
	return true;
}

bool ProjectModel::replaceStages(const Stages& stages, QString* error)
{
	if (!validate(stages, error))
		return false;
	m_stages = stages;
	return true;
}

bool ProjectModel::addStage(const Stage& item, QString* error)
{
	if (m_stages.contains(item.id))
		return fail(error, QStringLiteral("Этап с таким ID уже существует."));
	Stages candidate = m_stages;
	candidate.insert(item.id, item);
	if (!validate(candidate, error))
		return false;
	m_stages.swap(candidate);
	return true;
}

bool ProjectModel::updateStage(const Stage& item, QString* error)
{
	if (!m_stages.contains(item.id))
		return fail(error, QStringLiteral("Этап не найден."));
	Stages candidate = m_stages;
	candidate[item.id] = item;
	if (!validate(candidate, error))
		return false;
	m_stages.swap(candidate);
	return true;
}

bool ProjectModel::removeSubtree(const QString& id, QString* error)
{
	if (!m_stages.contains(id))
		return fail(error, QStringLiteral("Этап не найден."));
	QSet<QString> removed;
	for (auto it = m_stages.cbegin(); it != m_stages.cend(); ++it) {
		if (it.key() == id || isDescendantOf(m_stages, it.key(), id))
			removed.insert(it.key());
	}
	Stages candidate = m_stages;
	for (const QString& removedId : removed)
		candidate.remove(removedId);
	for (auto it = candidate.begin(); it != candidate.end(); ++it) {
		for (const QString& removedId : removed)
			it->dependsOn.removeAll(removedId);
	}
	if (!validate(candidate, error))
		return false;
	m_stages.swap(candidate);
	return true;
}

bool ProjectModel::changeDates(const QString& id, const QDate& start, const QDate& end, QString* error)
{
	if (!m_stages.contains(id))
		return fail(error, QStringLiteral("Этап не найден."));
	Stages candidate = m_stages;
	candidate[id].startDate = start;
	candidate[id].endDate = end;
	if (!validate(candidate, error))
		return false;
	m_stages.swap(candidate);
	return true;
}

bool ProjectModel::moveSubtree(const QString& id, int days, QString* error)
{
	if (!m_stages.contains(id))
		return fail(error, QStringLiteral("Этап не найден."));
	Stages candidate = m_stages;
	for (auto it = candidate.begin(); it != candidate.end(); ++it) {
		if (it.key() == id || isDescendantOf(candidate, it.key(), id)) {
			it->startDate = it->startDate.addDays(days);
			it->endDate = it->endDate.addDays(days);
		}
	}
	if (!validate(candidate, error))
		return false;
	m_stages.swap(candidate);
	return true;
}

