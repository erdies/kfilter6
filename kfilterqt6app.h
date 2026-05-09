/***************************************************************************
                          kfilterqt6app.h  -  Qt6 application shell
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#ifndef KFILTERQT6APP_H
#define KFILTERQT6APP_H

#include <QMainWindow>
#include <QPoint>
#include <QString>
#include <QUrl>

#include <array>

class QAction;
class QActionGroup;
class CircuitOut;
class driver;
class QLabel;
class QSplitter;
class QToolBar;
class KFilterDoc;
class KFilterView;
class NetworkSectionEditDialog;

/**
 * Qt6 application shell used during the KDE3 -> Qt6/KF6 port.
 *
 * This is still intentionally conservative: it keeps the legacy model and
 * calculation code in place while exposing the ported file, parameter, plot and
 * schematic functionality through regular Qt6 Widgets menus.
 */
class KFilterQt6App : public QMainWindow
{
    Q_OBJECT

public:
    explicit KFilterQt6App(QWidget *parent = nullptr);
    ~KFilterQt6App() override;

    bool openDocumentFile(const QUrl &url);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void refreshOverview();
    void editDriverParameters();
    void editDriverParametersFromPreview(int driverIndex);
    void editNetworkParameters();
    void refreshCircuitPreview();
    void showAboutDialog();
    void resetWindowLayout();
    void chooseCircuitPreviewBackgroundColor();
    void resetCircuitPreviewBackgroundColor();
    void editNetworkSectionFromPreview(int driverIndex, int sectionIndex, int groupValue);
    void showNetworkSectionContextMenuFromPreview(int driverIndex, int sectionIndex, int groupValue, const QPoint& globalPosition);
    void clearNetworkSectionFromPreview(int driverIndex, int sectionIndex, int groupValue);
    void showNetworkSectionHoverFromPreview(int driverIndex, int sectionIndex, int groupValue);
    void showDriverHoverFromPreview(int driverIndex);
    void clearNetworkSectionHoverFromPreview();
    void setFileToolBarVisible(bool visible);
    void setEditToolBarVisible(bool visible);
    void setStatusBarVisible(bool visible);
    void loadSettings();
    void saveSettings() const;

private:
    void createActions();
    void createMenusAndToolBar();
    void updateWindowTitle();
    void updateActionState();
    bool maybeSave();
    bool saveToUrl(const QUrl &url);
    void rememberDirectoryForPath(const QString &filePath);
    QString dialogStartDirectory() const;
    QString currentDisplayName() const;
    QString currentLocalPath() const;
    void setCircuitPreviewDriverIndex(int driverIndex, bool showStatusMessage);
    void updateCircuitPreviewDriverActions();
    bool circuitPreviewDriverSlotAvailable(int driverIndex) const;
    QString circuitPreviewDriverMenuText(int driverIndex) const;
    void openDriverParametersDialog(int initialDriverIndex);
    bool networkSectionEditInProgress() const;
    bool raiseActiveNetworkSectionEditor();

    static constexpr int CircuitPreviewDriverActionCount = 4;

    KFilterDoc *m_doc = nullptr;
    KFilterView *m_plotView = nullptr;
    CircuitOut *m_circuitPreview = nullptr;
    int m_circuitPreviewDriverIndex = CircuitPreviewDriverActionCount;
    int m_lastDriverParametersDriverIndex = 0;
    int m_lastNetworkParametersDriverIndex = 0;
    QSplitter *m_mainSplitter = nullptr;
    QToolBar *m_fileToolBar = nullptr;
    QToolBar *m_editToolBar = nullptr;
    QLabel *m_stateLabel = nullptr;
    QString m_lastDirectory;
    QString m_lastCircuitPreviewHoverStatus;

    QAction *m_newAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_saveAsAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_driverParametersAction = nullptr;
    QAction *m_networkParametersAction = nullptr;
    QAction *m_showFileToolBarAction = nullptr;
    QAction *m_showEditToolBarAction = nullptr;
    QAction *m_showStatusBarAction = nullptr;
    QAction *m_resetLayoutAction = nullptr;
    QActionGroup *m_circuitPreviewDriverActionGroup = nullptr;
    QAction *m_circuitPreviewAllDriversAction = nullptr;
    std::array<QAction *, CircuitPreviewDriverActionCount> m_circuitPreviewDriverActions{};
    QAction *m_circuitPreviewBackgroundColorAction = nullptr;
    QAction *m_resetCircuitPreviewBackgroundColorAction = nullptr;
    QAction *m_aboutAction = nullptr;
    NetworkSectionEditDialog *m_activeNetworkSectionEditDialog = nullptr;
};

#endif // KFILTERQT6APP_H
