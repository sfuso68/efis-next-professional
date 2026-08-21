#include "circular_gauge_item.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
using Points = std::vector<QPointF>;
QSGGeometryNode* line(const Points& points, QColor color, float width,
                      QSGGeometry::DrawingMode mode = QSGGeometry::DrawLineStrip) {
    auto* geometry = new QSGGeometry{QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(points.size())};
    geometry->setDrawingMode(mode); geometry->setLineWidth(width);
    auto* data = geometry->vertexDataAsPoint2D();
    for (std::size_t i=0;i<points.size();++i) data[i].set(points[i].x(),points[i].y());
    auto* material = new QSGFlatColorMaterial; material->setColor(color);
    auto* node = new QSGGeometryNode; node->setGeometry(geometry); node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material); node->setFlag(QSGNode::OwnsMaterial); return node;
}
constexpr double pi = 3.141592653589793;
double gaugeAngle(double fraction) { return (-220.0 + std::clamp(fraction,0.0,1.0)*260.0)*pi/180.0; }
Points arc(QPointF c, double radius, double from, double to, int count=30) {
    Points result; result.reserve(count+1);
    for(int i=0;i<=count;++i){const double a=gaugeAngle(from+(to-from)*i/count);result.emplace_back(c.x()+std::cos(a)*radius,c.y()+std::sin(a)*radius);} return result;
}
}

CircularGaugeItem::CircularGaugeItem(QQuickItem* parent):QQuickItem(parent){setFlag(ItemHasContents,true);}
void CircularGaugeItem::assign(double& target,double value){if(qFuzzyCompare(target,value))return;target=value;emit gaugeChanged();update();}
void CircularGaugeItem::setValue(double v){assign(value_,v);} void CircularGaugeItem::setMinimum(double v){assign(minimum_,v);}
void CircularGaugeItem::setMaximum(double v){assign(maximum_,v);} void CircularGaugeItem::setGreenStart(double v){assign(green_start_,v);}
void CircularGaugeItem::setGreenEnd(double v){assign(green_end_,v);}

QSGNode* CircularGaugeItem::updatePaintNode(QSGNode* oldNode,UpdatePaintNodeData*){
    delete oldNode; auto* root=new QSGNode; const double w=width(),h=height(); if(w<=0||h<=0)return root;
    const QPointF c{w*.5,h*.57}; const double radius=std::min(w,h)*.41;
    const double span=std::max(0.001,maximum_-minimum_);
    const double gs=std::clamp((green_start_-minimum_)/span,0.0,1.0);
    const double ge=std::clamp((green_end_-minimum_)/span,gs,1.0);
    const float outerWidth=static_cast<float>(std::clamp(radius*.14,3.0,8.0));
    const float baseWidth=static_cast<float>(std::clamp(radius*.09,2.0,5.0));
    const float rangeWidth=static_cast<float>(std::clamp(radius*.075,1.8,4.2));
    root->appendChildNode(line(arc(c,radius+1,0,1,96),QColor{"#101C24"},outerWidth));
    root->appendChildNode(line(arc(c,radius,0,1,96),QColor{"#3C5563"},baseWidth));
    if(gs>0)root->appendChildNode(line(arc(c,radius,0,gs),QColor{"#DF4138"},rangeWidth));
    root->appendChildNode(line(arc(c,radius,gs,ge),QColor{"#62D347"},rangeWidth));
    if(ge<.9)root->appendChildNode(line(arc(c,radius,ge,.9),QColor{"#E6A72E"},rangeWidth));
    root->appendChildNode(line(arc(c,radius,std::max(.9,ge),1),QColor{"#DF4138"},rangeWidth));
    Points ticks;
    for(int i=0;i<=30;++i){const double a=gaugeAngle(i/30.0);const double inner=radius-(i%5==0?10:5);ticks.emplace_back(c.x()+std::cos(a)*inner,c.y()+std::sin(a)*inner);ticks.emplace_back(c.x()+std::cos(a)*(radius+1),c.y()+std::sin(a)*(radius+1));}
    root->appendChildNode(line(ticks,QColor{"#E8F1F3"},radius<35?0.75F:1.0F,QSGGeometry::DrawLines));
    const double a=gaugeAngle((value_-minimum_)/span);
    const float needleWidth=static_cast<float>(std::clamp(radius*.045,1.2,2.8));
    root->appendChildNode(line({{c.x()+1,c.y()+1},{c.x()+std::cos(a)*(radius-10)+1,c.y()+std::sin(a)*(radius-10)+1}},QColor{"#55000000"},needleWidth+1.5F,QSGGeometry::DrawLines));
    root->appendChildNode(line({c,{c.x()+std::cos(a)*(radius-10),c.y()+std::sin(a)*(radius-10)}},QColor{"#F7FBFC"},needleWidth,QSGGeometry::DrawLines));
    root->appendChildNode(line(arc(c,std::clamp(radius*.075,2.4,4.5),0,1,24),QColor{"#19C3E6"},std::clamp(static_cast<float>(radius*.06),2.0F,3.5F)));
    return root;
}
