#pragma once

#include <QColor>
#include <QDate>
#include <QString>
#include <QStringList>

#include <optional>

struct Stage
{
	inline static const QDate MinIsoDate{1, 1, 1};
	inline static const QDate MaxIsoDate{9999, 12, 31};

	QString id;
	QString label;
	QDate startDate;
	QDate endDate;
	QColor color = QColor(QStringLiteral("#4A90D9"));
	std::optional<QString> parentId;
	QStringList dependsOn;
};
