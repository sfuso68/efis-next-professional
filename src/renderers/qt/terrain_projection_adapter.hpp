#pragma once

#include <QObject>

class TerrainProjectionAdapter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double tileWidth READ tileWidth CONSTANT)
    Q_PROPERTY(double tileDepth READ tileDepth CONSTANT)
    Q_PROPERTY(double highDetailEnter READ highDetailEnter CONSTANT)
    Q_PROPERTY(double highDetailExit READ highDetailExit CONSTANT)
    Q_PROPERTY(double preloadMargin READ preloadMargin CONSTANT)

  public:
    explicit TerrainProjectionAdapter(QObject* parent = nullptr) : QObject(parent) {}

    [[nodiscard]] double tileWidth() const noexcept;
    [[nodiscard]] double tileDepth() const noexcept;
    [[nodiscard]] double highDetailEnter() const noexcept;
    [[nodiscard]] double highDetailExit() const noexcept;
    [[nodiscard]] double preloadMargin() const noexcept;

    Q_INVOKABLE [[nodiscard]] double worldX(double latitude, double longitude) const noexcept;
    Q_INVOKABLE [[nodiscard]] double worldZ(double latitude, double longitude) const noexcept;
    Q_INVOKABLE [[nodiscard]] double footprintDistance(double cameraX, double cameraZ, double tileX,
                                                       double tileZ) const noexcept;
    Q_INVOKABLE [[nodiscard]] bool highDetailForDistance(double distance,
                                                         bool currentlyHighDetail) const noexcept;
    Q_INVOKABLE [[nodiscard]] bool tileEnabledForDistance(double distance) const noexcept;
};
