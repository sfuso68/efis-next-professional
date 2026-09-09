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
        require(model.profiles().size() == 2, "two controlled profiles must be exposed");
        require(model.domains().size() == 28, "all operational domains must be exposed");
        require(model.summary().value("total").toInt() == 447, "all fields must be exposed");
        require(!model.summary().value("release_ready").toBool(), "setup drafts must never be release-ready");
        require(model.status().contains(QStringLiteral("NOT ACTIVE")), "draft safety state must be visible");

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

        std::cout << "operational profile model tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
