#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

// Qt-facing staging model for Operational Catalog V2.  It deliberately writes
// a draft format which the flight runtime cannot load as an active profile.
class OperationalProfileModel {
public:
    bool load(const QString& catalogIndexPath, const QString& draftPath);

    [[nodiscard]] QVariantList profiles() const { return profiles_; }
    [[nodiscard]] QVariantList domains() const { return domains_; }
    [[nodiscard]] QVariantList fields() const;
    [[nodiscard]] QVariantMap summary() const;
    [[nodiscard]] QVariantMap selectedField() const;
    [[nodiscard]] QString selectedProfileKey() const { return selected_profile_key_; }
    [[nodiscard]] QString selectedDomain() const { return selected_domain_; }
    [[nodiscard]] QString filter() const { return filter_; }
    [[nodiscard]] QString status() const { return status_; }

    bool selectProfile(const QString& profileKey);
    bool selectDomain(const QString& domain);
    bool selectField(const QString& parameterKey);
    void setFilter(const QString& filter);
    bool stageSelectedField(const QString& disposition, const QString& valueText,
                            const QString& rationale, const QString& sourceDocument,
                            const QString& sourceRevision, const QString& sourcePage);
    bool discardDraft();

private:
    [[nodiscard]] QVariantMap mergedField(const QVariantMap& definition,
                                          const QVariantMap& raw) const;
    [[nodiscard]] QVariantMap inheritedParameter(const QString& parameterKey) const;
    [[nodiscard]] QVariant parseValue(const QVariantMap& definition,
                                      const QString& text, bool& ok) const;
    [[nodiscard]] bool saveDraft();
    void loadDraft();
    void refreshSelection();

    QString catalog_index_path_;
    QString draft_path_;
    QString selected_profile_key_;
    QString selected_domain_;
    QString selected_field_key_;
    QString filter_;
    QString status_{QStringLiteral("NOT LOADED")};
    QVariantList profiles_;
    QVariantList domains_;
    QVariantList definitions_;
    QVariantList selected_parameters_;
    QVariantMap staged_values_;
};
