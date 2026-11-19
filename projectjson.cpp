#include "projectjson.h"

#include "projectmodel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
constexpr int FormatVersion = 1;

bool fail(QString* error, const QString& message)
{
	if (error)
		*error = message;
	return false;
}

bool readRequiredString(const QJsonObject& object, const QString& key, QString* value,
					   QString* error, const QString& stageId)
{
	const QJsonValue field = object.value(key);
	if (!field.isString())
		return fail(error, QStringLiteral("У этапа '%1' поле '%2' должно быть строкой.").arg(stageId, key));
	*value = field.toString();
	return true;
}
}

bool ProjectJson::loadFile(const QString& filePath, ProjectModel* project, QString* error)
{
	if (!project)
		return fail(error, QStringLiteral("Не задана модель проекта."));
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly))
		return fail(error, QStringLiteral("Не удалось открыть файл '%1': %2").arg(filePath, file.errorString()));

	QJsonParseError parseError;
	const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError)
		return fail(error, QStringLiteral("Ошибка JSON (позиция %1): %2").arg(parseError.offset).arg(parseError.errorString()));
	if (!document.isObject())
		return fail(error, QStringLiteral("Корневое значение JSON должно быть объектом."));

	const QJsonObject root = document.object();
	const QJsonValue version = root.value(QStringLiteral("formatVersion"));
	if (!version.isDouble() || version.toInt(-1) != FormatVersion)
		return fail(error, QStringLiteral("Неподдерживаемая или отсутствующая версия формата проекта."));
	const QJsonValue stagesValue = root.value(QStringLiteral("stages"));
	if (!stagesValue.isArray())
		return fail(error, QStringLiteral("Поле 'stages' должно быть массивом."));

	ProjectModel::Stages parsedStages;
	const QJsonArray stages = stagesValue.toArray();
	for (int index = 0; index < stages.size(); ++index) {
		if (!stages.at(index).isObject())
			return fail(error, QStringLiteral("Элемент stages[%1] должен быть объектом.").arg(index));
		const QJsonObject object = stages.at(index).toObject();
		Stage stage;
		if (!readRequiredString(object, QStringLiteral("id"), &stage.id, error, QString::number(index))
			|| !readRequiredString(object, QStringLiteral("label"), &stage.label, error, stage.id))
			return false;

		QString startText;
		QString endText;
		QString colorText;
		if (!readRequiredString(object, QStringLiteral("startDate"), &startText, error, stage.id)
			|| !readRequiredString(object, QStringLiteral("endDate"), &endText, error, stage.id)
			|| !readRequiredString(object, QStringLiteral("color"), &colorText, error, stage.id))
			return false;
		stage.startDate = QDate::fromString(startText, Qt::ISODate);
		stage.endDate = QDate::fromString(endText, Qt::ISODate);
		if (!stage.startDate.isValid() || stage.startDate.toString(Qt::ISODate) != startText
			|| !stage.endDate.isValid() || stage.endDate.toString(Qt::ISODate) != endText)
			return fail(error, QStringLiteral("Даты этапа '%1' должны быть в формате yyyy-MM-dd.").arg(stage.id));
		static const QRegularExpression rgbColor(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
		if (!rgbColor.match(colorText).hasMatch())
			return fail(error, QStringLiteral("Цвет этапа '%1' должен быть в формате #RRGGBB.").arg(stage.id));
		stage.color = QColor(colorText);

		const QJsonValue parent = object.value(QStringLiteral("parentId"));
		if (parent.isString())
			stage.parentId = parent.toString();
		else if (!parent.isNull() && !parent.isUndefined())
			return fail(error, QStringLiteral("Поле parentId этапа '%1' должно быть строкой или null.").arg(stage.id));

		const QJsonValue dependencies = object.value(QStringLiteral("dependsOn"));
		if (!dependencies.isUndefined()) {
			if (!dependencies.isArray())
				return fail(error, QStringLiteral("Поле dependsOn этапа '%1' должно быть массивом.").arg(stage.id));
			for (const QJsonValue& dependency : dependencies.toArray()) {
				if (!dependency.isString())
					return fail(error, QStringLiteral("Зависимость этапа '%1' должна быть строкой.").arg(stage.id));
				stage.dependsOn.append(dependency.toString());
			}
		}
		if (parsedStages.contains(stage.id))
			return fail(error, QStringLiteral("Повторяющийся ID этапа '%1'.").arg(stage.id));
		parsedStages.insert(stage.id, stage);
	}

	return project->replaceStages(parsedStages, error);
}

bool ProjectJson::saveFile(const QString& filePath, const ProjectModel& project, QString* error)
{
	if (!ProjectModel::validate(project.stages(), error))
		return false;
	QJsonArray stages;
	for (const Stage& stage : project.stages()) {
		QJsonObject object{
			{QStringLiteral("id"), stage.id},
			{QStringLiteral("label"), stage.label},
			{QStringLiteral("startDate"), stage.startDate.toString(Qt::ISODate)},
			{QStringLiteral("endDate"), stage.endDate.toString(Qt::ISODate)},
			{QStringLiteral("color"), stage.color.name(QColor::HexRgb)},
			{QStringLiteral("parentId"), stage.parentId ? QJsonValue(*stage.parentId) : QJsonValue(QJsonValue::Null)}
		};
		QJsonArray dependencies;
		for (const QString& id : stage.dependsOn) dependencies.append(id);
		object.insert(QStringLiteral("dependsOn"), dependencies);
		stages.append(object);
	}
	const QJsonObject root{
		{QStringLiteral("formatVersion"), FormatVersion},
		{QStringLiteral("stages"), stages}
	};
	QSaveFile file(filePath);
	if (!file.open(QIODevice::WriteOnly))
		return fail(error, QStringLiteral("Не удалось сохранить файл '%1': %2").arg(filePath, file.errorString()));
	if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) == -1) {
		const QString message = file.errorString();
		file.cancelWriting();
		return fail(error, QStringLiteral("Ошибка записи файла '%1': %2").arg(filePath, message));
	}
	if (!file.commit())
		return fail(error, QStringLiteral("Не удалось завершить сохранение файла '%1': %2").arg(filePath, file.errorString()));
	return true;
}
