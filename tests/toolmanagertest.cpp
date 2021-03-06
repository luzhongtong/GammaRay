/*
  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2015-2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Anton Kreuzkamp <anton.kreuzkamp@kdab.com>

  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
  accordance with GammaRay Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <config-gammaray.h>
#include <common/toolmanagerinterface.h>
#include <ui/clienttoolmanager.h>

#include <probe/hooks.h>
#include <probe/probecreator.h>
#include <core/probe.h>
#include <common/paths.h>
#include <common/objectbroker.h>

#include <3rdparty/qt/modeltest.h>

#include <QtTest/qtest.h>
#include <QtTest/QSignalSpy>

#include <QAbstractItemModel>
#include <QAction>
#include <QObject>
#include <QWidget>

using namespace GammaRay;

Q_DECLARE_METATYPE(QVector<GammaRay::ToolInfo>)

class ToolManagerTest : public QObject
{
    Q_OBJECT
private:
    void createProbe()
    {
        Paths::setRelativeRootPath(GAMMARAY_INVERSE_BIN_DIR);
        qputenv("GAMMARAY_ProbePath", Paths::probePath(GAMMARAY_PROBE_ABI).toUtf8());
        Hooks::installHooks();
        Probe::startupHookReceived();
        new ProbeCreator(ProbeCreator::Create);
        QTest::qWait(1); // event loop re-entry
    }

    int visibleRowCount(QAbstractItemModel *model)
    {
        int count = 0;
        for (int i = 0; i < model->rowCount(); ++i) {
            auto idx = model->index(i, 1);
            if (!idx.data(Qt::DisplayRole).toString().startsWith(QLatin1String("QDesktop")))
                ++count;
        }
        return count;
    }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<QVector<ToolInfo> >();
        new ClientToolManager;
    }

    void init()
    {
        delete Probe::instance();
        createProbe();
    }

    void testProbeSide()
    {
        auto *toolManager = ObjectBroker::object<ToolManagerInterface *>();
        QVERIFY(toolManager);

        QSignalSpy availableToolsSpy(toolManager, &ToolManagerInterface::availableToolsResponse);
        QSignalSpy toolEnabledSpy(toolManager, &ToolManagerInterface::toolEnabled);
        QSignalSpy toolSelectedSpy(toolManager, &ToolManagerInterface::toolSelected);
        QSignalSpy toolsForObjectSpy(toolManager, &ToolManagerInterface::toolsForObjectResponse);

        toolManager->requestAvailableTools();
        availableToolsSpy.wait(500);
        QCOMPARE(availableToolsSpy.size(), 1);
        const auto &list = availableToolsSpy[0][0].value<QVector<ToolData> >();
        QVERIFY(list.size() > 0);

        bool hasBasicTools = false;
        const ToolData *actionInspector = nullptr;
        const ToolData *guiSupport = nullptr;
        foreach (const auto &tool, list) {
            if (tool.id == "GammaRay::ObjectInspector")
                hasBasicTools = true;
            else if (tool.id == "gammaray_actioninspector")
                actionInspector = &tool;
            else if (tool.id == "gammaray_guisupport")
                guiSupport = &tool;
        }
        QVERIFY(hasBasicTools);
        QVERIFY(actionInspector);
        QCOMPARE(actionInspector->enabled, false);
        QCOMPARE(actionInspector->hasUi, true);
        QVERIFY(guiSupport);
        QCOMPARE(guiSupport->enabled, true);
        QCOMPARE(guiSupport->hasUi, false);
        // Create QAction to enable action inspector
        QAction action("Test Action", this);
        toolEnabledSpy.wait(1000);
        QVERIFY(!toolEnabledSpy.isEmpty());
        QStringList enabledTools;
        for (auto i = toolEnabledSpy.constBegin(); i != toolEnabledSpy.constEnd(); ++i)
            enabledTools << i->first().toString();
        QVERIFY(enabledTools.contains("gammaray_actioninspector"));

        toolManager->selectObject(ObjectId(&action), QStringLiteral("gammaray_actioninspector"));
        toolSelectedSpy.wait(50);
        QCOMPARE(toolSelectedSpy.size(), 1);
        QString selectedTool = toolSelectedSpy.first().first().toString();
        QCOMPARE(selectedTool, QStringLiteral("gammaray_actioninspector"));

        toolManager->requestToolsForObject(ObjectId(&action));
        toolsForObjectSpy.wait(50);
        QCOMPARE(toolsForObjectSpy.size(), 1);
        const ObjectId &actionId = toolsForObjectSpy.first().first().value<ObjectId>();
        QCOMPARE(actionId.asQObject(), &action);
        const auto &actionTools = toolsForObjectSpy.first().last().value<QVector<QString> >();
        QStringList supportedToolIds;
        foreach (const auto &tool, actionTools)
            supportedToolIds << tool;
        QVERIFY(supportedToolIds.contains(QStringLiteral("GammaRay::ObjectInspector")));
        QVERIFY(supportedToolIds.contains(QStringLiteral("GammaRay::MetaObjectBrowser")));
        QVERIFY(supportedToolIds.contains(QStringLiteral("gammaray_actioninspector")));
    }

    void testClientSide()
    {
        ClientToolManager::instance()->requestAvailableTools();
        ModelTest modelTest(ClientToolManager::instance()->model());

        // we're testing inprocess, thus tool list should be available instantly.
        QVERIFY(ClientToolManager::instance()->isToolListLoaded());

        testHasBasicTools(false);
        testToolEnabled();
        testToolSelected();
        testRequestToolsForObject();

        testClearance();

        ClientToolManager::instance()->requestAvailableTools(); // "reconnect"
        testHasBasicTools(true);
        testToolSelected();
        testRequestToolsForObject();
    }

private:
    void testHasBasicTools(bool actionInspectorEnabled)
    {
        bool hasBasicTools = false;
        const ToolInfo *actionInspector = nullptr;
        const ToolInfo *guiSupport = nullptr;
        foreach (const ToolInfo &tool, ClientToolManager::instance()->tools()) {
            if (tool.id() == "GammaRay::ObjectInspector")
                hasBasicTools = true;
            else if (tool.id() == "gammaray_actioninspector")
                actionInspector = &tool;
            else if (tool.id() == "gammaray_guisupport")
                guiSupport = &tool;
        }
        QVERIFY(hasBasicTools);
        QVERIFY(actionInspector);
        QCOMPARE(actionInspector->isEnabled(), actionInspectorEnabled);
        QCOMPARE(actionInspector->hasUi(), true);
        QVERIFY(!guiSupport); // tools without ui are supposed to be filtered out
        QVERIFY(!ClientToolManager::instance()->widgetForId("inexistantTool"));
        QVERIFY(actionInspectorEnabled == (bool)ClientToolManager::instance()->widgetForId("gammaray_actioninspector")); // if tool is disabled we explicitly want widgetForId to be null.
    }

    void testToolEnabled()
    {
        QSignalSpy toolEnabledSpy(ClientToolManager::instance(), &ClientToolManager::toolEnabled);
        // Create QAction to enable action inspector
        QAction action("Test Action", this);
        toolEnabledSpy.wait(50);
        QVERIFY(toolEnabledSpy.size() >= 1);
        QStringList enabledTools;
        for (auto i = toolEnabledSpy.constBegin(); i != toolEnabledSpy.constEnd(); ++i)
            enabledTools << i->first().toString();
        QVERIFY(enabledTools.contains("gammaray_actioninspector"));

        QVERIFY(ClientToolManager::instance()->widgetForId("gammaray_actioninspector"));
    }

    void testToolSelected()
    {
        auto *toolManager = ObjectBroker::object<ToolManagerInterface *>();
        QVERIFY(toolManager);

        QSignalSpy toolSelectedSpy(ClientToolManager::instance(), &ClientToolManager::toolSelected);
        QAction action("Test Action", this);

        toolManager->selectObject(ObjectId(&action), QStringLiteral("gammaray_actioninspector"));
        toolSelectedSpy.wait(50);
        QCOMPARE(toolSelectedSpy.size(), 1);
        QString selectedTool = toolSelectedSpy.first().first().toString();
        QCOMPARE(selectedTool, QStringLiteral("gammaray_actioninspector"));
    }

    void testRequestToolsForObject()
    {
        QSignalSpy toolsForObjectSpy(ClientToolManager::instance(), &ClientToolManager::toolsForObjectResponse);
        QAction action("Test Action", this);

        ClientToolManager::instance()->requestToolsForObject(ObjectId(&action));
        toolsForObjectSpy.wait(50);
        QCOMPARE(toolsForObjectSpy.size(), 1);
        const ObjectId &actionId = toolsForObjectSpy.first().first().value<ObjectId>();
        QCOMPARE(actionId.asQObject(), &action);
        const auto &actionTools = toolsForObjectSpy.first().last().value<QVector<ToolInfo> >();
        QStringList supportedToolIds;
        foreach (const auto &tool, actionTools) {
            QVERIFY(!tool.name().isEmpty());
            QVERIFY(tool.name() != tool.id());
            supportedToolIds << tool.id();
        }
        QVERIFY(supportedToolIds.contains(QStringLiteral("GammaRay::ObjectInspector")));
        QVERIFY(supportedToolIds.contains(QStringLiteral("GammaRay::MetaObjectBrowser")));
        QVERIFY(supportedToolIds.contains(QStringLiteral("gammaray_actioninspector")));
    }

    void testClearance()
    {
        QSignalSpy resetSpy(ClientToolManager::instance()->model(), &QAbstractItemModel::modelReset);
        ClientToolManager::instance()->clear();
        QVERIFY(ClientToolManager::instance());
        QCOMPARE(ClientToolManager::instance()->tools().size(), 0);
        resetSpy.wait(50);
        QCOMPARE(resetSpy.size(), 1);
    }
};

QTEST_MAIN(ToolManagerTest)

#include "toolmanagertest.moc"
