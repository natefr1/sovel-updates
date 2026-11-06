#include <QtTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QJSValue>
#include <QDateTime>
class CalendarTests:public QObject {
 Q_OBJECT
private slots:
 void leapMonthAndYearNavigation(){
  QQmlEngine engine;QQmlComponent component(&engine,QUrl::fromLocalFile(QString(SOVEL_QML_DIR)+"/CalendarPanel.qml"));std::unique_ptr<QObject> root(component.create());QVERIFY2(root,qPrintable(component.errorString()));
  root->setProperty("month",QDateTime(QDate(2028,2,1),QTime(12,0)));auto days=root->property("days").value<QJSValue>();QCOMPARE(days.property("length").toInt(),42);bool leap=false;for(int i=0;i<42;i++)leap|=days.property(i).toDateTime().date()==QDate(2028,2,29);QVERIFY(leap);
  root->setProperty("month",QDateTime(QDate(2026,12,1),QTime(12,0)));QVERIFY(QMetaObject::invokeMethod(root.get(),"shift",Q_ARG(QVariant,QVariant(1))));QCOMPARE(root->property("month").toDateTime().date(),QDate(2027,1,1));
 }
};
QTEST_MAIN(CalendarTests)
#include "calendar.moc"
