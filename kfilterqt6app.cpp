/***************************************************************************
                          kfilterqt6app.cpp  -  Qt6 application shell
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#include "kfilterqt6app.h"

#include "circuitout.h"
#include "driver.h"
#include "driverparametersdialog.h"
#include "networkparametersdialog.h"
#include "kfilterdoc.h"
#include "kfilterprojectio.h"
#include "kfilterview.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QGroupBox>
#include <QSizePolicy>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QFrame>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>


namespace
{
QString ensureProjectSuffix(QString filePath)
{
    if (!filePath.endsWith(QStringLiteral(".kfp"), Qt::CaseInsensitive)) {
        filePath += QStringLiteral(".kfp");
    }
    return filePath;
}

}

KFilterQt6App::KFilterQt6App(QWidget *parent)
    : QMainWindow(parent),
      m_doc(new KFilterDoc(this)),
      m_lastDirectory(QDir::homePath())
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(5);

    m_stateLabel = new QLabel(central);
    m_stateLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_plotView = new KFilterView(m_doc, central);
    m_plotView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *plotBox = new QGroupBox(tr("Frequency Response / Impedance"), central);
    auto *plotLayout = new QVBoxLayout(plotBox);
    plotLayout->setContentsMargins(6, 8, 6, 6);
    plotLayout->addWidget(m_plotView, 1);

    m_circuitPreview = new CircuitOut(central);
    m_circuitPreview->setMinimumSize(900, 330);
    m_circuitPreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *circuitScrollArea = new QScrollArea(central);
    circuitScrollArea->setWidgetResizable(true);
    circuitScrollArea->setFrameShape(QFrame::NoFrame);
    circuitScrollArea->setWidget(m_circuitPreview);

    auto *circuitBox = new QGroupBox(tr("Network / Filter Schematic"), central);
    auto *circuitLayout = new QVBoxLayout(circuitBox);
    circuitLayout->setContentsMargins(6, 8, 6, 6);
    circuitLayout->addWidget(circuitScrollArea, 1);

    m_mainSplitter = new QSplitter(Qt::Vertical, central);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(8);
    m_mainSplitter->addWidget(plotBox);
    m_mainSplitter->addWidget(circuitBox);
    m_mainSplitter->setStretchFactor(0, 5);
    m_mainSplitter->setStretchFactor(1, 4);

    layout->addWidget(m_stateLabel);
    layout->addWidget(m_mainSplitter, 1);
    setCentralWidget(central);

    createActions();
    createMenusAndToolBar();

    connect(m_doc, &KFilterDoc::forceviewrefresh, this, &KFilterQt6App::refreshOverview);
    connect(m_doc, &KFilterDoc::forceviewrefresh, m_plotView, [this]() {
        m_plotView->update();
    });
    connect(m_doc, &KFilterDoc::forceviewrefresh, this, &KFilterQt6App::refreshCircuitPreview);

    m_doc->newDocument();
    loadSettings();
    statusBar()->showMessage(tr("Ready."));
    refreshOverview();
}

KFilterQt6App::~KFilterQt6App() = default;

void KFilterQt6App::createActions()
{
    m_newAction = new QAction(tr("&New"), this);
    m_newAction->setShortcut(QKeySequence::New);
    connect(m_newAction, &QAction::triggered, this, &KFilterQt6App::newFile);

    m_openAction = new QAction(tr("&Open..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &KFilterQt6App::openFile);

    m_saveAction = new QAction(tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &KFilterQt6App::saveFile);

    m_saveAsAction = new QAction(tr("Save &As..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &KFilterQt6App::saveFileAs);

    m_quitAction = new QAction(tr("&Quit"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    m_driverParametersAction = new QAction(tr("Driver &Parameters..."), this);
    connect(m_driverParametersAction, &QAction::triggered, this, &KFilterQt6App::editDriverParameters);

    m_networkParametersAction = new QAction(tr("&Network / Filter Parameters..."), this);
    connect(m_networkParametersAction, &QAction::triggered, this, &KFilterQt6App::editNetworkParameters);

    m_showFileToolBarAction = new QAction(tr("Show &File Toolbar"), this);
    m_showFileToolBarAction->setCheckable(true);
    m_showFileToolBarAction->setChecked(true);
    connect(m_showFileToolBarAction, &QAction::toggled, this, &KFilterQt6App::setFileToolBarVisible);

    m_showEditToolBarAction = new QAction(tr("Show &Edit Toolbar"), this);
    m_showEditToolBarAction->setCheckable(true);
    m_showEditToolBarAction->setChecked(true);
    connect(m_showEditToolBarAction, &QAction::toggled, this, &KFilterQt6App::setEditToolBarVisible);

    m_showStatusBarAction = new QAction(tr("Show &Status Bar"), this);
    m_showStatusBarAction->setCheckable(true);
    m_showStatusBarAction->setChecked(true);
    connect(m_showStatusBarAction, &QAction::toggled, this, &KFilterQt6App::setStatusBarVisible);

    m_resetLayoutAction = new QAction(tr("Reset &Window Layout"), this);
    connect(m_resetLayoutAction, &QAction::triggered, this, &KFilterQt6App::resetWindowLayout);

    m_circuitPreviewDriverActionGroup = new QActionGroup(this);
    m_circuitPreviewDriverActionGroup->setExclusive(true);

    m_circuitPreviewAllDriversAction = new QAction(tr("&All Active Drivers"), this);
    m_circuitPreviewAllDriversAction->setCheckable(true);
    m_circuitPreviewAllDriversAction->setChecked(true);
    m_circuitPreviewDriverActionGroup->addAction(m_circuitPreviewAllDriversAction);
    connect(m_circuitPreviewAllDriversAction, &QAction::triggered, this, [this]() {
        setCircuitPreviewDriverIndex(KFilterProjectIo::DriverCount, true);
    });

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        m_circuitPreviewDriverActions[index] = new QAction(circuitPreviewDriverMenuText(index), this);
        m_circuitPreviewDriverActions[index]->setCheckable(true);
        m_circuitPreviewDriverActions[index]->setEnabled(circuitPreviewDriverSlotAvailable(index));
        m_circuitPreviewDriverActionGroup->addAction(m_circuitPreviewDriverActions[index]);
        connect(m_circuitPreviewDriverActions[index], &QAction::triggered, this, [this, index]() {
            setCircuitPreviewDriverIndex(index, true);
        });
    }

    m_circuitPreviewBackgroundColorAction = new QAction(tr("Background &Color..."), this);
    connect(m_circuitPreviewBackgroundColorAction, &QAction::triggered,
            this, &KFilterQt6App::chooseCircuitPreviewBackgroundColor);

    m_resetCircuitPreviewBackgroundColorAction = new QAction(tr("&Reset Background Color"), this);
    connect(m_resetCircuitPreviewBackgroundColorAction, &QAction::triggered,
            this, &KFilterQt6App::resetCircuitPreviewBackgroundColor);

    m_aboutAction = new QAction(tr("&About KFilter"), this);
    connect(m_aboutAction, &QAction::triggered, this, &KFilterQt6App::showAboutDialog);

    updateCircuitPreviewDriverActions();
}

void KFilterQt6App::createMenusAndToolBar()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_driverParametersAction);
    editMenu->addAction(m_networkParametersAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_showFileToolBarAction);
    viewMenu->addAction(m_showEditToolBarAction);
    viewMenu->addAction(m_showStatusBarAction);
    viewMenu->addSeparator();

    QMenu *networkPreviewMenu = viewMenu->addMenu(tr("&Network Preview"));
    QMenu *driverViewMenu = networkPreviewMenu->addMenu(tr("&Driver View"));
    driverViewMenu->addAction(m_circuitPreviewAllDriversAction);
    driverViewMenu->addSeparator();
    for (QAction *driverAction : m_circuitPreviewDriverActions) {
        if (driverAction != nullptr) {
            driverViewMenu->addAction(driverAction);
        }
    }
    networkPreviewMenu->addSeparator();
    networkPreviewMenu->addAction(m_circuitPreviewBackgroundColorAction);
    networkPreviewMenu->addAction(m_resetCircuitPreviewBackgroundColorAction);

    viewMenu->addAction(m_resetLayoutAction);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_aboutAction);

    m_fileToolBar = addToolBar(tr("File"));
    m_fileToolBar->setObjectName(QStringLiteral("fileToolBar"));
    m_fileToolBar->addAction(m_newAction);
    m_fileToolBar->addAction(m_openAction);
    m_fileToolBar->addAction(m_saveAction);
    m_fileToolBar->addAction(m_saveAsAction);

    m_editToolBar = addToolBar(tr("Edit"));
    m_editToolBar->setObjectName(QStringLiteral("editToolBar"));
    m_editToolBar->addAction(m_driverParametersAction);
    m_editToolBar->addAction(m_networkParametersAction);
}


void KFilterQt6App::setFileToolBarVisible(bool visible)
{
    if (m_fileToolBar != nullptr) {
        m_fileToolBar->setVisible(visible);
    }

    if (m_showFileToolBarAction != nullptr && m_showFileToolBarAction->isChecked() != visible) {
        const QSignalBlocker blocker(m_showFileToolBarAction);
        m_showFileToolBarAction->setChecked(visible);
    }

    statusBar()->showMessage(visible ? tr("File toolbar shown.") : tr("File toolbar hidden."), 3000);
}

void KFilterQt6App::setEditToolBarVisible(bool visible)
{
    if (m_editToolBar != nullptr) {
        m_editToolBar->setVisible(visible);
    }

    if (m_showEditToolBarAction != nullptr && m_showEditToolBarAction->isChecked() != visible) {
        const QSignalBlocker blocker(m_showEditToolBarAction);
        m_showEditToolBarAction->setChecked(visible);
    }

    statusBar()->showMessage(visible ? tr("Edit toolbar shown.") : tr("Edit toolbar hidden."), 3000);
}

void KFilterQt6App::setStatusBarVisible(bool visible)
{
    statusBar()->setVisible(visible);

    if (m_showStatusBarAction != nullptr && m_showStatusBarAction->isChecked() != visible) {
        const QSignalBlocker blocker(m_showStatusBarAction);
        m_showStatusBarAction->setChecked(visible);
    }

    if (visible) {
        statusBar()->showMessage(tr("Status bar shown."), 3000);
    }
}

void KFilterQt6App::resetWindowLayout()
{
    if (m_mainSplitter == nullptr) {
        return;
    }

    // Keep the plot slightly larger than the schematic, while leaving enough
    // room for the complete 8-section network preview and value table.
    m_mainSplitter->setSizes({520, 360});
}

void KFilterQt6App::chooseCircuitPreviewBackgroundColor()
{
    if (m_circuitPreview == nullptr) {
        return;
    }

    const QColor selectedColor = QColorDialog::getColor(
        m_circuitPreview->backgroundColor(),
        this,
        tr("Network Preview Background Color"));

    if (!selectedColor.isValid()) {
        return;
    }

    m_circuitPreview->setBackgroundColor(selectedColor);

    QSettings settings;
    if (selectedColor == CircuitOut::defaultBackgroundColor()) {
        settings.remove(QStringLiteral("CircuitPreview/backgroundColor"));
    } else {
        settings.setValue(QStringLiteral("CircuitPreview/backgroundColor"), selectedColor);
    }

    statusBar()->showMessage(tr("Network preview background color changed."), 3000);
}

void KFilterQt6App::resetCircuitPreviewBackgroundColor()
{
    if (m_circuitPreview == nullptr) {
        return;
    }

    m_circuitPreview->setBackgroundColor(CircuitOut::defaultBackgroundColor());

    QSettings settings;
    settings.remove(QStringLiteral("CircuitPreview/backgroundColor"));

    statusBar()->showMessage(tr("Network preview background color reset."), 3000);
}

void KFilterQt6App::loadSettings()
{
    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("MainWindow/geometry")).toByteArray();
    if (geometry.isEmpty() || !restoreGeometry(geometry)) {
        resize(1180, 860);
    }

    const QByteArray splitterState = settings.value(QStringLiteral("MainWindow/splitterState")).toByteArray();
    if (splitterState.isEmpty() || m_mainSplitter == nullptr || !m_mainSplitter->restoreState(splitterState)) {
        resetWindowLayout();
    }

    const bool showFileToolBar = settings.value(QStringLiteral("MainWindow/showFileToolBar"), true).toBool();
    const bool showEditToolBar = settings.value(QStringLiteral("MainWindow/showEditToolBar"), true).toBool();
    const bool showStatusBar = settings.value(QStringLiteral("MainWindow/showStatusBar"), true).toBool();

    if (m_showFileToolBarAction != nullptr) {
        m_showFileToolBarAction->setChecked(showFileToolBar);
    }
    setFileToolBarVisible(showFileToolBar);

    if (m_showEditToolBarAction != nullptr) {
        m_showEditToolBarAction->setChecked(showEditToolBar);
    }
    setEditToolBarVisible(showEditToolBar);

    if (m_showStatusBarAction != nullptr) {
        m_showStatusBarAction->setChecked(showStatusBar);
    }
    setStatusBarVisible(showStatusBar);

    const QString lastDirectory = settings.value(QStringLiteral("Files/lastDirectory"), QDir::homePath()).toString();
    m_lastDirectory = lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory;

    const QColor previewBackgroundColor = settings.value(QStringLiteral("CircuitPreview/backgroundColor"),
                                                          CircuitOut::defaultBackgroundColor()).value<QColor>();
    if (m_circuitPreview != nullptr && previewBackgroundColor.isValid()) {
        m_circuitPreview->setBackgroundColor(previewBackgroundColor);
    }

    settings.remove(QStringLiteral("CircuitPreview/driverIndex"));
    setCircuitPreviewDriverIndex(KFilterProjectIo::DriverCount, false);
}

void KFilterQt6App::saveSettings() const
{
    QSettings settings;

    settings.setValue(QStringLiteral("MainWindow/geometry"), saveGeometry());
    if (m_mainSplitter != nullptr) {
        settings.setValue(QStringLiteral("MainWindow/splitterState"), m_mainSplitter->saveState());
    }

    settings.setValue(QStringLiteral("MainWindow/showFileToolBar"),
                      m_showFileToolBarAction == nullptr || m_showFileToolBarAction->isChecked());
    settings.setValue(QStringLiteral("MainWindow/showEditToolBar"),
                      m_showEditToolBarAction == nullptr || m_showEditToolBarAction->isChecked());
    settings.setValue(QStringLiteral("MainWindow/showStatusBar"),
                      m_showStatusBarAction == nullptr || m_showStatusBarAction->isChecked());

    settings.setValue(QStringLiteral("Files/lastDirectory"),
                      m_lastDirectory.isEmpty() ? QDir::homePath() : m_lastDirectory);

    settings.remove(QStringLiteral("CircuitPreview/driverIndex"));

    if (m_circuitPreview != nullptr) {
        const QColor previewBackgroundColor = m_circuitPreview->backgroundColor();
        if (previewBackgroundColor == CircuitOut::defaultBackgroundColor()) {
            settings.remove(QStringLiteral("CircuitPreview/backgroundColor"));
        } else {
            settings.setValue(QStringLiteral("CircuitPreview/backgroundColor"), previewBackgroundColor);
        }
    }
}

void KFilterQt6App::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        saveSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

void KFilterQt6App::newFile()
{
    if (!maybeSave()) {
        return;
    }

    m_doc->newDocument();
    statusBar()->showMessage(tr("New document created."), 3000);
    refreshOverview();
}

void KFilterQt6App::openFile()
{
    if (!maybeSave()) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open KFilter Project"),
        dialogStartDirectory(),
        tr("KFilter project files (*.kfp);;All files (*)"));

    if (filePath.isEmpty()) {
        return;
    }

    openDocumentFile(QUrl::fromLocalFile(filePath));
}

bool KFilterQt6App::openDocumentFile(const QUrl &url)
{
    if (!url.isLocalFile()) {
        QMessageBox::warning(this, tr("Open KFilter Project"),
                             tr("Only local project files are currently supported."));
        return false;
    }

    if (!m_doc->openDocument(url)) {
        QMessageBox::warning(this, tr("Open KFilter Project"),
                             tr("The project file could not be opened:\n%1").arg(url.toLocalFile()));
        return false;
    }

    rememberDirectoryForPath(url.toLocalFile());
    statusBar()->showMessage(tr("Opened %1").arg(url.toLocalFile()), 3000);
    refreshOverview();
    return true;
}

bool KFilterQt6App::saveFile()
{
    const QString path = currentLocalPath();
    if (path.isEmpty()) {
        return saveFileAs();
    }

    return saveToUrl(QUrl::fromLocalFile(path));
}

bool KFilterQt6App::saveFileAs()
{
    QString proposedPath = currentLocalPath();
    if (proposedPath.isEmpty()) {
        proposedPath = QDir(dialogStartDirectory()).filePath(QStringLiteral("Untitled.kfp"));
    }

    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save KFilter Project"),
        proposedPath,
        tr("KFilter project files (*.kfp);;All files (*)"));

    if (filePath.isEmpty()) {
        return false;
    }

    filePath = ensureProjectSuffix(filePath);
    return saveToUrl(QUrl::fromLocalFile(filePath));
}

bool KFilterQt6App::saveToUrl(const QUrl &url)
{
    if (!m_doc->saveDocument(url)) {
        QMessageBox::warning(this, tr("Save KFilter Project"),
                             tr("The project file could not be saved:\n%1").arg(url.toLocalFile()));
        return false;
    }

    rememberDirectoryForPath(url.toLocalFile());
    statusBar()->showMessage(tr("Saved %1").arg(url.toLocalFile()), 3000);
    refreshOverview();
    return true;
}

bool KFilterQt6App::maybeSave()
{
    if (!m_doc->isModified()) {
        return true;
    }

    const QMessageBox::StandardButton ret = QMessageBox::question(
        this,
        tr("KFilter"),
        tr("The document '%1' has unsaved changes.\nDo you want to save them?").arg(currentDisplayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (ret == QMessageBox::Save) {
        return saveFile();
    }

    if (ret == QMessageBox::Cancel) {
        return false;
    }

    return true;
}

void KFilterQt6App::rememberDirectoryForPath(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QString directory = info.absolutePath();
    if (!directory.isEmpty()) {
        m_lastDirectory = directory;
    }
}

QString KFilterQt6App::dialogStartDirectory() const
{
    const QString path = currentLocalPath();
    if (!path.isEmpty()) {
        const QFileInfo info(path);
        if (!info.absolutePath().isEmpty()) {
            return info.absolutePath();
        }
    }

    return m_lastDirectory.isEmpty() ? QDir::homePath() : m_lastDirectory;
}

QString KFilterQt6App::currentDisplayName() const
{
    const QUrl url = m_doc->URL();
    if (url.isLocalFile()) {
        return QFileInfo(url.toLocalFile()).fileName();
    }

    const QString value = url.toString();
    return value.isEmpty() ? tr("Untitled") : value;
}

QString KFilterQt6App::currentLocalPath() const
{
    const QUrl url = m_doc->URL();
    if (!url.isLocalFile()) {
        return QString();
    }
    return url.toLocalFile();
}

void KFilterQt6App::refreshOverview()
{
    m_stateLabel->setText(tr("Document: %1%2")
                              .arg(currentDisplayName(), m_doc->isModified() ? tr(" [modified]") : QString()));
    refreshCircuitPreview();
    updateWindowTitle();
    updateActionState();
}

void KFilterQt6App::refreshCircuitPreview()
{
    if (m_circuitPreview == nullptr) {
        return;
    }

    if (m_circuitPreviewDriverIndex == KFilterProjectIo::DriverCount) {
        m_circuitPreview->setDrivers(m_doc->m_driverDriver, KFilterProjectIo::DriverCount);
        return;
    }

    int driverIndex = m_circuitPreviewDriverIndex;
    if (driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount) {
        driverIndex = KFilterProjectIo::DriverCount;
        m_circuitPreviewDriverIndex = KFilterProjectIo::DriverCount;
    }

    if (driverIndex == KFilterProjectIo::DriverCount) {
        m_circuitPreview->setDrivers(m_doc->m_driverDriver, KFilterProjectIo::DriverCount);
        return;
    }

    m_circuitPreview->setDriver(m_doc->m_driverDriver[driverIndex], driverIndex + 1);
}

void KFilterQt6App::setCircuitPreviewDriverIndex(int driverIndex, bool showStatusMessage)
{
    if (driverIndex < 0 || driverIndex > KFilterProjectIo::DriverCount ||
        (driverIndex < KFilterProjectIo::DriverCount && !circuitPreviewDriverSlotAvailable(driverIndex))) {
        driverIndex = KFilterProjectIo::DriverCount;
    }

    m_circuitPreviewDriverIndex = driverIndex;
    updateCircuitPreviewDriverActions();
    refreshCircuitPreview();

    if (!showStatusMessage) {
        return;
    }

    if (driverIndex == KFilterProjectIo::DriverCount) {
        statusBar()->showMessage(tr("Network preview shows all active drivers."), 3000);
    } else {
        statusBar()->showMessage(tr("Network preview shows Driver %1.").arg(driverIndex + 1), 3000);
    }
}

void KFilterQt6App::updateCircuitPreviewDriverActions()
{
    if (m_circuitPreviewAllDriversAction != nullptr) {
        m_circuitPreviewAllDriversAction->setEnabled(true);
        m_circuitPreviewAllDriversAction->setChecked(m_circuitPreviewDriverIndex == KFilterProjectIo::DriverCount);
    }

    for (int index = 0; index < KFilterProjectIo::DriverCount; ++index) {
        QAction *driverAction = m_circuitPreviewDriverActions[index];
        if (driverAction == nullptr) {
            continue;
        }

        const bool available = circuitPreviewDriverSlotAvailable(index);
        driverAction->setText(circuitPreviewDriverMenuText(index));
        driverAction->setEnabled(available);
        driverAction->setChecked(available && m_circuitPreviewDriverIndex == index);
    }
}

bool KFilterQt6App::circuitPreviewDriverSlotAvailable(int driverIndex) const
{
    return m_doc != nullptr && driverIndex >= 0 && driverIndex < KFilterProjectIo::DriverCount;
}

QString KFilterQt6App::circuitPreviewDriverMenuText(int driverIndex) const
{
    const QString fallback = tr("Driver %1").arg(driverIndex + 1);
    if (m_doc == nullptr || driverIndex < 0 || driverIndex >= KFilterProjectIo::DriverCount) {
        return fallback;
    }

    QString title = m_doc->m_driverDriver[driverIndex].GetTitle().trimmed();
    if (title.isEmpty() || title == QStringLiteral("This is a default driver")) {
        return fallback;
    }

    constexpr qsizetype MaximumMenuTitleLength = 60;
    if (title.size() > MaximumMenuTitleLength) {
        title = title.left(MaximumMenuTitleLength - 1) + QStringLiteral("…");
    }

    return tr("Driver %1: %2").arg(driverIndex + 1).arg(title);
}

void KFilterQt6App::updateWindowTitle()
{
    const QString path = currentLocalPath();
    setWindowFilePath(path.isEmpty() ? currentDisplayName() : path);
    setWindowModified(m_doc->isModified());
    setWindowTitle(QStringLiteral("%1[*] - KFilter").arg(currentDisplayName()));
}

void KFilterQt6App::updateActionState()
{
    // Keep Save available: for untitled documents it behaves like Save As.
    m_saveAction->setEnabled(true);
    m_saveAsAction->setEnabled(true);
    updateCircuitPreviewDriverActions();
}

void KFilterQt6App::showAboutDialog()
{
    QMessageBox::about(
        this,
        tr("About KFilter"),
        tr("<b>KFilter</b><br>"
           "Qt6 porting build<br><br>"
           "Core calculation, project file I/O, driver parameters, "
           "network parameters, plot view and schematic preview are currently "
           "available."));
}

void KFilterQt6App::editDriverParameters()
{
    DriverParametersDialog dialog(m_doc->m_driverDriver, this);
    connect(&dialog, &DriverParametersDialog::parametersApplied, this, [this]() {
        m_doc->setModified(true);
        m_doc->viewrefresh();
        statusBar()->showMessage(tr("Driver parameters applied."), 3000);
    });

    dialog.exec();
}

void KFilterQt6App::editNetworkParameters()
{
    NetworkParametersDialog dialog(m_doc->m_driverDriver, this);
    connect(&dialog, &NetworkParametersDialog::parametersApplied, this, [this]() {
        m_doc->setModified(true);
        m_doc->viewrefresh();
        statusBar()->showMessage(tr("Network / filter parameters applied."), 3000);
    });

    dialog.exec();
}
