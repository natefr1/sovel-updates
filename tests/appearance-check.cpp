#include "backend.h"
#include "appicons.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QImage>
#include <QTextStream>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <QTextLayout>
#include <QGlyphRun>
#define CHECK(test) do{if(!(test)){QTextStream(stderr)<<"Appearance check failed: " #test<<Qt::endl;return 1;}}while(false)
int main(int argc,char **argv){
 QGuiApplication app(argc,argv);QTemporaryDir temporary;CHECK(temporary.isValid());
 qputenv("XDG_CONFIG_HOME",temporary.path().toUtf8());qputenv("XDG_DATA_HOME",(temporary.path()+"/data").toUtf8());
 const auto original=temporary.path()+"/日本語 ' background.png";QImage image(60,40,QImage::Format_ARGB32);image.fill(QColor("#247848"));CHECK(image.save(original));
 Backend writer,reader;CHECK(writer.setWallpaperFile(QUrl::fromLocalFile(original)));const auto saved=writer.wallpaperUrl();CHECK(saved.isLocalFile());CHECK(saved.toLocalFile()!=original);CHECK(QFile::exists(saved.toLocalFile()));
 CHECK(QFile::remove(original));CHECK(QImage(saved.toLocalFile()).size()==QSize(60,40));
 QElapsedTimer elapsed;elapsed.start();while(reader.wallpaperUrl()!=saved&&elapsed.elapsed()<5000){QGuiApplication::processEvents();QThread::msleep(30);}CHECK(reader.wallpaperUrl()==saved);
 CHECK(!writer.setWallpaperFile(QUrl("https://example.invalid/image.png")));CHECK(writer.wallpaperUrl()==saved);
 for(const auto &format:QStringList{"PNG","JPEG","WEBP"}){const auto file=temporary.path()+"/photo."+format.toLower();CHECK(image.save(file,format.toLatin1().constData()));CHECK(writer.setWallpaperFile(QUrl::fromLocalFile(file)));CHECK(QImage(writer.wallpaperUrl().toLocalFile()).size()==image.size());}
 const auto icon=temporary.path()+"/app.png";CHECK(image.save(icon));
 const auto desktop=temporary.path()+"/steam.desktop";QFile entry(desktop);CHECK(entry.open(QIODevice::WriteOnly));entry.write("[Desktop Entry]\nType=Application\nName=Steam\nIcon="+icon.toUtf8()+"\n");entry.close();
 AppIcons icons;QSize result;const auto pixmap=icons.requestPixmap(QString::fromLatin1(QUrl::toPercentEncoding("desktop:"+desktop)),&result,QSize(32,32));CHECK(!pixmap.isNull());CHECK(pixmap.toImage().pixelColor(16,16)==QColor("#247848"));
 const auto folder=temporary.path()+"/data/icons/hicolor/128x128/apps";CHECK(QDir().mkpath(folder));CHECK(image.save(folder+"/com.spotify.Client.png"));
 CHECK(entry.open(QIODevice::WriteOnly|QIODevice::Truncate));entry.write("[Desktop Entry]\nType=Application\nName=Spotify\nIcon=com.spotify.Client\nX-Flatpak=com.spotify.Client\n");entry.close();
 AppIcons refreshed;const auto spotify=refreshed.requestPixmap(QString::fromLatin1(QUrl::toPercentEncoding("desktop:"+desktop)),&result,QSize(32,32));CHECK(!spotify.isNull());CHECK(spotify.toImage().pixelColor(16,16)==QColor("#247848"));
 CHECK(!refreshed.requestPixmap("no-such-icon",&result,QSize(32,32)).isNull());
 writer.setWallpaper("lockroom");CHECK(writer.wallpaperUrl().toString()=="qrc:/assets/lockroom.png");
 QTextLayout text(QString::fromUtf8("日本語 中文 한국어 العربية हिन्दी Русский 😀"),QFont("Noto Sans",18));text.beginLayout();auto line=text.createLine();line.setLineWidth(1800);text.endLayout();
 CHECK(!text.glyphRuns().isEmpty());for(const auto &run:text.glyphRuns())for(const auto glyph:run.glyphIndexes())CHECK(glyph!=0);
 QTextStream(stdout)<<"MULTISCRIPT_QT_GLYPH_COVERAGE_PASSED"<<Qt::endl;
 QTextStream(stdout)<<"WALLPAPER_COPY_PERSISTENCE_AND_REAL_ICON_RESOLUTION_PASSED"<<Qt::endl;return 0;
}
