#pragma once

#include <QQuickItem>

class GpuMapItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(double heading READ heading WRITE setHeading NOTIFY headingChanged)

public:
    explicit GpuMapItem(QQuickItem* parent = nullptr);
    [[nodiscard]] double heading() const noexcept { return heading_; }
    void setHeading(double value);

signals:
    void headingChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode,
                             UpdatePaintNodeData*) override;

private:
    double heading_{};
};
