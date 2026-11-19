#pragma once

#include <QString>

class ProjectModel;

class ProjectJson
{
public:
	static bool loadFile(const QString& filePath, ProjectModel* project, QString* error = nullptr);
	static bool saveFile(const QString& filePath, const ProjectModel& project, QString* error = nullptr);
};
