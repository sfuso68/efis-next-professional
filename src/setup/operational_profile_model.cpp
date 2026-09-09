#include "operational_profile_model.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace {
QString displayValue(const QVariant& value) {
    if (!value.isValid() || value.isNull()) return QStringLiteral("—");
    if (value.metaType().id() == QMetaType::Bool)
        return value.toBool() ? QStringLiteral("YES") : QStringLiteral("NO");
    if (value.canConvert<QVariantList>())
        return QString::fromUtf8(QJsonDocument{QJsonArray::fromVariantList(value.toList())}.toJson(QJsonDocument::Compact));
    if (value.canConvert<QVariantMap>())
        return QString::fromUtf8(QJsonDocument{QJsonObject::fromVariantMap(value.toMap())}.toJson(QJsonDocument::Compact));
    return value.toString();
}

bool importedValueMatches(const QVariant& value, const QVariantMap& definition) {
    const auto json = QJsonValue::fromVariant(value);
    const auto kind = definition.value("value_type").toString();
    if (kind == QStringLiteral("NUMBER") || kind == QStringLiteral("DURATION")) return json.isDouble();
    if (kind == QStringLiteral("INTEGER"))
        return json.isDouble() && std::floor(json.toDouble()) == json.toDouble();
    if (kind == QStringLiteral("BOOLEAN")) return json.isBool();
    if (kind == QStringLiteral("TEXT") || kind == QStringLiteral("DATE"))
        return json.isString() && !json.toString().trimmed().isEmpty();
    if (kind == QStringLiteral("ENUM"))
        return json.isString() && definition.value("enum_values").toList().contains(json.toString());
    if (kind == QStringLiteral("TABLE") || kind == QStringLiteral("CURVE")) return json.isArray();
    return !json.isUndefined();
}
}

bool OperationalProfileModel::load(const QString& catalogIndexPath, const QString& draftPath) {
    catalog_index_path_ = catalogIndexPath;
    draft_path_ = draftPath;
    QFile file{catalog_index_path_};
    if (!file.open(QIODevice::ReadOnly)) {
        status_ = QStringLiteral("CATALOG UNAVAILABLE");
        return false;
    }
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        status_ = QStringLiteral("CATALOG INVALID");
        return false;
    }
    const auto root = document.object();
    if (root.value("schema_version").toInt() < 2) {
        status_ = QStringLiteral("CATALOG V2 REQUIRED");
        return false;
    }
    profiles_ = root.value("operational_profiles").toArray().toVariantList();
    domains_ = root.value("operational_domains").toArray().toVariantList();
    definitions_ = root.value("operational_parameters").toArray().toVariantList();
    if (profiles_.isEmpty() || domains_.isEmpty() || definitions_.isEmpty()) {
        status_ = QStringLiteral("OPERATIONAL DATA MISSING");
        return false;
    }
    selected_profile_key_ = profiles_.front().toMap().value("profile_key").toString();
    for (const auto& profile : profiles_) {
        const auto value = profile.toMap();
        if (value.value("scope_type").toString() == QStringLiteral("INSTALLED_AIRCRAFT")) {
            selected_profile_key_ = value.value("profile_key").toString();
            break;
        }
    }
    selected_domain_ = domains_.front().toMap().value("id").toString();
    staged_values_.clear();
    loadDraft();
    refreshSelection();
    status_ = QStringLiteral("DRAFT ONLY • NOT ACTIVE");
    return true;
}

void OperationalProfileModel::loadDraft() {
    QFile file{draft_path_};
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) return;
    const auto root = document.object();
    if (root.value("schema").toString() != QStringLiteral("efis-next.operational-profile-draft.v2")) return;
    const auto profileKey = root.value("base_profile_key").toString();
    // A draft belongs to one base profile only. Selecting another catalog
    // profile must never silently switch the selection back to the draft.
    if (profileKey != selected_profile_key_) return;
    staged_values_ = root.value("edited_values").toObject().toVariantMap();
}

void OperationalProfileModel::refreshSelection() {
    selected_parameters_.clear();
    for (const auto& profile : profiles_) {
        const auto item = profile.toMap();
        if (item.value("profile_key").toString() == selected_profile_key_) {
            selected_parameters_ = item.value("parameters").toList();
            break;
        }
    }
    if (selected_parameters_.isEmpty()) {
        selected_field_key_.clear();
        return;
    }
    const auto visible = fields();
    selected_field_key_ = visible.isEmpty()
        ? selected_parameters_.front().toMap().value("parameter_key").toString()
        : visible.front().toMap().value("key").toString();
}

bool OperationalProfileModel::selectProfile(const QString& profileKey) {
    for (const auto& profile : profiles_) {
        if (profile.toMap().value("profile_key").toString() != profileKey) continue;
        selected_profile_key_ = profileKey;
        staged_values_.clear();
        loadDraft();
        refreshSelection();
        status_ = QStringLiteral("DRAFT ONLY • NOT ACTIVE");
        return true;
    }
    status_ = QStringLiteral("PROFILE NOT FOUND");
    return false;
}

bool OperationalProfileModel::selectDomain(const QString& domain) {
    for (const auto& item : domains_) {
        if (item.toMap().value("id").toString() != domain) continue;
        selected_domain_ = domain;
        refreshSelection();
        return true;
    }
    return false;
}

bool OperationalProfileModel::selectField(const QString& parameterKey) {
    for (const auto& field : fields()) {
        if (field.toMap().value("key").toString() != parameterKey) continue;
        selected_field_key_ = parameterKey;
        return true;
    }
    return false;
}

void OperationalProfileModel::setFilter(const QString& filter) {
    filter_ = filter.trimmed();
    refreshSelection();
}

QVariantMap OperationalProfileModel::inheritedParameter(const QString& parameterKey) const {
    QString parentKey;
    for (const auto& profileValue : profiles_) {
        const auto profile = profileValue.toMap();
        if (profile.value("profile_key").toString() == selected_profile_key_) {
            parentKey = profile.value("parent_profile").toString();
            break;
        }
    }
    if (parentKey.isEmpty()) return {};
    for (const auto& profileValue : profiles_) {
        const auto profile = profileValue.toMap();
        if (profile.value("profile_key").toString() != parentKey) continue;
        for (const auto& parameterValue : profile.value("parameters").toList()) {
            const auto parameter = parameterValue.toMap();
            if (parameter.value("parameter_key").toString() == parameterKey) return parameter;
        }
    }
    return {};
}

QVariantMap OperationalProfileModel::mergedField(const QVariantMap& definition,
                                                  const QVariantMap& raw) const {
    QVariantMap result = definition;
    result.insert("key", definition.value("key"));
    QVariantMap value = raw;
    const auto key = definition.value("key").toString();
    const auto staged = staged_values_.value(key).toMap();
    if (!staged.isEmpty()) {
        // A staged value owns its own value and provenance. Never let source
        // metadata from the underlying catalog make a new draft value appear
        // documented when the operator did not enter that evidence.
        static const QStringList replacedKeys{QStringLiteral("value"), QStringLiteral("unit"),
            QStringLiteral("simulated"), QStringLiteral("effective_simulated"),
            QStringLiteral("source_title"), QStringLiteral("source_document"),
            QStringLiteral("source_revision"), QStringLiteral("source_page"),
            QStringLiteral("rationale"), QStringLiteral("reviewed_by"),
            QStringLiteral("reviewed_at")};
        for (const auto& replacedKey : replacedKeys) value.remove(replacedKey);
        for (auto it = staged.cbegin(); it != staged.cend(); ++it) value.insert(it.key(), it.value());
        value.insert("edited", true);
        value.insert("effective_disposition", value.value("disposition"));
        if (value.value("disposition").toString() == QStringLiteral("VALUE")) {
            value.insert("effective_value", value.value("value"));
            value.insert("effective_simulated", value.value("simulated", false));
        } else if (value.value("disposition").toString() == QStringLiteral("INHERIT")) {
            const auto inherited = inheritedParameter(key);
            value.insert("effective_disposition", inherited.value("effective_disposition", QStringLiteral("UNKNOWN")));
            value.insert("effective_approval_status", inherited.value("effective_approval_status", QStringLiteral("INDICATIVE")));
            value.insert("effective_simulated", inherited.value("effective_simulated", false));
            value.insert("origin_profile", inherited.value("origin_profile"));
            if (inherited.contains("effective_value"))
                value.insert("effective_value", inherited.value("effective_value"));
            else
                value.remove("effective_value");
        } else {
            value.remove("effective_value");
        }
    }
    for (auto it = value.cbegin(); it != value.cend(); ++it) result.insert(it.key(), it.value());
    const auto effective = result.value("effective_disposition").toString();
    const bool unresolved = effective == QStringLiteral("UNKNOWN") || effective == QStringLiteral("INHERIT");
    result.insert("unresolved", unresolved);
    result.insert("blocker", unresolved && result.value("criticality").toString() == QStringLiteral("SAFETY_CRITICAL"));
    QString dispositionText = effective;
    dispositionText.replace('_', ' ');
    result.insert("display_value", effective == QStringLiteral("VALUE")
        ? displayValue(result.value("effective_value")) : dispositionText);
    return result;
}

QVariantList OperationalProfileModel::fields() const {
    QVariantMap rawByKey;
    for (const auto& raw : selected_parameters_)
        rawByKey.insert(raw.toMap().value("parameter_key").toString(), raw);
    QVariantList result;
    for (const auto& entry : definitions_) {
        const auto definition = entry.toMap();
        if (!selected_domain_.isEmpty() && definition.value("domain").toString() != selected_domain_) continue;
        const auto merged = mergedField(definition, rawByKey.value(definition.value("key").toString()).toMap());
        if (!filter_.isEmpty()) {
            const auto haystack = (merged.value("key").toString() + ' ' + merged.value("label").toString()).toLower();
            if (!haystack.contains(filter_.toLower())) continue;
        }
        result.append(merged);
    }
    return result;
}

QVariantMap OperationalProfileModel::selectedField() const {
    for (const auto& field : fields()) {
        const auto item = field.toMap();
        if (item.value("key").toString() == selected_field_key_) return item;
    }
    return {};
}

QVariantMap OperationalProfileModel::selectedProfile() const {
    for (const auto& profileValue : profiles_) {
        const auto profile = profileValue.toMap();
        if (profile.value("profile_key").toString() == selected_profile_key_) return profile;
    }
    return {};
}

QVariantList OperationalProfileModel::domainSummaries() const {
    QVariantMap rawByKey;
    for (const auto& raw : selected_parameters_)
        rawByKey.insert(raw.toMap().value("parameter_key").toString(), raw);
    QVariantList summaries;
    for (const auto& domainValue : domains_) {
        auto domain = domainValue.toMap();
        int total = 0;
        int resolved = 0;
        int unresolved = 0;
        int blockers = 0;
        int simulated = 0;
        int edited = 0;
        for (const auto& definitionValue : definitions_) {
            const auto definition = definitionValue.toMap();
            if (definition.value("domain") != domain.value("id")) continue;
            const auto field = mergedField(definition, rawByKey.value(definition.value("key").toString()).toMap());
            ++total;
            resolved += field.value("unresolved").toBool() ? 0 : 1;
            unresolved += field.value("unresolved").toBool() ? 1 : 0;
            blockers += field.value("blocker").toBool() ? 1 : 0;
            simulated += field.value("effective_simulated").toBool() ? 1 : 0;
            edited += field.value("edited").toBool() ? 1 : 0;
        }
        domain.insert("total", total);
        domain.insert("resolved", resolved);
        domain.insert("unresolved", unresolved);
        domain.insert("blockers", blockers);
        domain.insert("simulated", simulated);
        domain.insert("edited", edited);
        domain.insert("completion_percent", total == 0 ? 0 : (resolved * 100) / total);
        summaries.append(domain);
    }
    return summaries;
}

QVariantList OperationalProfileModel::validationIssues() const {
    QVariantMap rawByKey;
    for (const auto& raw : selected_parameters_)
        rawByKey.insert(raw.toMap().value("parameter_key").toString(), raw);
    QVariantList issues;
    for (const auto& definitionValue : definitions_) {
        const auto definition = definitionValue.toMap();
        const auto key = definition.value("key").toString();
        const auto field = mergedField(definition, rawByKey.value(key).toMap());
        QString code;
        QString message;
        int priority = 0;
        if (field.value("unresolved").toBool() && field.value("criticality").toString() == QStringLiteral("SAFETY_CRITICAL")) {
            code = QStringLiteral("SAFETY_BLOCKER");
            message = QStringLiteral("Safety-critical value is unresolved");
            priority = 4;
        } else if (field.value("unresolved").toBool() && field.value("requirement").toString() == QStringLiteral("REQUIRED")) {
            code = QStringLiteral("REQUIRED_UNKNOWN");
            message = QStringLiteral("Required value is unresolved");
            priority = 3;
        } else if (field.value("effective_simulated").toBool()) {
            code = QStringLiteral("SIMULATED_VALUE");
            message = QStringLiteral("Replace simulated value with applicable aircraft evidence");
            priority = 2;
        } else if (field.value("effective_disposition").toString() == QStringLiteral("VALUE") &&
                   field.value("criticality").toString() == QStringLiteral("SAFETY_CRITICAL") &&
                   (field.value("source_document").toString().trimmed().isEmpty() ||
                    field.value("source_revision").toString().trimmed().isEmpty() ||
                    field.value("source_page").toString().trimmed().isEmpty())) {
            code = QStringLiteral("MISSING_EVIDENCE");
            message = QStringLiteral("Document, revision and page are required");
            priority = 1;
        }
        if (code.isEmpty()) continue;
        issues.append(QVariantMap{{"key", key}, {"domain", definition.value("domain")},
            {"label", definition.value("label")}, {"code", code},
            {"message", message}, {"priority", priority}});
    }
    std::sort(issues.begin(), issues.end(), [](const QVariant& left, const QVariant& right) {
        const auto a = left.toMap();
        const auto b = right.toMap();
        if (a.value("priority").toInt() != b.value("priority").toInt())
            return a.value("priority").toInt() > b.value("priority").toInt();
        return a.value("key").toString() < b.value("key").toString();
    });
    return issues;
}

QVariantMap OperationalProfileModel::validationReport() const {
    const auto overall = summary();
    const auto issues = validationIssues();
    int missingEvidence = 0;
    for (const auto& issue : issues)
        missingEvidence += issue.toMap().value("code").toString() == QStringLiteral("MISSING_EVIDENCE") ? 1 : 0;
    QString state = QStringLiteral("READY_FOR_INDEPENDENT_REVIEW");
    if (overall.value("unresolved").toInt() > 0) state = QStringLiteral("BLOCKED_INCOMPLETE");
    else if (overall.value("simulated").toInt() > 0) state = QStringLiteral("BLOCKED_SIMULATION");
    else if (missingEvidence > 0) state = QStringLiteral("BLOCKED_EVIDENCE");
    return {{"state", state}, {"issue_count", issues.size()},
        {"unresolved", overall.value("unresolved")},
        {"safety_blockers", overall.value("safety_blockers")},
        {"simulated", overall.value("simulated")}, {"missing_evidence", missingEvidence},
        {"ready_for_review", state == QStringLiteral("READY_FOR_INDEPENDENT_REVIEW")},
        {"activation_allowed", false}, {"checked_profile", selected_profile_key_}};
}

QVariantMap OperationalProfileModel::summary() const {
    int unresolved = 0;
    int safetyBlockers = 0;
    int values = 0;
    int simulated = 0;
    QVariantMap selectedProfile;
    for (const auto& profileValue : profiles_) {
        const auto profile = profileValue.toMap();
        if (profile.value("profile_key").toString() == selected_profile_key_) {
            selectedProfile = profile;
            break;
        }
    }
    for (const auto& definitionValue : definitions_) {
        const auto definition = definitionValue.toMap();
        QVariantMap raw;
        for (const auto& candidate : selected_parameters_) {
            if (candidate.toMap().value("parameter_key") == definition.value("key")) {
                raw = candidate.toMap();
                break;
            }
        }
        const auto field = mergedField(definition, raw);
        unresolved += field.value("unresolved").toBool() ? 1 : 0;
        safetyBlockers += field.value("blocker").toBool() ? 1 : 0;
        values += field.value("effective_disposition").toString() == QStringLiteral("VALUE") ? 1 : 0;
        simulated += field.value("effective_simulated").toBool() ? 1 : 0;
    }
    return {{"total", definitions_.size()}, {"values", values}, {"unresolved", unresolved},
            {"safety_blockers", safetyBlockers}, {"edited", staged_values_.size()},
            {"simulated", simulated}, {"release_ready", false},
            {"profile_key", selected_profile_key_},
            {"profile_title", selectedProfile.value("title")},
            {"lifecycle_state", selectedProfile.value("lifecycle_state")},
            {"simulation_profile", selected_profile_key_.startsWith(QStringLiteral("simulation."))}};
}

QVariant OperationalProfileModel::parseValue(const QVariantMap& definition,
                                              const QString& text, bool& ok) const {
    ok = false;
    const auto kind = definition.value("value_type").toString();
    const auto trimmed = text.trimmed();
    if (kind == QStringLiteral("NUMBER") || kind == QStringLiteral("DURATION")) {
        const auto value = trimmed.toDouble(&ok);
        return value;
    }
    if (kind == QStringLiteral("INTEGER")) {
        const auto value = trimmed.toLongLong(&ok);
        return value;
    }
    if (kind == QStringLiteral("BOOLEAN")) {
        const auto normalized = trimmed.toLower();
        ok = normalized == QStringLiteral("true") || normalized == QStringLiteral("false") ||
             normalized == QStringLiteral("1") || normalized == QStringLiteral("0") ||
             normalized == QStringLiteral("yes") || normalized == QStringLiteral("no");
        return normalized == QStringLiteral("true") || normalized == QStringLiteral("1") || normalized == QStringLiteral("yes");
    }
    if (kind == QStringLiteral("ENUM")) {
        const auto allowed = definition.value("enum_values").toList();
        ok = allowed.contains(trimmed);
        return trimmed;
    }
    if (kind == QStringLiteral("TABLE") || kind == QStringLiteral("CURVE") || kind == QStringLiteral("JSON")) {
        QJsonParseError error{};
        const auto document = QJsonDocument::fromJson(trimmed.toUtf8(), &error);
        ok = error.error == QJsonParseError::NoError && !document.isNull();
        if (!ok) return {};
        if (document.isArray()) return document.array().toVariantList();
        if (document.isObject()) return document.object().toVariantMap();
        ok = false;
        return {};
    }
    ok = !trimmed.isEmpty();
    return trimmed;
}

bool OperationalProfileModel::stageSelectedField(const QString& disposition,
    const QString& valueText, const QString& rationale, const QString& sourceDocument,
    const QString& sourceRevision, const QString& sourcePage) {
    static const QSet<QString> allowed{QStringLiteral("VALUE"), QStringLiteral("UNKNOWN"),
        QStringLiteral("NOT_APPLICABLE"), QStringLiteral("NOT_INSTALLED"), QStringLiteral("INHERIT")};
    const auto normalized = disposition.trimmed().toUpper();
    if (!allowed.contains(normalized) || selected_field_key_.isEmpty()) {
        status_ = QStringLiteral("INVALID DISPOSITION");
        return false;
    }
    if (normalized == QStringLiteral("INHERIT")) {
        QString parentProfile;
        for (const auto& item : profiles_) {
            const auto profile = item.toMap();
            if (profile.value("profile_key").toString() == selected_profile_key_) {
                parentProfile = profile.value("parent_profile_key").toString();
                if (parentProfile.isEmpty())
                    parentProfile = profile.value("parent_profile").toString();
                break;
            }
        }
        if (parentProfile.isEmpty()) {
            status_ = QStringLiteral("ROOT PROFILE CANNOT INHERIT");
            return false;
        }
    }
    QVariantMap definition;
    for (const auto& item : definitions_)
        if (item.toMap().value("key").toString() == selected_field_key_) definition = item.toMap();
    if (definition.isEmpty()) return false;
    QVariantMap staged{{"disposition", normalized}, {"approval_status", QStringLiteral("INDICATIVE")},
                       {"rationale", rationale.trimmed()}};
    if (normalized == QStringLiteral("VALUE")) {
        bool ok = false;
        const auto parsed = parseValue(definition, valueText, ok);
        if (!ok) {
            status_ = QStringLiteral("VALUE TYPE ERROR • ") + definition.value("value_type").toString();
            return false;
        }
        staged.insert("value", parsed);
        staged.insert("simulated", selected_profile_key_.startsWith(QStringLiteral("simulation.")));
        if (!definition.value("canonical_unit").toString().isEmpty())
            staged.insert("unit", definition.value("canonical_unit"));
        if (!sourceDocument.trimmed().isEmpty()) staged.insert("source_document", sourceDocument.trimmed());
        if (!sourceRevision.trimmed().isEmpty()) staged.insert("source_revision", sourceRevision.trimmed());
        if (!sourcePage.trimmed().isEmpty()) staged.insert("source_page", sourcePage.trimmed());
    } else if ((normalized == QStringLiteral("NOT_APPLICABLE") || normalized == QStringLiteral("NOT_INSTALLED")) && rationale.trimmed().isEmpty()) {
        status_ = QStringLiteral("RATIONALE REQUIRED");
        return false;
    }
    staged_values_.insert(selected_field_key_, staged);
    if (!saveDraft()) return false;
    status_ = QStringLiteral("DRAFT SAVED • NOT ACTIVE");
    return true;
}

bool OperationalProfileModel::selectNextIssue() {
    const auto issues = validationIssues();
    if (issues.isEmpty()) {
        status_ = QStringLiteral("NO STRUCTURAL ISSUE • INDEPENDENT REVIEW STILL REQUIRED");
        return false;
    }
    const auto issue = issues.front().toMap();
    filter_.clear();
    selected_domain_ = issue.value("domain").toString();
    refreshSelection();
    selected_field_key_ = issue.value("key").toString();
    status_ = issue.value("code").toString() + QStringLiteral(" • DRAFT ONLY");
    return true;
}

bool OperationalProfileModel::preloadSimulationProfile(const QString& profileKey) {
    if (!profileKey.startsWith(QStringLiteral("simulation.")) ||
        selected_profile_key_.startsWith(QStringLiteral("simulation."))) {
        status_ = QStringLiteral("SIMULATION PRELOAD REJECTED");
        return false;
    }
    QVariantList sourceParameters;
    for (const auto& profileValue : profiles_) {
        const auto profile = profileValue.toMap();
        if (profile.value("profile_key").toString() == profileKey) {
            sourceParameters = profile.value("parameters").toList();
            break;
        }
    }
    if (sourceParameters.size() != definitions_.size()) {
        status_ = QStringLiteral("SIMULATION TEMPLATE INCOMPLETE");
        return false;
    }
    const auto previous = staged_values_;
    QVariantMap imported;
    for (const auto& parameterValue : sourceParameters) {
        const auto parameter = parameterValue.toMap();
        if (parameter.value("effective_disposition").toString() != QStringLiteral("VALUE") ||
            !parameter.value("effective_simulated").toBool() || !parameter.contains("effective_value")) {
            staged_values_ = previous;
            status_ = QStringLiteral("SIMULATION TEMPLATE INVALID");
            return false;
        }
        QVariantMap entry{{"disposition", QStringLiteral("VALUE")},
            {"value", parameter.value("effective_value")}, {"simulated", true},
            {"approval_status", QStringLiteral("INDICATIVE")},
            {"source_document", QStringLiteral("EFIS NEXT Simulation Dataset V1")},
            {"source_revision", QStringLiteral("1")}, {"source_page", QStringLiteral("GENERATED")},
            {"rationale", QStringLiteral("SIMULATION PRELOAD — replace with evidence applicable to the installed aircraft.")}};
        if (!parameter.value("canonical_unit").toString().isEmpty())
            entry.insert("unit", parameter.value("canonical_unit"));
        imported.insert(parameter.value("parameter_key").toString(), entry);
    }
    staged_values_ = imported;
    if (!saveDraft()) {
        staged_values_ = previous;
        return false;
    }
    status_ = QStringLiteral("SIMULATION TEMPLATE LOADED • 447 VALUES • NOT FOR FLIGHT");
    return true;
}

bool OperationalProfileModel::importDraft(const QString& path) {
    QFile file{path.trimmed()};
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024) {
        status_ = QStringLiteral("DRAFT IMPORT READ ERROR");
        return false;
    }
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        status_ = QStringLiteral("DRAFT IMPORT INVALID JSON");
        return false;
    }
    const auto root = document.object();
    const auto profile = selectedProfile();
    if (root.value("schema").toString() != QStringLiteral("efis-next.operational-profile-draft.v2") ||
        root.value("base_profile_key").toString() != selected_profile_key_ ||
        root.value("base_revision").toInt(-1) != profile.value("revision").toInt() ||
        !root.contains("activation_allowed") || root.value("activation_allowed").toBool(true) ||
        !root.value("edited_values").isObject()) {
        status_ = QStringLiteral("DRAFT IMPORT SAFETY CHECK FAILED");
        return false;
    }
    QVariantMap definitionsByKey;
    for (const auto& definitionValue : definitions_) {
        const auto definition = definitionValue.toMap();
        definitionsByKey.insert(definition.value("key").toString(), definition);
    }
    const QSet<QString> dispositions{QStringLiteral("VALUE"), QStringLiteral("UNKNOWN"),
        QStringLiteral("NOT_APPLICABLE"), QStringLiteral("NOT_INSTALLED"), QStringLiteral("INHERIT")};
    const QSet<QString> allowedKeys{QStringLiteral("disposition"), QStringLiteral("value"),
        QStringLiteral("unit"), QStringLiteral("simulated"), QStringLiteral("approval_status"),
        QStringLiteral("rationale"), QStringLiteral("source_document"),
        QStringLiteral("source_revision"), QStringLiteral("source_page")};
    QVariantMap imported;
    const auto edited = root.value("edited_values").toObject();
    for (auto it = edited.begin(); it != edited.end(); ++it) {
        if (!definitionsByKey.contains(it.key()) || !it.value().isObject()) {
            status_ = QStringLiteral("DRAFT IMPORT UNKNOWN FIELD • ") + it.key();
            return false;
        }
        const auto object = it.value().toObject();
        for (auto entryIt = object.begin(); entryIt != object.end(); ++entryIt) {
            if (!allowedKeys.contains(entryIt.key())) {
                status_ = QStringLiteral("DRAFT IMPORT UNSUPPORTED METADATA • ") + it.key();
                return false;
            }
        }
        auto entry = object.toVariantMap();
        const auto disposition = entry.value("disposition").toString();
        const auto definition = definitionsByKey.value(it.key()).toMap();
        if (!dispositions.contains(disposition) ||
            (!entry.value("approval_status").toString().isEmpty() &&
             entry.value("approval_status").toString() != QStringLiteral("INDICATIVE")) ||
            (entry.contains("simulated") && entry.value("simulated").metaType().id() != QMetaType::Bool) ||
            (!entry.value("unit").toString().isEmpty() &&
             entry.value("unit").toString() != definition.value("canonical_unit").toString())) {
            status_ = QStringLiteral("DRAFT IMPORT INVALID FIELD • ") + it.key();
            return false;
        }
        if (disposition == QStringLiteral("INHERIT") &&
            profile.value("parent_profile").toString().trimmed().isEmpty()) {
            status_ = QStringLiteral("DRAFT IMPORT ROOT INHERIT FORBIDDEN • ") + it.key();
            return false;
        }
        if (disposition == QStringLiteral("VALUE")) {
            if (!entry.contains("value") || !importedValueMatches(entry.value("value"), definition)) {
                status_ = QStringLiteral("DRAFT IMPORT TYPE ERROR • ") + it.key();
                return false;
            }
        } else if (entry.contains("value")) {
            status_ = QStringLiteral("DRAFT IMPORT UNEXPECTED VALUE • ") + it.key();
            return false;
        }
        if ((disposition == QStringLiteral("NOT_APPLICABLE") || disposition == QStringLiteral("NOT_INSTALLED")) &&
            entry.value("rationale").toString().trimmed().isEmpty()) {
            status_ = QStringLiteral("DRAFT IMPORT RATIONALE REQUIRED • ") + it.key();
            return false;
        }
        entry.insert("approval_status", QStringLiteral("INDICATIVE"));
        imported.insert(it.key(), entry);
    }
    const auto previous = staged_values_;
    staged_values_ = imported;
    if (!saveDraft()) {
        staged_values_ = previous;
        return false;
    }
    refreshSelection();
    status_ = QStringLiteral("DRAFT IMPORTED • STRUCTURAL CHECK PASS • NOT ACTIVE");
    return true;
}

bool OperationalProfileModel::validateDraft() {
    const auto report = validationReport();
    status_ = report.value("state").toString() + QStringLiteral(" • CHECK ONLY • NOT ACTIVE");
    return report.value("ready_for_review").toBool();
}

bool OperationalProfileModel::saveDraft() {
    QDir{}.mkpath(QFileInfo{draft_path_}.absolutePath());
    QSaveFile file{draft_path_};
    if (!file.open(QIODevice::WriteOnly)) {
        status_ = QStringLiteral("DRAFT WRITE ERROR");
        return false;
    }
    int revision = 0;
    for (const auto& profile : profiles_)
        if (profile.toMap().value("profile_key").toString() == selected_profile_key_)
            revision = profile.toMap().value("revision").toInt();
    const QJsonObject root{{"schema", "efis-next.operational-profile-draft.v2"},
        {"base_profile_key", selected_profile_key_}, {"base_revision", revision},
        {"updated_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"activation_allowed", false}, {"edited_values", QJsonObject::fromVariantMap(staged_values_)}};
    file.write(QJsonDocument{root}.toJson(QJsonDocument::Indented));
    return file.commit();
}

bool OperationalProfileModel::discardDraft() {
    staged_values_.clear();
    if (QFileInfo::exists(draft_path_) && !QFile::remove(draft_path_)) {
        status_ = QStringLiteral("DRAFT REMOVE ERROR");
        return false;
    }
    status_ = QStringLiteral("DRAFT CLEARED • NOT ACTIVE");
    return true;
}
