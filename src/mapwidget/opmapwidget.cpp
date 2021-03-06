/**
******************************************************************************
*
* @file       opmapwidget.cpp
* @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
* @brief      The Map Widget, this is the part exposed to the user
* @see        The GNU Public License (GPL) Version 3
* @defgroup   OPMapWidget
* @{
*
*****************************************************************************/
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "opmapwidget.h"
#include <QtGui>
#include <QMetaObject>
#include "waypointitem.h"

namespace mapcontrol
{

    OPMapWidget::OPMapWidget(QWidget *parent, Configuration *config):QGraphicsView(parent),configuration(config),UAV(0),GPS(0),Home(0),followmouse(true),compass(0),showuav(false),showhome(false),showDiag(false),diagGraphItem(0),diagTimer(0)
    {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        core=new internals::Core;
        map=new MapGraphicItem(core,config);
        mscene.addItem(map);
        this->setScene(&mscene);
        this->adjustSize();
        connect(map,SIGNAL(zoomChanged(double,double,double)),this,SIGNAL(zoomChanged(double,double,double)));
        connect(map->core,SIGNAL(OnCurrentPositionChanged(internals::PointLatLng)),this,SIGNAL(OnCurrentPositionChanged(internals::PointLatLng)));
        connect(map->core,SIGNAL(OnEmptyTileError(int,core::Point)),this,SIGNAL(OnEmptyTileError(int,core::Point)));
        connect(map->core,SIGNAL(OnMapDrag()),this,SIGNAL(OnMapDrag()));
        connect(map->core,SIGNAL(OnMapTypeChanged(MapType::Types)),this,SIGNAL(OnMapTypeChanged(MapType::Types)));
        connect(map->core,SIGNAL(OnMapZoomChanged()),this,SIGNAL(OnMapZoomChanged()));
        connect(map->core,SIGNAL(OnTileLoadComplete()),this,SIGNAL(OnTileLoadComplete()));
        connect(map->core,SIGNAL(OnTileLoadStart()),this,SIGNAL(OnTileLoadStart()));
        connect(map->core,SIGNAL(OnTilesStillToLoad(int)),this,SIGNAL(OnTilesStillToLoad(int)));
        SetShowDiagnostics(showDiag);
        this->setMouseTracking(followmouse);
        SetShowCompass(true);

    }
    void OPMapWidget::SetShowDiagnostics(bool const& value)
    {
        showDiag=value;
        if(!showDiag)
        {
            if(diagGraphItem!=0)
            {
                delete diagGraphItem;
                diagGraphItem=0;
            }
            if(diagTimer!=0)
            {
                delete diagTimer;
                diagTimer=0;
            }
        }
        else
        {
            diagTimer=new QTimer();
            connect(diagTimer,SIGNAL(timeout()),this,SLOT(diagRefresh()));
            diagTimer->start(500);
        }

    }
    void OPMapWidget::SetUavPic(QString UAVPic)
    {
        if(UAV!=0)
            UAV->SetUavPic(UAVPic);
        if(GPS!=0)
            GPS->SetUavPic(UAVPic);


    }
    void OPMapWidget::SetShowUAV(const bool &value)
    {
        if(value && UAV==0)
        {
            UAV=new UAVItem(map,this);
            UAV->setParentItem(map);
            connect(this,SIGNAL(UAVLeftSafetyBouble(internals::PointLatLng)),UAV,SIGNAL(UAVLeftSafetyBouble(internals::PointLatLng)));
            connect(this,SIGNAL(UAVReachedWayPoint(int,WayPointItem*)),UAV,SIGNAL(UAVReachedWayPoint(int,WayPointItem*)));
        }
        else if(!value)
        {
            if(UAV!=0)
            {
                delete UAV;
                UAV=0;
            }

        }
        if(value && GPS==0)
        {
            GPS=new GPSItem(map,this);
            GPS->setParentItem(map);
        }
        else if(!value)
        {
            if(GPS!=0)
            {
                delete GPS;
                GPS=0;
            }

        }
    }
    void OPMapWidget::SetShowHome(const bool &value)
    {
        if(value && Home==0)
        {
            Home=new HomeItem(map,this);
            Home->setParentItem(map);
        }
        else if(!value)
        {
            if(Home!=0)
            {
                delete Home;
                Home=0;
            }

        }
    }

    void OPMapWidget::resizeEvent(QResizeEvent *event)
    {
        if (scene())
            scene()->setSceneRect(
                    QRect(QPoint(0, 0), event->size()));
        QGraphicsView::resizeEvent(event);
        if(compass)
            compass->setScale(0.1+0.05*(qreal)(event->size().width())/1000*(qreal)(event->size().height())/600);

    }
    QSize OPMapWidget::sizeHint() const
    {
        return map->sizeHint();
    }
    void OPMapWidget::showEvent(QShowEvent *event)
    {
        connect(&mscene,SIGNAL(sceneRectChanged(QRectF)),map,SLOT(resize(QRectF)));
        map->start();
        QGraphicsView::showEvent(event);
    }
    OPMapWidget::~OPMapWidget()
    {
        delete UAV;
        delete Home;
        delete map;
        delete core;
        delete configuration;
        foreach(QGraphicsItem* i,this->items())
        {
            delete i;
        }
    }
    void OPMapWidget::closeEvent(QCloseEvent *event)
    {
        core->OnMapClose();
        event->accept();
    }
    void OPMapWidget::SetUseOpenGL(const bool &value)
    {
        useOpenGL=value;
        if (useOpenGL)
            setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers)));
        else
            setupViewport(new QWidget());
        update();
    }
    internals::PointLatLng OPMapWidget::currentMousePosition()
    {
        return currentmouseposition;
    }

    void OPMapWidget::mouseMoveEvent(QMouseEvent *event)
    {
        QGraphicsView::mouseMoveEvent(event);
        QPointF p=event->posF();
        p=map->mapFromParent(p);
        currentmouseposition=map->FromLocalToLatLng(p.x(),p.y());
    }
    ////////////////WAYPOINT////////////////////////
    WayPointItem* OPMapWidget::WPCreate()
    {
        WayPointItem* item=new WayPointItem(this->CurrentPosition(),0,map);
        ConnectWP(item);
        item->setParentItem(map);
        return item;
    }
    void OPMapWidget::WPCreate(WayPointItem* item)
    {
        ConnectWP(item);
        item->setParentItem(map);
    }
    WayPointItem* OPMapWidget::WPCreate(internals::PointLatLng const& coord,int const& altitude)
    {
        WayPointItem* item=new WayPointItem(coord,altitude,map);
        ConnectWP(item);
        item->setParentItem(map);
        return item;
    }
    WayPointItem* OPMapWidget::WPCreate(internals::PointLatLng const& coord,int const& altitude, QString const& description)
    {
        WayPointItem* item=new WayPointItem(coord,altitude,description,map);
        ConnectWP(item);
        item->setParentItem(map);
        return item;
    }
    WayPointItem* OPMapWidget::WPInsert(const int &position)
    {
        WayPointItem* item=new WayPointItem(this->CurrentPosition(),0,map);
        item->SetNumber(position);
        ConnectWP(item);
        item->setParentItem(map);
        emit WPInserted(position,item);
        return item;
    }
    void OPMapWidget::WPInsert(WayPointItem* item,const int &position)
    {
        item->SetNumber(position);
        ConnectWP(item);
        item->setParentItem(map);
        emit WPInserted(position,item);

    }
    WayPointItem* OPMapWidget::WPInsert(internals::PointLatLng const& coord,int const& altitude,const int &position)
    {
        WayPointItem* item=new WayPointItem(coord,altitude,map);
        item->SetNumber(position);
        ConnectWP(item);
        item->setParentItem(map);
        emit WPInserted(position,item);
        return item;
    }
    WayPointItem* OPMapWidget::WPInsert(internals::PointLatLng const& coord,int const& altitude, QString const& description,const int &position)
    {
        WayPointItem* item=new WayPointItem(coord,altitude,description,map);
        item->SetNumber(position);
        ConnectWP(item);
        item->setParentItem(map);
        emit WPInserted(position,item);
        return item;
    }
    void OPMapWidget::WPDelete(WayPointItem *item)
    {
        emit WPDeleted(item->Number());
        delete item;
    }
    void OPMapWidget::WPDeleteAll()
    {
        foreach(QGraphicsItem* i,map->childItems())
        {
            WayPointItem* w=qgraphicsitem_cast<WayPointItem*>(i);
            if(w)
                delete w;
        }
    }
    QList<WayPointItem*> OPMapWidget::WPSelected()
    {
        QList<WayPointItem*> list;
        foreach(QGraphicsItem* i,mscene.selectedItems())
        {
            WayPointItem* w=qgraphicsitem_cast<WayPointItem*>(i);
            if(w)
                list.append(w);
        }
        return list;
    }
    void OPMapWidget::WPRenumber(WayPointItem *item, const int &newnumber)
    {
        item->SetNumber(newnumber);
    }

    void OPMapWidget::ConnectWP(WayPointItem *item)
    {
        connect(item,SIGNAL(WPNumberChanged(int,int,WayPointItem*)),this,SIGNAL(WPNumberChanged(int,int,WayPointItem*)));
        connect(item,SIGNAL(WPValuesChanged(WayPointItem*)),this,SIGNAL(WPValuesChanged(WayPointItem*)));
        connect(this,SIGNAL(WPInserted(int,WayPointItem*)),item,SLOT(WPInserted(int,WayPointItem*)));
        connect(this,SIGNAL(WPNumberChanged(int,int,WayPointItem*)),item,SLOT(WPRenumbered(int,int,WayPointItem*)));
        connect(this,SIGNAL(WPDeleted(int)),item,SLOT(WPDeleted(int)));
    }
    void OPMapWidget::diagRefresh()
    {
        if(showDiag)
        {
            if(diagGraphItem==0)
            {
                diagGraphItem=new QGraphicsTextItem();
                mscene.addItem(diagGraphItem);
                diagGraphItem->setPos(10,100);
                diagGraphItem->setZValue(3);
                diagGraphItem->setFlag(QGraphicsItem::ItemIsMovable,true);
                diagGraphItem->setDefaultTextColor(Qt::yellow);
            }
            diagGraphItem->setPlainText(core->GetDiagnostics().toString());
        }
        else
            if(diagGraphItem!=0)
            {
            delete diagGraphItem;
            diagGraphItem=0;
        }
    }

    //////////////////////////////////////////////
    void OPMapWidget::SetShowCompass(const bool &value)
    {
        if(value && !compass)
        {
            compass=new QGraphicsSvgItem(QString::fromUtf8(":/markers/images/compas.svg"));
            compass->setScale(0.1+0.05*(qreal)(this->size().width())/1000*(qreal)(this->size().height())/600);
            //    compass->setTransformOriginPoint(compass->boundingRect().width(),compass->boundingRect().height());
            compass->setFlag(QGraphicsItem::ItemIsMovable,true);
            mscene.addItem(compass);
            compass->setTransformOriginPoint(compass->boundingRect().width()/2,compass->boundingRect().height()/2);            
            compass->setPos(55-compass->boundingRect().width()/2,55-compass->boundingRect().height()/2);
            compass->setZValue(3);
            compass->setOpacity(0.7);
            
        }
        if(!value && compass)
        {
            delete compass;
            compass=0;
        }
    }
    void OPMapWidget::SetRotate(qreal const& value)
    {
        map->mapRotate(value);
        if(compass && (compass->rotation() != value)) {
            compass->setRotation(value);
        }
    }
    void OPMapWidget::RipMap()
    {
        new MapRipper(core,map->SelectedArea());
    }
}
