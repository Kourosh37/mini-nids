"""Qt-only capture/table interaction checks; no packet analysis or user files."""
import os
for variable in ['QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','LD_LIBRARY_PATH']:
    os.environ.pop(variable, None)
os.environ.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software', QML_DISABLE_DISK_CACHE='1')
import shutil, subprocess, time, tempfile
import PySide6
from pathlib import Path
from xml.sax.saxutils import escape
from PySide6.QtCore import QObject, Property, Signal, Slot, Qt, QUrl, QPoint, QPointF, QResource, QMetaObject, QCoreApplication, QEvent, qVersion, QAbstractTableModel, QModelIndex
from PySide6.QtGui import QGuiApplication, QWheelEvent
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuickControls2 import QQuickStyle
from PySide6.QtTest import QTest

root = Path(__file__).resolve().parents[2]
temporary = tempfile.TemporaryDirectory(prefix='mini-nids-ui-')
work = Path(temporary.name)
qml = work/'qml'
qml.mkdir(parents=True, exist_ok=True)
for source in (root/'apps/gui/qml').glob('*.qml'): shutil.copy2(source, qml/source.name)
(qml/'qmldir').write_text('module MiniNids\nsingleton Theme 1.0 Theme.qml\n')
resources = '<RCC>'
for prefix, paths in [('fonts', list((root/'assets/fonts').glob('*.ttf'))), ('icons', [root/'assets/icons/org.mininids.MiniNIDS.png'])]:
    resources += '<qresource prefix="/'+prefix+'">'+''.join('<file alias="'+p.name+'">'+escape(p.as_posix())+'</file>' for p in paths)+'</qresource>'
(work/'assets.qrc').write_text(resources+'</RCC>')
qt_tools = Path(PySide6.__file__).resolve().parent
rcc = next(path for path in [qt_tools/'rcc.exe', qt_tools/'rcc', qt_tools/'Qt/libexec/rcc'] if path.is_file())
subprocess.run([str(rcc),'--binary',str(work/'assets.qrc'),'-o',str(work/'assets.rcc')], check=True)
(qml/'Preview.qml').write_text('''import QtQuick
import QtQuick.Controls
ApplicationWindow {
  id: preview
  width: 1080; height: 760; visible: true; color: Theme.background
  property bool showTables: false
  font.family: Theme.uiFontFamily; font.pixelSize: 14
  palette.window: Theme.surface; palette.windowText: Theme.text
  palette.base: Theme.background; palette.text: Theme.text
  palette.button: Theme.surface; palette.buttonText: Theme.text
  palette.highlight: Theme.accent; palette.highlightedText: Theme.background
  palette.toolTipBase: Theme.surface; palette.toolTipText: Theme.text
  header: Rectangle { implicitHeight: 74; color: Theme.surface; Label { anchors.left: parent.left; anchors.leftMargin: 20; anchors.verticalCenter: parent.verticalCenter; text: "Capture"; color: Theme.text; font.pixelSize: 18 } }
  footer: Rectangle { implicitHeight: 38; color: Theme.sidebar; Label { anchors.fill: parent; anchors.margins: 10; text: "Synthetic UI check"; color: Theme.muted; font.pixelSize: 11 } }
  CapturePage { visible: !preview.showTables; anchors.fill: parent; anchors.margins: parent.width < 600 ? 12 : 24 }
  DataPage { visible: preview.showTables; anchors.fill: parent; anchors.margins: parent.width < 600 ? 12 : 24; dataModel: tableModel; title: "Packets"; pageIndex: 2 }
}''')

class Controller(QObject):
    changed = Signal()
    archiveEntriesChanged = Signal()
    def __init__(self):
        super().__init__()
        self.values = dict(busy=False,progress=0.4,status='Select a capture to prepare',inputFile='',outputRoot='C:/Reports/MiniNIDS',groundTruth='',preparedEntry=-1)
        self.entries = [dict(name=f'dataset/capture-{i:03}.pcap',bytes=2500000+i,supported=i!=5,pcapng=i%2==0,reason='Invalid header' if i==5 else '',warning='') for i in range(250)]
        self.entries[-1]['name'] = 'dataset/target-capture.pcap'
        self.prepared = -1
        self.starts = 0
    def update(self, **values): self.values.update(values); self.changed.emit()
    def set_input(self, value):
        self.values['inputFile'] = value; self.values['preparedEntry'] = -1
        self.archiveEntriesChanged.emit(); self.changed.emit()
    summary = Property("QVariantMap", lambda s: {}, notify=changed)
    busy = Property(bool, lambda s:s.values['busy'], notify=changed)
    progress = Property(float, lambda s:s.values['progress'], notify=changed)
    status = Property(str, lambda s:s.values['status'], notify=changed)
    inputFile = Property(str, lambda s:s.values['inputFile'], set_input, notify=changed)
    outputRoot = Property(str, lambda s:s.values['outputRoot'], lambda s,v:s.update(outputRoot=v), notify=changed)
    groundTruth = Property(str, lambda s:s.values['groundTruth'], lambda s,v:s.update(groundTruth=v), notify=changed)
    preparedEntry = Property(int, lambda s:s.values['preparedEntry'], notify=changed)
    archiveInput = Property(bool, lambda s:s.values['inputFile'].endswith('.zip'), notify=changed)
    archiveEntries = Property('QVariantList', lambda s:s.entries, notify=archiveEntriesChanged)
    recentFiles = Property('QStringList', lambda s:['C:/Captures/previous.pcap'], notify=changed)
    canAnalyze = Property(bool, lambda s:not s.busy and bool(s.inputFile) and (not s.archiveInput or s.preparedEntry>=0), notify=changed)
    @Slot(int)
    def prepareArchiveEntry(self, row):
        self.prepared = row; self.values['preparedEntry'] = row
        self.entries[row]['warning'] = 'Metadata checked; packet data preserved.'
        self.archiveEntriesChanged.emit(); self.changed.emit()
    @Slot()
    def start(self): self.starts += 1; self.update(busy=True,status='Analyzing capture…')
    @Slot()
    def cancel(self): self.update(busy=False,status='Analysis cancelled')

class TableModel(QAbstractTableModel):
    changed = Signal()
    def __init__(self):
        super().__init__(); self.current = 0; self.size = 100; self.query = ""
    def rowCount(self, parent=QModelIndex()): return 0 if parent.isValid() else min(self.size, 1300-self.current*self.size)
    def columnCount(self, parent=QModelIndex()): return 0 if parent.isValid() else 6
    def data(self, index, role):
        if role == Qt.DisplayRole and index.isValid():
            return [self.current*self.size+index.row()+1, "100.000000000", "192.0.2.1", "198.51.100.1", "TCP", 64][index.column()]
    def headerData(self, section, orientation, role):
        if role == Qt.DisplayRole and orientation == Qt.Horizontal: return ["#", "Capture time", "Source", "Destination", "Protocol", "Bytes"][section]
    def roleNames(self): return {int(Qt.DisplayRole):b"display"}
    def set_page(self, value):
        self.beginResetModel(); self.current = max(0,min(value,self.pageCount-1)); self.endResetModel(); self.changed.emit()
    def set_size(self, value):
        self.size=value; self.set_page(0)
    def set_filter(self, value): self.query=value; self.changed.emit()
    count = Property(int, lambda s:s.rowCount(), notify=changed)
    totalCount = Property(int, lambda s:1300, notify=changed)
    matchCount = Property(int, lambda s:1300, notify=changed)
    page = Property(int, lambda s:s.current, set_page, notify=changed)
    pageSize = Property(int, lambda s:s.size, set_size, notify=changed)
    pageCount = Property(int, lambda s:(1300+s.size-1)//s.size, notify=changed)
    filter = Property(str, lambda s:s.query, set_filter, notify=changed)
    queryBusy = Property(bool, lambda s:False, notify=changed)
    queryError = Property(str, lambda s:"", notify=changed)
    searchHelp = Property(str, lambda s:"Fields: id, time, src, dst, protocol, bytes, port", constant=True)
    @Slot(int)
    def sortColumn(self, column): pass
    @Slot(int, result=str)
    def rowSummary(self, row): return f"{self.current*self.size+row+1} | TCP | 192.0.2.1 | 198.51.100.1 | 64"

assert qVersion() == '6.8.3', qVersion()
app = QGuiApplication([])
QQuickStyle.setStyle('Basic')
assert QResource.registerResource(str(work/'assets.rcc'))
controller = Controller()
table_model = TableModel()
engine = QQmlApplicationEngine()
warnings = []
engine.warnings.connect(lambda errors:warnings.extend(e.toString() for e in errors))
engine.rootContext().setContextProperty('controller', controller)
engine.rootContext().setContextProperty('tableModel', table_model)
try:
    engine.load(QUrl.fromLocalFile(str(qml/'Preview.qml')))
    assert engine.rootObjects(), warnings
    window = engine.rootObjects()[0]

    def items(item=None):
        item = item or window.contentItem()
        yield item
        for child in item.childItems(): yield from items(child)
    def find(name): return next(item for item in items() if item.objectName()==name)
    def point(item): return item.mapToScene(QPointF(item.width()/2,item.height()/2)).toPoint()
    def click(item): QTest.mouseClick(window, Qt.LeftButton, Qt.NoModifier, point(item)); QTest.qWait(120)
    def render(name):
        QTest.qWait(120)
        image = window.grabWindow()
        assert not image.isNull(), name
        if os.environ.get('MININIDS_UI_SCREENSHOTS'):
            destination = Path(os.environ['MININIDS_UI_SCREENSHOTS']); destination.mkdir(parents=True,exist_ok=True)
            assert image.save(str(destination/(name+'.png')))
    def wheel(item):
        position = item.mapToScene(QPointF(30,item.height()/2))
        QTest.mouseMove(window,position.toPoint()); QTest.qWait(40)
        event = QWheelEvent(position, QPointF(window.mapToGlobal(position.toPoint())), QPoint(), QPoint(0,-480), Qt.NoButton, Qt.NoModifier, Qt.NoScrollPhase, False)
        event.setTimestamp(int(time.monotonic()*1000))
        QGuiApplication.sendEvent(window,event); QTest.qWait(100)

    QTest.qWait(150)
    render('empty-wide')
    controller.set_input('C:/Captures/dataset.zip')
    QTest.qWait(150)
    view = find('archiveList')
    source = find('captureSourcePanel')
    actions = find('captureActionPanel')
    source_y, action_y = source.mapToScene(QPointF()).y(), actions.mapToScene(QPointF()).y()
    for _ in range(4): wheel(view)
    assert view.property('contentY') > 0
    assert source.mapToScene(QPointF()).y()==source_y and actions.mapToScene(QPointF()).y()==action_y
    controller.update(busy=True,status='Preparing capture…')
    QTest.qWait(100)
    assert QMetaObject.invokeMethod(view,'cancelFlick',Qt.DirectConnection)
    scroll_y = view.property('contentY')
    for i in range(20): controller.update(progress=i/25); QTest.qWait(10)
    assert abs(view.property('contentY')-scroll_y)<1, (scroll_y,view.property('contentY'))
    controller.entries[-1]['warning'] = 'Metadata warning'
    controller.archiveEntriesChanged.emit()
    QTest.qWait(80)
    assert abs(view.property('contentY')-scroll_y)<1
    controller.cancel()
    render('zip-wide-scrolled')
    old_scroll = view.property('contentY')
    click(find('captureOptionsButton'))
    output = find('reportOutputInput')
    output.forceActiveFocus()
    QTest.keyClick(window,Qt.Key_A,Qt.ControlModifier)
    QGuiApplication.clipboard().setText('C:/Reports/Changed')
    QTest.keyClick(window,Qt.Key_V,Qt.ControlModifier)
    click(find('captureOptionsDone'))
    assert controller.outputRoot == 'C:/Reports/Changed'
    assert abs(view.property('contentY')-old_scroll)<1
    search = find('archiveSearch')
    search.forceActiveFocus()
    QGuiApplication.clipboard().setText('target-capture')
    QTest.keyClick(window,Qt.Key_V,Qt.ControlModifier)
    QTest.qWait(120)
    assert view.property('count')==1
    prepare = next(item for item in items() if item.objectName()=='prepareCaptureButton' and item.isVisible())
    click(prepare)
    assert controller.prepared==249 and controller.canAnalyze
    click(find('captureOptionsButton'))
    options = window.findChild(QObject,'captureOptionsDialog')
    assert options is not None and options.property('visible')
    render('options-wide')
    QTest.keyClick(window,Qt.Key_Escape)
    QTest.qWait(100)
    assert not options.property('visible')
    click(find('analyzeButton'))
    assert controller.starts==1 and controller.busy
    assert find('cancelTaskButton').isVisible()
    click(find('cancelTaskButton'))
    assert not controller.busy
    search.setProperty('text','no-such-capture')
    QTest.qWait(100)
    assert view.property('count') == 0 and not find('prepareCaptureButton').property('enabled')
    assert not find('analyzeButton').isVisible()
    search.setProperty('text','')
    QTest.qWait(100)
    view.forceActiveFocus()
    QTest.keyClick(window,Qt.Key_Down)
    assert not find('analyzeButton').isVisible()
    assert find('prepareCaptureButton').property('enabled')
    QTest.keyClick(window,Qt.Key_Return)
    QTest.qWait(100)
    assert controller.prepared == 1
    search.setProperty('text','target-capture')

    for width,height in [(760,760),(420,860),(360,600)]:
        controller.set_input('C:/Captures/another-dataset.zip')
        assert search.property('text')==''
        window.resize(width,height); QTest.qWait(150)
        source_y,action_y=source.mapToScene(QPointF()).y(),actions.mapToScene(QPointF()).y()
        before=view.property('contentY')
        wheel(view)
        assert view.property('contentY')>before, (width,before,view.property('contentY'),view.property('count'),view.height(),view.property('contentHeight'),view.property('interactive'),view.property('currentIndex'),view.isVisible(),controller.busy,warnings)
        assert source.mapToScene(QPointF()).y()==source_y and actions.mapToScene(QPointF()).y()==action_y
        button=find('prepareCaptureButton')
        assert 0 <= point(button).y() < height-38
        render('zip-'+str(width))
        click(find('captureOptionsButton'))
        assert options.property('visible')
        render('options-'+str(width))
        QTest.keyClick(window,Qt.Key_Escape); QTest.qWait(80)
    controller.set_input('C:/Captures/example.pcap')
    assert controller.canAnalyze
    render('pcap-compact')
    window.setProperty('showTables', True)
    window.resize(1080,760); QTest.qWait(150)
    click(find('nextPage')); assert table_model.page == 1
    click(find('previousPage')); assert table_model.page == 0
    table_search = find('tableSearch')
    table_search.forceActiveFocus()
    QGuiApplication.clipboard().setText('protocol:TCP bytes>=1000')
    QTest.keyClick(window,Qt.Key_V,Qt.ControlModifier)
    QTest.qWait(150)
    assert table_model.filter == 'protocol:TCP bytes>=1000'
    render('table-wide')
    for width,height in [(760,760),(420,860),(360,600)]:
        window.resize(width,height); QTest.qWait(150)
        for name in ['previousPage','nextPage','pageNumber']:
            item=find(name); position=point(item)
            assert item.isVisible() and 0 <= position.x() < width and 0 <= position.y() < height, (name,width,position)
        click(find('nextPage'))
        if width < 600:
            assert find('compactTableRow').property('text').startswith(str(table_model.page*100+1)+' |')
        render('table-'+str(width))
    assert not warnings,'\n'.join(warnings)
    print('Qt 6.8.3 passed: actual wheel scrolling with fixed source/actions, progress and row-update scroll retention, 250-entry filtering with original ZIP indices, Options/Escape, start/cancel, paged tables, live search input, and 1080/760/420/360px layouts.')
    window.close()
finally:
    engine.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
    assert QResource.unregisterResource(str(work/'assets.rcc'))
    temporary.cleanup()
