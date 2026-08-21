#include "gpu_map_item.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <array>
#include <cmath>
#include <vector>

namespace {
QSGGeometryNode* line_node(const std::vector<QPointF>& points, QColor color,
                           float width, const QSizeF& size, bool loop = false) {
    const int count = static_cast<int>(points.size()) + (loop ? 1 : 0);
    auto* geometry = new QSGGeometry{QSGGeometry::defaultAttributes_Point2D(), count};
    geometry->setDrawingMode(QSGGeometry::DrawLineStrip);
    geometry->setLineWidth(width);
    auto* vertices = geometry->vertexDataAsPoint2D();
    for (int index = 0; index < count; ++index) {
        const auto& point = points[static_cast<std::size_t>(index % points.size())];
        vertices[index].set(static_cast<float>(point.x() * size.width()),
                            static_cast<float>(point.y() * size.height()));
    }
    auto* material = new QSGFlatColorMaterial;
    material->setColor(color);
    auto* node = new QSGGeometryNode;
    node->setGeometry(geometry); node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material); node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

QSGGeometryNode* triangle_strip(const std::vector<QPointF>& points, QColor color,
                                const QSizeF& size) {
    auto* geometry = new QSGGeometry{QSGGeometry::defaultAttributes_Point2D(),
                                     static_cast<int>(points.size())};
    geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    auto* vertices = geometry->vertexDataAsPoint2D();
    for (std::size_t index = 0; index < points.size(); ++index) {
        vertices[index].set(static_cast<float>(points[index].x() * size.width()),
                            static_cast<float>(points[index].y() * size.height()));
    }
    auto* material = new QSGFlatColorMaterial; material->setColor(color);
    auto* node = new QSGGeometryNode;
    node->setGeometry(geometry); node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material); node->setFlag(QSGNode::OwnsMaterial);
    return node;
}
}

GpuMapItem::GpuMapItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void GpuMapItem::setHeading(double value) {
    if (qFuzzyCompare(heading_, value)) return;
    heading_ = value; emit headingChanged(); update();
}

QSGNode* GpuMapItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    delete oldNode;
    auto* root = new QSGNode;
    const QSizeF size{width(), height()};
    if (size.isEmpty()) return root;

    root->appendChildNode(triangle_strip({{0,0},{1,0},{0,1},{1,1}},
                                        QColor{"#13232D"}, size));
    // Several subdued terrain bands create depth without competing with flight data.
    root->appendChildNode(triangle_strip({{0,0.78},{0.14,0.60},{0.29,0.70},{0.42,0.48},
                                          {0.57,0.65},{0.72,0.43},{0.88,0.59},{1,0.50},{1,1},{0,1}},
                                         QColor{"#22372F"}, size));
    root->appendChildNode(triangle_strip({{0,0.88},{0.18,0.73},{0.33,0.79},{0.49,0.61},
                                          {0.64,0.70},{0.80,0.56},{1,0.69},{1,1},{0,1}},
                                         QColor{"#304232"}, size));
    root->appendChildNode(triangle_strip({{0.57,0.66},{0.72,0.49},{0.86,0.58},{0.95,0.48},
                                          {1,0.59},{1,0.80},{0.86,0.73},{0.70,0.82}},
                                         QColor{"#46503A"}, size));

    // Fine and major reference grid.
    for (int index = 1; index < 16; ++index) {
        const double coordinate = static_cast<double>(index) / 16.0;
        root->appendChildNode(line_node({{coordinate,0},{coordinate,1}},
                                        index % 2 == 0 ? QColor{"#29424E"} : QColor{"#1C313B"},
                                        index % 2 == 0 ? 1.0F : 0.6F, size));
    }
    for (int index = 1; index < 12; ++index) {
        const double coordinate = static_cast<double>(index) / 12.0;
        root->appendChildNode(line_node({{0,coordinate},{1,coordinate}},
                                        index % 2 == 0 ? QColor{"#29424E"} : QColor{"#1C313B"},
                                        index % 2 == 0 ? 1.0F : 0.6F, size));
    }
    // Synthetic roads/terrain contours and a disabled airspace boundary sample.
    root->appendChildNode(line_node({{0.00,0.66},{0.16,0.62},{0.31,0.68},{0.47,0.59},
                                     {0.61,0.64},{0.77,0.55},{1.00,0.60}},
                                    QColor{"#647052"}, 1.2F, size));
    root->appendChildNode(line_node({{0.04,0.82},{0.20,0.76},{0.36,0.82},{0.52,0.72},
                                     {0.69,0.77},{0.88,0.69}},
                                    QColor{"#526348"}, 1.0F, size));
    root->appendChildNode(line_node({{0.02,0.28},{0.20,0.24},{0.31,0.31},{0.48,0.26},
                                     {0.68,0.30},{0.83,0.23},{1.0,0.27}},
                                    QColor{"#273F4C"}, 1.0F, size));
    for (double radius : {0.18, 0.34, 0.50}) {
        std::vector<QPointF> ring;
        for (int index = 0; index < 64; ++index) {
            const double angle = static_cast<double>(index) * 2.0 * 3.141592653589793 / 64.0;
            ring.emplace_back(0.50 + std::cos(angle) * radius,
                              0.56 + std::sin(angle) * radius);
        }
        root->appendChildNode(line_node(ring, QColor{"#4B8797"}, 1.4F, size, true));
    }
    // Track/bearing reference from ownship to the active leg.
    root->appendChildNode(line_node({{0.50,0.56},{0.50,0.08}},
                                    QColor{"#607B86"}, 1.0F, size));
    const std::vector<QPointF> route{{0.10,0.90},{0.27,0.72},{0.43,0.62},
                                     {0.50,0.56},{0.63,0.38},{0.82,0.17}};
    root->appendChildNode(line_node(route, QColor{"#052E38"}, 7.0F, size));
    root->appendChildNode(line_node(route, QColor{"#20D6F4"}, 3.0F, size));
    // Ownship halo and high-contrast aircraft glyph.
    std::vector<QPointF> halo;
    for (int index = 0; index < 40; ++index) {
        const double angle = static_cast<double>(index) * 2.0 * 3.141592653589793 / 40.0;
        halo.emplace_back(0.50 + std::cos(angle) * 0.035,
                          0.56 + std::sin(angle) * 0.035);
    }
    root->appendChildNode(line_node(halo, QColor{"#839CA6"}, 1.0F, size, true));
    root->appendChildNode(line_node({{0.50,0.50},{0.47,0.62},{0.50,0.59},
                                     {0.53,0.62},{0.50,0.50}},
                                    QColor{"#F2F0E9"}, 3.0F, size));
    return root;
}
