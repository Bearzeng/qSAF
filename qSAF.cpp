//##########################################################################
//#                                                                        #
//#                       CLOUDCOMPARE PLUGIN: qSAF                        #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#                             COPYRIGHT: XXX                             #
//#                                                                        #
//##########################################################################

#include "qSAF.h"

//Qt
#include <QtGui>
#include <QApplication>
#include <QProgressDialog>
#include <QMainWindow>
#include <QElapsedTimer>

//Dialog
#include "ccSAFDlg.h"

//System
#include <iostream>
#include <vector>
#include <string>
#include <assert.h>

//qCC_db
#include <ccGenericPointCloud.h>
#include <ccPointCloud.h>
#include <ccProgressDialog.h>

//CC
#include <ScalarField.h>

qSAF::qSAF(QObject* parent/*=0*/)
    : QObject(parent)
    , ccStdPluginInterface( ":/CC/plugin/qSAF/info.json" )
    , m_action(nullptr)
{
}

void qSAF::onNewSelection(const ccHObject::Container& selectedEntities)
{
    if (m_action)
        m_action->setEnabled(selectedEntities.size()==1 && selectedEntities[0]->isA(CC_TYPES::POINT_CLOUD));
}

QList<QAction *> qSAF::getActions()
{
    if (!m_action)
    {
        m_action = new QAction(getName(),this);
        m_action->setToolTip(getDescription());
        m_action->setIcon(getIcon());
        //connect appropriate signal
        connect(m_action, SIGNAL(triggered()), this, SLOT(doAction()));
    }

    return {m_action};
}

void qSAF::doAction()
{
    //m_app should have already been initialized by CC when plugin is loaded!
    //(--> pure internal check)
    assert(m_app);
    if (!m_app)
        return;

    const ccHObject::Container& selectedEntities = m_app->getSelectedEntities();
    size_t selNum = selectedEntities.size();
    if (selNum != 1)
    {
        m_app->dispToConsole("[SAF] Select only one cloud!", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
        return;
    }

    ccHObject* ent = selectedEntities[0];
    assert(ent);
    if (!ent || !ent->isA(CC_TYPES::POINT_CLOUD))
    {
        m_app->dispToConsole("[SAF] Select a real point cloud!", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
        return;
    }

    //to get the point cloud from selected entity.
    ccPointCloud* pc = static_cast<ccPointCloud*>(ent);

    //Number of points
    unsigned count = pc->size();

    //initial dialog parameters
    static double threshold_1 = 20;
    static double threshold_2 = 70;
    double threshold_temp = 0;

    //display the dialog
    {
        ccSAFDlg safDlg(m_app->getMainWindow());
        safDlg.doubleSpinBox_1->setValue(threshold_1);
        safDlg.doubleSpinBox_2->setValue(threshold_2);

        if(!safDlg.exec())
        {
            return;
        }

        //save the parameters for next time
        threshold_1 = safDlg.doubleSpinBox_1->value();
        threshold_2 = safDlg.doubleSpinBox_2->value();
    }

    //display the progress dialog
    QProgressDialog pDlg;
    pDlg.setWindowTitle("SAF");
    pDlg.setLabelText(QString("Scan Angle Filter\nfrom %1 to %2").arg(threshold_1).arg(threshold_2));
    pDlg.setRange(0, count);
    pDlg.setCancelButton(0);
    pDlg.show();
    QApplication::processEvents();

    QElapsedTimer timer;
    timer.start();

    ScalarType scanAngle;

    CCCoreLib::ReferenceCloud rangeAnglerc(pc);

    //To ensure that threshold_1 is less than threshold_2
    if(threshold_1 > threshold_2)
    {
        threshold_temp = threshold_1;
        threshold_1 = threshold_2;
        threshold_2 = threshold_temp;
    }

    //[进度条]进度条的取消SAF按钮
    bool wasCancelled = false;

    //Index of Scan Angle Rank
    int scanAngleSFIndex = pc->getScalarFieldIndexByName("ScanAngleRank");
    if (scanAngleSFIndex < 0) {
        m_app->dispToConsole("[SAF] Failed to find \"ScanAngleRank\" on the selected entity.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
        return;
    }

    for(unsigned i = 0; i < count; ++i)
    {
        //Get the scan angle for each point
        scanAngle = pc->getScalarField(scanAngleSFIndex)->getValue(i);

        //absolute
        if(scanAngle < 0)
        {
            scanAngle = -scanAngle;
        }

        //Gets the index of the point within the range
        if(threshold_1 <= scanAngle && scanAngle <= threshold_2)
        {
            rangeAnglerc.addPointIndex(i);
        }

        //[进度条]重置进度条
        pDlg.setValue(i);
        QCoreApplication::processEvents();

        //[进度条]取消SAF处理
        if (pDlg.wasCanceled())
        {
            wasCancelled = true;
            break;
        }
    }

    //partialClone ReferenceCloud to ccPointCloud
    ccPointCloud* rangeAnglepc = pc->partialClone(&rangeAnglerc);

    //Whether the rangeAnglepc is null
    if(!rangeAnglepc)
    {
        m_app->dispToConsole("[SAF] Failed to extract the range angle subset.", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
        return;
    }

    //Count the percentage of filtered and spend time
    m_app->dispToConsole(QString("[SAF] %1% of scan angle points are filtered").arg((rangeAnglerc.size() * 100.0) / count, 0, 'f', 2), ccMainAppInterface::STD_CONSOLE_MESSAGE);
    m_app->dispToConsole(QString("[SAF] Timing: %1 s.").arg(timer.elapsed() / 1000.0, 0, 'f', 1), ccMainAppInterface::STD_CONSOLE_MESSAGE);

    //close the process dialog
    pDlg.close();
    QApplication::processEvents();

    //[进度条]取消SAF
    if (wasCancelled)
    {
        m_app->dispToConsole("[SAF] SAF was cancelled", ccMainAppInterface::STD_CONSOLE_MESSAGE);
        return;
    }

    //hide the original cloud
    pc->setEnabled(false);

    //we add new group to DB/display
    ccHObject* cloudContainer = new ccHObject(pc->getName() + QString("_saf"));

    //rangeAnglepc
    rangeAnglepc->setVisible(true);
    rangeAnglepc->setName("SAF Point Cloud");
    cloudContainer->addChild(rangeAnglepc);

    //add cloudContainer To DBTree
    m_app->addToDB(cloudContainer);

    //refresh
    m_app->refreshAll();

}
