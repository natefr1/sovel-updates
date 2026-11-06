#include "chrome.h"
#include <QtTest>
class ChromeTests:public QObject {
 Q_OBJECT
private slots:
 void disabledBarDoesNotCaptureTheDesktop(){
  QQuickView view;view.resize(1280,720);Chrome chrome(&view,"bar",true);chrome.configure({{"enabled",false}});QVERIFY(!chrome.shown());QVERIFY(!view.mask().isEmpty());QVERIFY(!view.mask().intersects(QRect(0,0,1280,720)));chrome.configure({{"enabled",true}});QVERIFY(chrome.shown());QVERIFY(view.mask().contains(QPoint(100,10)));
 }
 void eachEdgeSurvivesGeometryChanges(){
  for(auto size:{QSize(1280,720),QSize(640,360),QSize(1920,1080)})for(const auto &edge:QStringList{"bottom","top","left","right"}){
   QQuickView view;view.resize(size);Chrome chrome(&view,"dock",true);chrome.configure({{"activationEdge",edge},{"sensitivity",10},{"revealDelay",20},{"hideDelay",100}});chrome.setContentRect(100,100,300,100);chrome.setAutoHidden(true);QVERIFY(!chrome.shown());
   QPoint p=edge=="bottom"?QPoint(size.width()/2,size.height()-1):edge=="top"?QPoint(size.width()/2,1):edge=="left"?QPoint(1,size.height()/2):QPoint(size.width()-1,size.height()/2);
   QVERIFY(view.mask().contains(p));QEnterEvent enter(p,p,p);QCoreApplication::sendEvent(&view,&enter);QTRY_VERIFY_WITH_TIMEOUT(chrome.shown(),1000);
   QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(&view,&leave);QTRY_VERIFY_WITH_TIMEOUT(!chrome.shown(),1000);QVERIFY(view.mask().contains(p));
  }
 }
 void pointerAndPopupsHoldTheBar(){
  QQuickView view;view.resize(1280,720);Chrome chrome(&view,"bar",true);chrome.configure({{"revealDelay",0},{"hideDelay",100}});chrome.setAutoHidden(true);
  QEnterEvent enter(QPointF(20,1),QPointF(20,1),QPointF(20,1));QCoreApplication::sendEvent(&view,&enter);QTRY_VERIFY(chrome.shown());
  QMouseEvent move(QEvent::MouseMove,QPointF(20,25),QPointF(20,25),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(&view,&move);
  chrome.setPopupOpen(true);chrome.setPopupOpen(true);chrome.setPopupOpen(false);QTest::qWait(160);QVERIFY(chrome.shown());chrome.setPopupOpen(false);QTest::qWait(160);QVERIFY(chrome.shown());
  QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(&view,&leave);QTRY_VERIFY_WITH_TIMEOUT(!chrome.shown(),1000);
 }
 void aTopDockDoesNotStealTheMenuBarsActivationEdge(){
  QQuickView barView,dockView;barView.resize(1280,720);dockView.resize(1280,720);
  Chrome bar(&barView,"bar",true),dock(&dockView,"dock",true);dock.shareTopEdgeWith(&bar);
  bar.configure({{"revealDelay",0},{"hideDelay",100}});dock.configure({{"activationEdge","top"},{"revealDelay",0},{"hideDelay",100}});bar.setAutoHidden(true);dock.setAutoHidden(true);
  QEnterEvent enter(QPointF(640,1),QPointF(640,1),QPointF(640,1));QCoreApplication::sendEvent(&dockView,&enter);
  QTRY_VERIFY(bar.shown());QTRY_VERIFY(dock.shown());QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(&dockView,&leave);QTRY_VERIFY(!bar.shown());QTRY_VERIFY(!dock.shown());
 }
 void disappearingPopupCannotKeepInputCaptured(){
  QQuickView view;view.resize(1280,720);Chrome chrome(&view,"bar",true);chrome.setAutoHidden(true);
  auto popup=new QObject;chrome.registerPopup(popup,true);chrome.registerPopup(popup,true);QVERIFY(chrome.popupOpen());delete popup;QVERIFY(!chrome.popupOpen());QVERIFY(!chrome.shown());QVERIFY(!view.mask().contains(QPoint(600,400)));
  QObject first,second;chrome.registerPopup(&first,true);chrome.registerPopup(&second,true);chrome.registerPopup(&first,false);QVERIFY(chrome.popupOpen());chrome.releasePopups();QVERIFY(!chrome.popupOpen());QVERIFY(!view.mask().contains(QPoint(600,400)));
 }
};
QTEST_MAIN(ChromeTests)
#include "chrome.moc"
