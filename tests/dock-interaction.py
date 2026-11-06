"""Actual QML interaction checks with a recording backend; no apps are launched.

Requires PySide6. This checks the dock component, not the native session or PAM.
"""
import os
import sys
import tempfile
import subprocess
from pathlib import Path
import PySide6
from PySide6.QtCore import QObject, Property, Signal, Slot, QUrl, QPoint, QPointF, Qt, QResource
from PySide6.QtGui import QGuiApplication, QColor
from PySide6.QtQuick import QQuickView
from PySide6.QtTest import QTest

os.environ['QT_QUICK_CONTROLS_STYLE'] = 'Basic'
source = Path(__file__).resolve().parents[1]
qt_dir = Path(PySide6.__file__).parent

class RecordingBackend(QObject):
    preferencesChanged = Signal()
    def __init__(self):
        super().__init__()
        self.reduced = False
        self.launches = []
    @Property(bool, notify=preferencesChanged)
    def reducedMotion(self):
        return self.reduced
    @Property(bool, notify=preferencesChanged)
    def lowEffects(self):
        return False
    @Slot(str)
    def launch(self, app):
        self.launches.append(app)

def descendants(item):
    yield item
    for child in item.childItems():
        yield from descendants(child)

with tempfile.TemporaryDirectory(prefix='sovel-dock-') as scratch:
    scratch = Path(scratch)
    suffix = '.exe' if sys.platform == 'win32' else ''
    shader = scratch / 'glass.frag.qsb'
    subprocess.run([str(qt_dir / ('qsb' + suffix)), '--glsl', '100 es,120,150', '--hlsl', '50', '--msl', '12', '-o', str(shader), str(source / 'shaders/glass.frag')], check=True)
    qrc = scratch / 'preview.qrc'
    qrc.write_text('<RCC><qresource prefix="/">'
                   f'<file alias="assets/landscape.png">{(source / "assets/landscape.png").as_posix()}</file>'
                   f'<file alias="shaders/glass.frag.qsb">{shader.as_posix()}</file>'
                   + ''.join(f'<file alias="assets/icons/{p.name}">{p.as_posix()}</file>' for p in (source/'assets/icons').glob('*.svg')) +
                   '</qresource></RCC>', encoding='utf-8')
    binary = scratch / 'preview.rcc'
    rcc = Path(sys.executable).parent / ('pyside6-rcc' + suffix)
    subprocess.run([str(rcc), '--binary', str(qrc), '-o', str(binary)], check=True)
    app = QGuiApplication(sys.argv)
    assert QResource.registerResource(str(binary))
    backend = RecordingBackend()
    view = QQuickView()
    view.setColor(QColor('#171020'))
    view.rootContext().setContextProperty('System', backend)
    view.setResizeMode(QQuickView.ResizeMode.SizeRootObjectToView)
    view.setSource(QUrl.fromLocalFile(str(source / 'qml/Dock.qml')))
    assert view.status() != QQuickView.Status.Error, view.errors()
    view.resize(700, 112)
    view.show()
    assert QTest.qWaitForWindowExposed(view)
    QTest.qWait(200)
    root = view.rootObject()
    buttons = {i.objectName(): i for i in descendants(root) if i.objectName().startswith('dock-')}
    assert len(buttons) == 5, buttons.keys()
    files = buttons['dock-files']
    center = files.mapToScene(QPointF(files.width()/2, files.height()/2))
    QTest.mouseMove(view, center.toPoint())
    QTest.qWait(700)
    assert files.width() > 60, files.width()
    # This point is above the old fixed 50-pixel slot's clickable area.
    enlarged_top = files.mapToScene(QPointF(files.width()/2, 7)).toPoint()
    assert enlarged_top.y() < 39, enlarged_top
    QTest.mouseClick(view, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier, enlarged_top)
    assert backend.launches == ['files'], backend.launches
    files.forceActiveFocus()
    QTest.keyClick(view, Qt.Key.Key_Right)
    assert buttons['dock-library'].hasActiveFocus()
    QTest.keyClick(view, Qt.Key.Key_Space)
    assert backend.launches[-1] == 'library'
    QTest.keyClick(view, Qt.Key.Key_End)
    assert buttons['dock-store'].hasActiveFocus()
    QTest.keyClick(view, Qt.Key.Key_Home)
    assert files.hasActiveFocus()
    QTest.keyClick(view, Qt.Key.Key_Return)
    assert backend.launches == ['files', 'library', 'files'], backend.launches
    backend.reduced = True
    backend.preferencesChanged.emit()
    QTest.qWait(100)
    assert abs(files.width()-50) < .01, files.width()
    assert abs(files.y()) < .01, files.y()
    QTest.keyClick(view, Qt.Key.Key_Space)
    QTest.qWait(50)
    assert abs(files.y()) < .01
    assert backend.launches == ['files', 'library', 'files', 'files']
    print('PASS: enlarged hit target, keyboard navigation, single activation, reduced-motion interruption')
    view.close()
    del view
