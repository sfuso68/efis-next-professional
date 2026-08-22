#pragma once

#include <QQuickItem>

class CircularGaugeItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY gaugeChanged)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum NOTIFY gaugeChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY gaugeChanged)
    Q_PROPERTY(double greenStart READ greenStart WRITE setGreenStart NOTIFY gaugeChanged)
    Q_PROPERTY(double greenEnd READ greenEnd WRITE setGreenEnd NOTIFY gaugeChanged)
    Q_PROPERTY(double majorStep READ majorStep WRITE setMajorStep NOTIFY gaugeChanged)
    Q_PROPERTY(double minorStep READ minorStep WRITE setMinorStep NOTIFY gaugeChanged)

public:
    explicit CircularGaugeItem(QQuickItem* parent = nullptr);
    double value() const { return value_; } double minimum() const { return minimum_; }
    double maximum() const { return maximum_; } double greenStart() const { return green_start_; }
    double greenEnd() const { return green_end_; }
    double majorStep() const { return major_step_; } double minorStep() const { return minor_step_; }
    void setValue(double v); void setMinimum(double v); void setMaximum(double v);
    void setGreenStart(double v); void setGreenEnd(double v);
    void setMajorStep(double v); void setMinorStep(double v);
signals:
    void gaugeChanged();
protected:
    QSGNode* updatePaintNode(QSGNode*, UpdatePaintNodeData*) override;
private:
    void assign(double& target, double value);
    double value_{}, minimum_{}, maximum_{100.0}, green_start_{20.0}, green_end_{80.0};
    double major_step_{10.0}, minor_step_{2.0};
};
