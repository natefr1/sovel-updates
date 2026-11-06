#include "store.h"
#include <QCoreApplication>
#include <QTimer>
#include <QTextStream>
// Explicit integration check, not part of offline CTest. Optional installation
// is limited to the same reviewed app that is returned by the live catalog.
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);Store store;int phase=0;
    const bool install=qEnvironmentVariableIntValue("SOVEL_TEST_STORE_INSTALL")==1;
    QObject::connect(&store,&Store::changed,&app,[&]{
        if(store.busy())return;
        if(phase==0){
            bool found=false;for(const auto &v:store.results())if(v.toMap()["id"].toString()=="com.brave.Browser")found=true;
            if(!found){QTextStream(stderr)<<"Catalog failed: "<<store.status()<<Qt::endl;app.exit(1);return;}
            QTextStream(stdout)<<"Live catalog: Brave found"<<Qt::endl;
            phase=1;store.review("com.brave.Browser");
        }else if(phase==1){
            if(!store.permissions().contains("[Context]")){QTextStream(stderr)<<"Permission review failed: "<<store.status()<<Qt::endl;app.exit(2);return;}
            QTextStream(stdout)<<"Live permission review: passed"<<Qt::endl;
            phase=2;
            if(install){QTextStream(stdout)<<"Installing reviewed Brave Flatpak"<<Qt::endl;store.installSelected();}
            else app.exit(0);
        }else{
            QTextStream(stdout)<<store.status()<<Qt::endl;
            app.exit(store.status().startsWith("Installation complete")?0:3);
        }
    });
    QTimer::singleShot(0,&app,[&]{store.search("Brave");});
    QTimer::singleShot(600000,&app,[&]{store.cancel();app.exit(4);});
    return app.exec();
}
