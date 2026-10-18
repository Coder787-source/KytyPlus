#include "cheatFile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QXmlStreamReader>

namespace Cheats {

namespace {

QString Tr(const char* text) {
	return QCoreApplication::translate("Cheats", text);
}

QString ValidateDocument(const QJsonDocument& document) {
	if (!document.isObject()) {
		return Tr("Invalid cheat JSON.");
	}
	const auto mods = document.object().value(QStringLiteral("mods"));
	if (!mods.isArray()) {
		return Tr("Cheat JSON has no \"mods\" array.");
	}
	for (const auto& value: mods.toArray()) {
		const auto mod     = value.toObject();
		const auto name    = mod.value(QStringLiteral("name"));
		const auto enabled = mod.value(QStringLiteral("enabled"));
		if (!value.isObject() || !name.isString() || name.toString().isEmpty() ||
		    (!enabled.isUndefined() && !enabled.isBool())) {
			return Tr("Invalid cheat entry.");
		}
	}
	return {};
}

// Retains hex digits only, tolerating the dash-separated byte style used by trainer XML.
QString HexDigits(const QString& text) {
	QString out;
	for (const auto character: text) {
		const auto latin = character.toLatin1();
		if ((latin >= '0' && latin <= '9') || (latin >= 'a' && latin <= 'f') ||
		    (latin >= 'A' && latin <= 'F')) {
			out += character.toLower();
		} else if (character != QLatin1Char('-') && character != QLatin1Char(' ') &&
		           character != QLatin1Char('_')) {
			return {};
		}
	}
	return out;
}

// ShenJS trainer XML ("*.shn") describes the same absolute writes as the GoldHEN-style mods
// JSON, so converting here keeps one cheat representation for the rest of the pipeline.
QString ParseTrainerXml(const QByteArray& data, QJsonDocument* document) {
	QXmlStreamReader xml(data);
	if (!xml.readNextStartElement() || xml.name() != QLatin1String("Trainer")) {
		return Tr("Not a supported cheat file.");
	}
	const auto attribute = [&xml](const char* name) {
		return xml.attributes().value(QLatin1String(name)).toString().trimmed();
	};
	auto process = attribute("Process");
	if (process.isEmpty()) {
		process = QStringLiteral("eboot.bin");
	}

	QJsonArray mods;
	QJsonArray lines;
	QJsonObject line;
	QString    cheat_name;
	bool       in_cheat = false;
	bool       in_line  = false;
	while (!xml.atEnd()) {
		const auto token = xml.readNext();
		if (token == QXmlStreamReader::StartElement) {
			const auto name = xml.name().toString();
			if (name == QLatin1String("Cheat")) {
				in_cheat   = true;
				cheat_name = xml.attributes().value(QLatin1String("Text")).toString().trimmed();
				lines      = QJsonArray();
				continue;
			}
			if (name == QLatin1String("Cheatline")) {
				in_line = true;
				line    = QJsonObject();
				continue;
			}
			if (!in_line) {
				continue;
			}
			const auto value = xml.readElementText().trimmed();
			if (name == QLatin1String("Offset")) {
				line[QStringLiteral("offset")] = HexDigits(value).rightJustified(8, QLatin1Char('0'));
			} else if (name == QLatin1String("ValueOn")) {
				line[QStringLiteral("on")] = HexDigits(value);
			} else if (name == QLatin1String("ValueOff")) {
				line[QStringLiteral("off")] = HexDigits(value);
			} else if (name == QLatin1String("Section") && !value.isEmpty() &&
			           value != QLatin1String("0")) {
				// A mods plan cannot select a section, so such lines are dropped below.
				line[QStringLiteral("section")] = value;
			}
			continue;
		}
		if (token != QXmlStreamReader::EndElement) {
			continue;
		}
		const auto name = xml.name().toString();
		if (name == QLatin1String("Cheatline") && in_line) {
			in_line = false;
			if (!line.contains(QStringLiteral("section")) &&
			    !line.value(QStringLiteral("offset")).toString().isEmpty() &&
			    line.contains(QStringLiteral("on")) && line.contains(QStringLiteral("off"))) {
				lines.append(line);
			}
			continue;
		}
		if (name == QLatin1String("Cheat") && in_cheat) {
			in_cheat = false;
			if (!cheat_name.isEmpty() && !lines.isEmpty()) {
				QJsonObject mod;
				mod[QStringLiteral("name")]   = cheat_name;
				mod[QStringLiteral("type")]   = QStringLiteral("checkbox");
				mod[QStringLiteral("memory")] = lines;
				mods.append(mod);
			}
		}
	}
	if (xml.hasError()) {
		return Tr("Malformed trainer XML: %1").arg(xml.errorString());
	}
	if (mods.isEmpty()) {
		return Tr("No cheats usable with the current patch engine were found.");
	}
	QJsonObject root;
	root[QStringLiteral("id")]      = attribute("Cusa");
	root[QStringLiteral("version")] = attribute("Version");
	root[QStringLiteral("process")] = process;
	root[QStringLiteral("mods")]    = mods;
	*document = QJsonDocument(root);
	return {};
}

} // namespace

bool IsSupportedTitleId(const QString& title_id) {
	static const QRegularExpression pattern(QStringLiteral("\\APPSA[0-9]{5}\\z"));
	return pattern.match(title_id.trimmed().toUpper()).hasMatch();
}

QString PlanPath(const QString& title_id) {
	if (!IsSupportedTitleId(title_id)) {
		return {};
	}
	return QDir(QCoreApplication::applicationDirPath())
	    .filePath(QStringLiteral("_Patches/%1.json").arg(title_id.trimmed().toUpper()));
}

QString ParseDocument(const QByteArray& data, QJsonDocument* document) {
	if (data.trimmed().startsWith('<')) {
		const auto error = ParseTrainerXml(data, document);
		if (!error.isEmpty()) {
			*document = QJsonDocument();
		}
		return error;
	}
	const auto parsed = QJsonDocument::fromJson(data);
	const auto error  = ValidateDocument(parsed);
	*document         = error.isEmpty() ? parsed : QJsonDocument();
	return error;
}

QString ValidateImport(const QJsonDocument& document, const QString& title_id,
                       const QString& version, const QString& process) {
	const auto parse_error = ValidateDocument(document);
	if (!parse_error.isEmpty()) {
		return parse_error;
	}
	const auto root = document.object();
	if (!IsSupportedTitleId(title_id) ||
	    root.value(QStringLiteral("id"))
	            .toString()
	            .compare(title_id.trimmed(), Qt::CaseInsensitive) != 0) {
		return Tr("The cheat file does not match this game's title ID.");
	}
	if (version.isEmpty() || root.value(QStringLiteral("version")).toString() != version) {
		return Tr("The cheat file does not match this game's version (%1).").arg(version);
	}
	if (process.isEmpty() ||
	    root.value(QStringLiteral("process")).toString().compare(process, Qt::CaseInsensitive) !=
	        0) {
		return Tr("The cheat file does not match this game's executable (%1).").arg(process);
	}
	static const QRegularExpression offset_pattern(
	    QStringLiteral("\\A(?:0[xX])?[0-9a-fA-F]{1,16}\\z"));
	static const QRegularExpression bytes_pattern(QStringLiteral("\\A(?:[0-9a-fA-F]{2})+\\z"));
	const auto                      mods = root.value(QStringLiteral("mods")).toArray();
	if (mods.isEmpty()) {
		return Tr("The cheat file contains no cheats.");
	}
	if (root.contains(QStringLiteral("master"))) {
		return Tr("This JSON uses master codes, which are not supported yet.");
	}
	for (const auto& value: mods) {
		const auto mod    = value.toObject();
		const auto memory = mod.value(QStringLiteral("memory"));
		if (mod.contains(QStringLiteral("module_name"))) {
			return Tr("This JSON uses module-specific cheats, which are not supported yet.");
		}
		if (!memory.isArray() || memory.toArray().isEmpty()) {
			return Tr("Unsupported memory entries in cheat \"%1\".")
			    .arg(mod.value(QStringLiteral("name")).toString());
		}
		for (const auto& entry: memory.toArray()) {
			const auto write    = entry.toObject();
			const auto offset   = write.value(QStringLiteral("offset")).toString();
			const auto off      = write.value(QStringLiteral("off")).toString();
			const auto on       = write.value(QStringLiteral("on")).toString();
			const auto absolute = write.value(QStringLiteral("absolute"));
			const auto expected = write.value(QStringLiteral("expected"));
			if ((!absolute.isUndefined() && (!absolute.isBool() || absolute.toBool())) ||
			    (!expected.isUndefined() &&
			     (!expected.isString() ||
			      expected.toString().compare(off, Qt::CaseInsensitive) != 0)) ||
			    write.contains(QStringLiteral("section")) ||
			    write.contains(QStringLiteral("sectionName")) ||
			    write.contains(QStringLiteral("sectionProtection")) ||
			    write.contains(QStringLiteral("logicalOffset")) ||
			    write.contains(QStringLiteral("sectionOffset"))) {
				return Tr("This JSON uses memory addressing features that are not supported yet.");
			}
			if (!entry.isObject() || !offset_pattern.match(offset).hasMatch() ||
			    !bytes_pattern.match(off).hasMatch() || !bytes_pattern.match(on).hasMatch() ||
			    off.size() != on.size()) {
				return Tr("Unsupported memory entries in cheat \"%1\".")
				    .arg(mod.value(QStringLiteral("name")).toString());
			}
		}
	}
	return {};
}

QJsonDocument WithSelection(const QJsonDocument& document, const QList<bool>& enabled) {
	auto root = document.object();
	auto mods = root.value(QStringLiteral("mods")).toArray();
	if (mods.size() != enabled.size()) {
		return {};
	}
	for (qsizetype index = 0; index < mods.size(); ++index) {
		auto mod = mods[index].toObject();
		mod.insert(QStringLiteral("enabled"), enabled[index]);
		mods[index] = mod;
	}
	root.insert(QStringLiteral("mods"), mods);
	return QJsonDocument(root);
}

QString SaveDocument(const QString& path, const QJsonDocument& document,
                     const QByteArray& expected_data) {
	if (path.isEmpty() || !document.isObject()) {
		return Tr("Invalid cheat file.");
	}
	QFile input(path);
	if (input.exists()) {
		if (!input.open(QIODevice::ReadOnly)) {
			return Tr("Could not read cheat file: %1").arg(input.errorString());
		}
		const auto current = input.readAll();
		if (input.error() != QFile::NoError) {
			return Tr("Could not read cheat file: %1").arg(input.errorString());
		}
		if (current != expected_data) {
			return Tr("Cheat file changed; reload the dialog.");
		}
	} else if (!expected_data.isEmpty()) {
		return Tr("Cheat file changed; reload the dialog.");
	}
	input.close();
	if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
		return Tr("Could not create the local cheats directory.");
	}
	QSaveFile  output(path);
	const auto data = document.toJson();
	if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() ||
	    !output.commit()) {
		return Tr("Could not save cheat file: %1").arg(output.errorString());
	}
	return {};
}

} // namespace Cheats
