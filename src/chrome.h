#pragma once
#include <QObject>
#include <QQuickView>
#include <QRegion>
#include <QTimer>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QVariantMap>
#include <QPointer>
#include <QSet>
#ifdef SOVEL_LAYER_SHELL
#include <LayerShellQt/Window>
#endif

class Chrome : public QObject {
 Q_OBJECT
 Q_PROPERTY(bool shown READ shown NOTIFY shownChanged)
 Q_PROPERTY(bool popupOpen READ popupOpen NOTIFY shownChanged)
public:
 Chrome(QQuickView *view,QString surface,bool desktop):QObject(view),m_view(view),m_surface(surface),m_desktop(desktop){
  view->installEventFilter(this);m_reveal.setSingleShot(true);m_hide.setSingleShot(true);
  connect(&m_reveal,&QTimer::timeout,this,[this]{if(m_inside){m_revealed=true;update();}});
  connect(&m_hide,&QTimer::timeout,this,[this]{if(!m_popupCount){m_revealed=false;update();}});
  connect(view,&QWindow::widthChanged,this,[this]{apply();});connect(view,&QWindow::heightChanged,this,[this]{apply();});
 }
 bool shown()const{return m_shown;}
 bool popupOpen()const{return m_popupCount>0;}
 void shareTopEdgeWith(Chrome *bar){m_topBar=bar;}
 Q_INVOKABLE void configure(const QVariantMap &value){m_enabled=value.value("enabled",true).toBool();m_reserve=value.value("reserve",m_enabled&&!value.value("autoHide",true).toBool()).toBool();m_autoHide=value.value("autoHide",true).toBool();m_sensitivity=qBound(2,value.value("sensitivity",8).toInt(),24);m_revealDelay=qBound(0,value.value("revealDelay",80).toInt(),1000);m_hideDelay=qBound(100,value.value("hideDelay",450).toInt(),2500);m_edge=m_surface=="bar"?"top":value.value("activationEdge","bottom").toString();update();}
 Q_INVOKABLE void setContentRect(int x,int y,int w,int h){m_content=QRect(x,y,w,h);apply();}
 Q_INVOKABLE void setAutoHidden(bool value){if(value&&!m_autoHidden&&m_inside&&content().contains(m_position))m_revealed=true;m_autoHidden=value;update();}
 Q_INVOKABLE void setShown(bool value){setAutoHidden(!value);}
 Q_INVOKABLE void setPopupOpen(bool value){m_popupCount=qMax(0,m_popupCount+(value?1:-1));if(m_popupCount){m_hide.stop();m_reveal.stop();}else if(m_autoHidden&&(!m_inside||(!content().contains(m_position)&&!edge().contains(m_position))))m_hide.start(m_hideDelay);update();}
 Q_INVOKABLE void registerPopup(QObject *popup,bool open){
  if(!popup)return;
  if(open&&!m_popups.contains(popup)){m_popups.insert(popup);connect(popup,&QObject::destroyed,this,[this,popup]{if(m_popups.remove(popup))setPopupOpen(false);});setPopupOpen(true);}
  else if(!open&&m_popups.remove(popup))setPopupOpen(false);
 }
 Q_INVOKABLE void releasePopups(){
  const auto popups=m_popups;for(auto popup:popups)if(popup->metaObject()->indexOfMethod("close()")>=0)QMetaObject::invokeMethod(popup,"close");m_popups.clear();m_popupCount=0;update();
 }
signals:
 void shownChanged();
protected:
 bool eventFilter(QObject *object,QEvent *event)override{
  if(object!=m_view||!m_desktop||(m_surface!="bar"&&m_surface!="dock"))return false;
  if(event->type()==QEvent::Enter){m_inside=true;point(static_cast<QEnterEvent*>(event)->position().toPoint());}
  else if(event->type()==QEvent::MouseMove){m_inside=true;point(static_cast<QMouseEvent*>(event)->position().toPoint());}
  else if(event->type()==QEvent::Leave){m_inside=false;m_reveal.stop();if(!m_popupCount)m_hide.start(m_hideDelay);if(m_surface=="dock"&&m_edge=="top"&&m_topBar){m_topBar->m_inside=false;m_topBar->m_reveal.stop();if(!m_topBar->m_popupCount)m_topBar->m_hide.start(m_topBar->m_hideDelay);}}
  else if(event->type()==QEvent::Show)apply();
  return false;
 }
private:
 QRect edge()const{
  if(m_edge=="top")return {0,0,m_view->width(),m_sensitivity};
  if(m_edge=="left")return {0,0,m_sensitivity,m_view->height()};
  if(m_edge=="right")return {m_view->width()-m_sensitivity,0,m_sensitivity,m_view->height()};
  return {0,m_view->height()-m_sensitivity,m_view->width(),m_sensitivity};
 }
 QRect content()const{return m_surface=="bar"?QRect(0,0,m_view->width(),30):m_content;}
 void point(const QPoint &position){
  m_position=position;
  if(edge().contains(position)||content().contains(position)){m_hide.stop();if(!m_shown&&!m_reveal.isActive())m_reveal.start(m_revealDelay);}
  else if(!m_popupCount&&!m_hide.isActive())m_hide.start(m_hideDelay);
  if(m_surface=="dock"&&m_edge=="top"&&m_topBar){m_topBar->m_inside=true;m_topBar->point(position);}
 }
 void update(){
  m_shown=m_enabled&&(!m_desktop||m_surface=="background"||!m_autoHide||!m_autoHidden||m_revealed||m_popupCount);
  apply();emit shownChanged();
 }
 void apply(){
  if(!m_desktop||(m_surface!="bar"&&m_surface!="dock"))return;
  // Keep an independent native edge strip throughout the reveal animation.
  const QRegion mask=!m_enabled?QRegion(QRect(-2,-2,1,1)):m_popupCount?QRegion():m_shown?(QRegion(content())|QRegion(edge())):QRegion(edge());
  if(!m_maskSet||mask!=m_mask){m_mask=mask;m_maskSet=true;m_view->setMask(mask);}
#ifdef SOVEL_LAYER_SHELL
  // The reserved strip never changes while the bar reveals, hides or opens a
  // menu. Every change makes Labwc re-arrange all windows, which moved windows
  // on other displays to the display under the pointer.
  using L=LayerShellQt::Window;
  auto layer=L::get(m_view);
  const int zone=m_surface=="bar"&&m_reserve?30:0;
  const auto keyboard=!m_enabled?L::KeyboardInteractivityNone:m_popupCount?L::KeyboardInteractivityExclusive:L::KeyboardInteractivityOnDemand;
  if(!m_layerSet){layer->setLayer(L::LayerOverlay);m_layerSet=true;}
  if(zone!=m_zone){m_zone=zone;layer->setExclusiveZone(zone);}
  if(keyboard!=m_keyboard){m_keyboard=keyboard;layer->setKeyboardInteractivity(keyboard);}
#endif
 }
 QQuickView *m_view;QString m_surface,m_edge="bottom";bool m_desktop,m_enabled=true,m_autoHide=true,m_autoHidden=false,m_shown=true,m_revealed=false,m_inside=false,m_reserve=false;
 QRegion m_mask;bool m_maskSet=false,m_layerSet=false;int m_zone=-1,m_keyboard=-1;
 QPointer<Chrome> m_topBar;
 QSet<QObject*> m_popups;
 int m_popupCount=0,m_sensitivity=8,m_revealDelay=80,m_hideDelay=450;QRect m_content;QPoint m_position{-1,-1};QTimer m_reveal,m_hide;
};
