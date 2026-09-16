#include "operational_profile_model.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main(int argc, char** argv) {
    QCoreApplication application{argc, argv};
    try {
        QTemporaryDir temporary;
        require(temporary.isValid(), "temporary directory must be available");
        const QString root = QStringLiteral(EFIS_TEST_DATA_DIR);
        const QString index = root + QStringLiteral("/data/catalog/efis-catalog-index.json");
        const QString draft = temporary.filePath(QStringLiteral("operational-profile-draft.json"));

        OperationalProfileModel model;
        require(model.load(index, draft), "V2 catalog index must load");
        require(model.profiles().size() == 3, "three controlled profiles must be exposed");
        require(model.domains().size() == 28, "all operational domains must be exposed");
        require(model.summary().value("total").toInt() == 447, "all fields must be exposed");
        require(!model.summary().value("release_ready").toBool(), "setup drafts must never be release-ready");
        require(model.status().contains(QStringLiteral("NOT ACTIVE")), "draft safety state must be visible");
        require(model.selectedProfileKey() == QStringLiteral("aircraft.asso-v-jolly-owner"),
                "the real installed-aircraft draft must remain the default selection");
        require(!model.summary().value("simulation_profile").toBool(),
                "the default installed-aircraft draft must not be labelled as the synthetic scenario");
        require(model.domainSummaries().size() == 28, "guided workflow must summarize every domain");
        int summarizedFields = 0;
        for (const auto& domain : model.domainSummaries())
            summarizedFields += domain.toMap().value("total").toInt();
        require(summarizedFields == 447, "domain workflow must account for every field exactly once");
        require(model.validationReport().value("state").toString() == QStringLiteral("BLOCKED_INCOMPLETE"),
                "the installed-aircraft draft must begin visibly blocked by missing data");
        require(!model.validationIssues().isEmpty(), "the correction queue must expose unresolved fields");

        require(model.selectProfile(QStringLiteral("simulation.asso-v-jolly-912-ul")),
                "complete simulation profile must be selectable");
        require(model.summary().value("simulation_profile").toBool(),
                "simulation profile must be unambiguously identified");
        require(model.summary().value("simulated").toInt() == 447,
                "every simulation field must retain its simulated marker");
        require(model.summary().value("safety_blockers").toInt() == 0,
                "complete synthetic profile must exercise every safety field");
        require(!model.summary().value("release_ready").toBool(),
                "complete synthetic data must still be prohibited from release");

        require(model.selectProfile(QStringLiteral("model.asso-v-jolly-912-ul")), "model profile must be selectable");
        require(model.selectedProfileKey() == QStringLiteral("model.asso-v-jolly-912-ul"),
                "selecting a profile must not be overridden by another profile's draft");
        require(model.selectDomain(QStringLiteral("identity")), "model identity domain must be selectable");
        require(model.selectField(QStringLiteral("identity.registration")), "model registration must be selectable");
        require(!model.stageSelectedField(QStringLiteral("INHERIT"), {}, {}, {}, {}, {}),
                "a root model profile cannot inherit");

        require(model.selectProfile(QStringLiteral("aircraft.asso-v-jolly-owner")), "installed aircraft must be selectable");
        require(model.selectDomain(QStringLiteral("identity")), "identity domain must be selectable");
        require(model.selectField(QStringLiteral("identity.registration")), "registration must be selectable");
        require(!model.stageSelectedField(QStringLiteral("NOT_APPLICABLE"), {}, {}, {}, {}, {}),
                "not-applicable without rationale must be rejected");
        require(model.stageSelectedField(QStringLiteral("VALUE"), QStringLiteral("I-TEST"),
                QStringLiteral("Bench placeholder"), QStringLiteral("Aircraft certificate"),
                QStringLiteral("REV A"), QStringLiteral("1")), "typed registration draft must save");
        require(QFile::exists(draft), "draft must be written atomically");
        require(model.summary().value("edited").toInt() == 1, "edit count must update");
        require(!model.summary().value("release_ready").toBool(), "an edited draft cannot activate itself");

        require(model.selectField(QStringLiteral("identity.seats")), "seat count must be selectable");
        require(!model.stageSelectedField(QStringLiteral("VALUE"), QStringLiteral("two"), {}, {}, {}, {}),
                "integer type mismatch must be rejected");
        require(model.stageSelectedField(QStringLiteral("VALUE"), QStringLiteral("2"), {},
                QStringLiteral("AFM"), QStringLiteral("1"), QStringLiteral("2")),
                "valid integer must be accepted");
        require(model.stageSelectedField(QStringLiteral("INHERIT"), {}, {}, {}, {}, {}),
                "installed-aircraft field must be restorable to inherited state");
        require(model.selectedField().value("effective_disposition").toString() == QStringLiteral("VALUE"),
                "inherit must resolve the parent value rather than appear unresolved");
        require(model.selectedField().value("effective_value").toLongLong() == 2,
                "inherit must expose the effective parent value");

        QFile draftFile{draft};
        require(draftFile.open(QIODevice::ReadOnly), "draft must be readable");
        const auto draftDocument = QJsonDocument::fromJson(draftFile.readAll()).object();
        require(draftDocument.value("schema").toString() == QStringLiteral("efis-next.operational-profile-draft.v2"),
                "draft schema must not be confused with active profile schema");
        require(!draftDocument.value("activation_allowed").toBool(true), "draft must explicitly prohibit activation");
        require(draftDocument.value("edited_values").toObject().size() == 2, "both edits must persist");
        draftFile.close();

        model.setFilter(QStringLiteral("registration"));
        require(model.fields().size() == 1, "field filtering must be deterministic");
        require(model.discardDraft(), "draft discard must succeed");
        require(!QFile::exists(draft), "discard must remove only the draft artifact");
        require(model.summary().value("edited").toInt() == 0, "discard must clear edit state");

        const QString prefillDraft = temporary.filePath(QStringLiteral("simulation-prefill-draft.json"));
        OperationalProfileModel workflow;
        require(workflow.load(index, prefillDraft), "workflow model must load");
        require(workflow.preloadSimulationProfile(QStringLiteral("simulation.asso-v-jolly-912-ul")),
                "explicit simulation prefill must create a draft");
        require(workflow.summary().value("edited").toInt() == 447,
                "simulation prefill must explicitly stage every dictionary field");
        require(workflow.summary().value("simulated").toInt() == 447,
                "simulation prefill must retain the SIM marker on every field");
        require(workflow.validationReport().value("state").toString() == QStringLiteral("BLOCKED_SIMULATION"),
                "complete simulated values must remain blocked from independent review");
        require(!workflow.validateDraft(), "a complete simulation prefill must never pass review readiness");

        const QString importedDraft = temporary.filePath(QStringLiteral("imported-draft.json"));
        OperationalProfileModel importer;
        require(importer.load(index, importedDraft), "draft importer model must load");
        require(importer.importDraft(prefillDraft), "a matching safe V2 draft must import");
        require(importer.summary().value("edited").toInt() == 447,
                "import must preserve the complete staged field set");
        require(importer.summary().value("simulated").toInt() == 447,
                "import must preserve field-level simulation markers");

        QFile sourceDraft{prefillDraft};
        require(sourceDraft.open(QIODevice::ReadOnly), "prefill draft must be readable for safety fixture");
        auto unsafeRoot = QJsonDocument::fromJson(sourceDraft.readAll()).object();
        sourceDraft.close();
        unsafeRoot.insert(QStringLiteral("activation_allowed"), true);
        QFile unsafeFile{temporary.filePath(QStringLiteral("unsafe-draft.json"))};
        require(unsafeFile.open(QIODevice::WriteOnly), "unsafe fixture must be writable");
        unsafeFile.write(QJsonDocument{unsafeRoot}.toJson(QJsonDocument::Indented));
        unsafeFile.close();
        require(!importer.importDraft(unsafeFile.fileName()),
                "an imported draft requesting activation must be rejected");
        require(importer.summary().value("edited").toInt() == 447,
                "rejected import must preserve the prior safe draft");

        require(importer.selectProfile(QStringLiteral("model.asso-v-jolly-912-ul")),
                "root profile must be selectable for import safety fixture");
        unsafeRoot.insert(QStringLiteral("base_profile_key"), QStringLiteral("model.asso-v-jolly-912-ul"));
        auto unsafeEdits = QJsonObject{};
        unsafeEdits.insert(QStringLiteral("identity.registration"),
            QJsonObject{{QStringLiteral("disposition"), QStringLiteral("INHERIT")},
                        {QStringLiteral("approval_status"), QStringLiteral("INDICATIVE")}});
        unsafeRoot.insert(QStringLiteral("edited_values"), unsafeEdits);
        unsafeRoot.insert(QStringLiteral("activation_allowed"), false);
        QFile rootInheritFile{temporary.filePath(QStringLiteral("root-inherit-draft.json"))};
        require(rootInheritFile.open(QIODevice::WriteOnly), "root inherit fixture must be writable");
        rootInheritFile.write(QJsonDocument{unsafeRoot}.toJson(QJsonDocument::Indented));
        rootInheritFile.close();
        require(!importer.importDraft(rootInheritFile.fileName()),
                "a root-profile draft must not import an unresolved inheritance request");

        std::cout << "operational profile model tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
