#include "laycaneditorplugin.h"
#include "laycaneditorconstants.h"

#include <coreplugin/icore.h>
#include <coreplugin/icontext.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/coreconstants.h>

#include <QAction>
#include <QMessageBox>
#include <QMainWindow>
#include <QMenu>

#include <QtPlugin>

using namespace LaycanEditor::Internal;

LaycanEditorPlugin::LaycanEditorPlugin()
{
    // Create your members
}

LaycanEditorPlugin::~LaycanEditorPlugin()
{
    // Unregister objects from the plugin manager's object pool
    // Delete members
    //delete view;
}

bool LaycanEditorPlugin::initialize(const QStringList &arguments, QString *errorString)
{
    // Register objects in the plugin manager's object pool
    // Load settings
    // Add actions to menus
    // Connect to other plugins' signals
    // In the initialize function, a plugin can be sure that the plugins it
    // depends on have initialized their members.

    Q_UNUSED(arguments)
    Q_UNUSED(errorString)

    QAction *action = new QAction(tr("LaycanEditor action"), this);
    Core::Command *cmd = Core::ActionManager::registerAction(action, Constants::ACTION_ID,
                                                             Core::Context(Core::Constants::C_GLOBAL));
    cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Alt+L")));
    connect(action, SIGNAL(triggered()), this, SLOT(callWindow()));

    Core::ActionContainer *menu = Core::ActionManager::createMenu(Constants::MENU_ID);
    menu->menu()->setTitle(tr("Laycan Editor"));
    menu->addAction(cmd);
    Core::ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(menu);

    return true;
}

void LaycanEditorPlugin::extensionsInitialized()
{
    // Retrieve objects from the plugin manager's object pool
    // In the extensionsInitialized function, a plugin can be sure that all
    // plugins that depend on it are completely initialized.
}

ExtensionSystem::IPlugin::ShutdownFlag LaycanEditorPlugin::aboutToShutdown()
{
    // Save settings
    // Disconnect from signals that are not needed during shutdown
    // Hide UI (if you add UI that is not in the main window directly)
    return SynchronousShutdown;
}

void LaycanEditorPlugin::callWindow()
{
    view = new LaycanEditorView;
    view->setWindowFlags(
        Qt::WindowTitleHint |
        Qt::CustomizeWindowHint |
        Qt::WindowCloseButtonHint |
        Qt::WindowMinimizeButtonHint);
    view->setAttribute(Qt::WA_DeleteOnClose);
    view->show();
}
