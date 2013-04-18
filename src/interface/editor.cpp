/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation;

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include <QtGui>
#include <QBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QTableWidget>
#include <QListWidget>
#include <QMainWindow>
#include <QTimer>
#include <QSvgGenerator>

#include "editor.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layersound.h"
#include "layercamera.h"
#include "mainwindow.h"

#define MIN(a,b) ((a)>(b)?(b):(a))

Editor::Editor(QMainWindow* parent)
{
    mainWindow = parent;

    QSettings settings("Pencil","Pencil");

    object = NULL; // the editor is initialized with no object
    savedName = "";
    altpress=false;
    modified=false;
    numberOfModifications = 0;
    autosave=settings.value("autosave").toBool();
    autosaveNumber=settings.value("autosaveNumber").toInt();
    if (autosaveNumber==0) { autosaveNumber=20; settings.setValue("autosaveNumber", 20); }
    backupIndex = -1;
    clipboardBitmapOk = false;
    clipboardVectorOk = false;

    if (settings.value("onionLayer1Opacity").isNull()) settings.setValue("onionLayer1Opacity",50);
    if (settings.value("onionLayer2Opacity").isNull()) settings.setValue("onionLayer2Opacity",0);
    if (settings.value("onionLayer3Opacity").isNull()) settings.setValue("onionLayer3Opacity",0);
    onionLayer1Opacity = settings.value("onionLayer1Opacity").toInt();
    onionLayer2Opacity = settings.value("onionLayer2Opacity").toInt();
    onionLayer3Opacity = settings.value("onionLayer3Opacity").toInt();

    fps = settings.value("fps").toInt();
    if (fps==0) { fps=12; settings.setValue("fps", 12); }
    maxFrame = 1;
    timer = new QTimer(this);
    timer->setInterval(1000/fps);
    connect(timer, SIGNAL(timeout()), this, SLOT(playNextFrame()));
    playing = false;
    looping = false;
    sound = true;

    frameList << 1;
    currentFrame = 1;
    currentLayer = 0;

    QHBoxLayout* lay = new QHBoxLayout();
    QVBoxLayout* framelay = new QVBoxLayout();

    scribbleArea = new ScribbleArea(this, this);
    timeLine = new TimeLine(this, this);
    timeControl = new TimeControls();
    toolSet = new ToolSet();
    palette = new Palette(this);
    preferences = new Preferences();
    exportFramesDialog = NULL; // will be created when needed
    exportMovieDialog = NULL;
    exportFlashDialog = NULL;
    newDocumentDialog = NULL;
    exportFramesDialog_hBox = NULL;
    exportFramesDialog_vBox = NULL;
    exportFramesDialog_format = NULL;
    exportMovieDialog_hBox = NULL;
    exportMovieDialog_vBox = NULL;
    exportMovieDialog_format = NULL;
    exportMovieDialog_fpsBox = NULL;

    exportFlashDialog_compression = NULL;

    // FOCUS POLICY
    scribbleArea->setFocusPolicy(Qt::StrongFocus);
    timeLine->setFocusPolicy(Qt::NoFocus);
    toolSet->setFocusPolicy(Qt::NoFocus);
    palette->setFocusPolicy(Qt::NoFocus);


    // CONNECTIONS
    connect(toolSet, SIGNAL(addClick()), this, SLOT(addKey()));
    connect(toolSet, SIGNAL(rmClick()), this, SLOT(removeKey()));
    connect(toolSet, SIGNAL(playClick()), this, SLOT(play()));
    connect(toolSet, SIGNAL(fpsClick(int)), this, SLOT(changeFps(int)));

    connect(toolSet, SIGNAL(pencilClick()), scribbleArea, SLOT(pencilOn()));
    connect(toolSet, SIGNAL(eraserClick()), scribbleArea, SLOT(eraserOn()));
    connect(toolSet, SIGNAL(selectClick()), scribbleArea, SLOT(selectOn()));
    connect(toolSet, SIGNAL(moveClick()), scribbleArea, SLOT(moveOn()));
    connect(toolSet, SIGNAL(handClick()), scribbleArea, SLOT(handOn()));
    connect(toolSet, SIGNAL(penClick()), scribbleArea, SLOT(penOn()));
    connect(toolSet, SIGNAL(polylineClick()), scribbleArea, SLOT(polylineOn()));
    connect(toolSet, SIGNAL(bucketClick()), scribbleArea, SLOT(bucketOn()));
    connect(toolSet, SIGNAL(eyedropperClick()), scribbleArea, SLOT(eyedropperOn()));
    connect(toolSet, SIGNAL(colouringClick()), scribbleArea, SLOT(colouringOn()));
    connect(toolSet, SIGNAL(smudgeClick()), scribbleArea, SLOT(smudgeOn()));

    connect(toolSet, SIGNAL(pressureClick(int)), this, SLOT(applyPressure(int)));
    connect(toolSet, SIGNAL(invisibleClick(int)), this, SLOT(applyInvisibility(int)));
    connect(toolSet, SIGNAL(preserveAlphaClick(int)), this, SLOT(applyPreserveAlpha(int)));
    connect(toolSet, SIGNAL(followContourClick(int)), this, SLOT(applyFollowContour(int)));

    connect(toolSet, SIGNAL(widthClick(qreal)), this, SLOT(applyWidth(qreal)));
    connect(toolSet, SIGNAL(featherClick(qreal)), this, SLOT(applyFeather(qreal)));
    connect(toolSet, SIGNAL(opacityClick(qreal)), this, SLOT(applyOpacity(qreal)));

    connect(toolSet, SIGNAL(colourClick()), this, SLOT(showPalette()));

    connect(toolSet,SIGNAL(clearClick()), scribbleArea, SLOT(clearImage()));

    connect(toolSet, SIGNAL(thinLinesClick()), scribbleArea, SLOT(toggleThinLines()));
    connect(toolSet, SIGNAL(outlinesClick()), scribbleArea, SLOT(toggleOutlines()));
    connect(scribbleArea, SIGNAL(thinLinesChanged(bool)), toolSet, SLOT(changeThinLinesButton(bool)));
    connect(scribbleArea, SIGNAL(outlinesChanged(bool)), toolSet, SLOT(changeOutlinesButton(bool)));
    connect(toolSet, SIGNAL(mirrorClick()), this, SLOT(toggleMirror()));
    connect(toolSet, SIGNAL(mirrorVClick()), this, SLOT(toggleMirrorV()));
    connect(toolSet, SIGNAL(togglePrev(bool)), scribbleArea, SLOT(toggleOnionPrev(bool)));
    connect(toolSet, SIGNAL(toggleNext(bool)), scribbleArea, SLOT(toggleOnionNext(bool)));
    connect(this, SIGNAL(toggleOnionPrev(bool)), scribbleArea, SLOT(toggleOnionPrev(bool)));
    connect(this, SIGNAL(toggleOnionNext(bool)), scribbleArea, SLOT(toggleOnionNext(bool)));
    connect(scribbleArea, SIGNAL(onionPrevChanged(bool)), this, SIGNAL(onionPrevChanged(bool)));
    connect(scribbleArea, SIGNAL(onionNextChanged(bool)), this, SIGNAL(onionNextChanged(bool)));
    connect(scribbleArea, SIGNAL(onionPrevChanged(bool)), toolSet, SLOT(onionPrevChanged(bool)));
    connect(scribbleArea, SIGNAL(onionNextChanged(bool)), toolSet, SLOT(onionNextChanged(bool)));
    //connect(timeLine, SIGNAL(onionPrevClick()), scribbleArea, SLOT(onionPrevSlot()));
    //connect(timeLine, SIGNAL(onionNextClick()), scribbleArea, SLOT(onionNextSlot()));

///////TODO connect timeline signals to editor
    connect(timeLine, SIGNAL(endplayClick()), this, SLOT(endPlay()));
    connect(timeLine, SIGNAL(startplayClick()), this, SLOT(startPlay()));
    connect(timeLine, SIGNAL(duplicateKeyClick()), this, SLOT(duplicateKey()));
///////
    connect(this, SIGNAL(selectAll()), scribbleArea, SLOT(selectAll()));

    connect(scribbleArea, SIGNAL(modification()), this, SLOT(modification()));
    connect(scribbleArea, SIGNAL(modification(int)), this, SLOT(modification(int)));
    connect(timeLine, SIGNAL(modification()), this, SLOT(modification()));

    connect(timeLine, SIGNAL(addKeyClick()), this, SLOT(addKey()));
    connect(timeLine, SIGNAL(removeKeyClick()), this, SLOT(removeKey()));

    connect(timeLine, SIGNAL(newBitmapLayer()), this, SLOT(newBitmapLayer()));
    connect(timeLine, SIGNAL(newVectorLayer()), this, SLOT(newVectorLayer()));
    connect(timeLine, SIGNAL(newSoundLayer()), this, SLOT(newSoundLayer()));
    connect(timeLine, SIGNAL(newCameraLayer()), this, SLOT(newCameraLayer()));
    connect(timeLine, SIGNAL(deleteCurrentLayer()), this, SLOT(deleteCurrentLayer()));

    connect(timeLine, SIGNAL(playClick()), this, SLOT(play()));
    connect(timeLine, SIGNAL(loopClick(bool)), this, SLOT(setLoop(bool)));
    connect(timeLine, SIGNAL(soundClick()), this, SLOT(setSound()));
    connect(timeLine, SIGNAL(fpsClick(int)), this, SLOT(changeFps(int)));

    connect(this, SIGNAL(toggleLoop(bool)), timeLine, SIGNAL(toggleLoop(bool)));
    connect(timeLine, SIGNAL(loopClick(bool)), this, SIGNAL(loopToggled(bool)));

    connect(preferences, SIGNAL(windowOpacityChange(int)), mainWindow, SLOT(setOpacity(int)));
    connect(preferences, SIGNAL(curveOpacityChange(int)), scribbleArea, SLOT(setCurveOpacity(int)));
    connect(preferences, SIGNAL(curveSmoothingChange(int)), scribbleArea, SLOT(setCurveSmoothing(int)));
    connect(preferences, SIGNAL(highResPositionChange(int)), scribbleArea, SLOT(setHighResPosition(int)));
    connect(preferences, SIGNAL(antialiasingChange(int)), scribbleArea, SLOT(setAntialiasing(int)));
    connect(preferences, SIGNAL(gradientsChange(int)), scribbleArea, SLOT(setGradients(int)));
    connect(preferences, SIGNAL(backgroundChange(int)), scribbleArea, SLOT(setBackground(int)));
    connect(preferences, SIGNAL(shadowsChange(int)), scribbleArea, SLOT(setShadows(int)));
    connect(preferences, SIGNAL(toolCursorsChange(int)), scribbleArea, SLOT(setToolCursors(int)));
    connect(preferences, SIGNAL(styleChange(int)), scribbleArea, SLOT(setStyle(int)));

    connect(preferences, SIGNAL(autosaveChange(int)), this, SLOT(changeAutosave(int)));
    connect(preferences, SIGNAL(autosaveNumberChange(int)), this, SLOT(changeAutosaveNumber(int)));

    connect(preferences, SIGNAL(lengthSizeChange(QString)), timeLine, SIGNAL(lengthChange(QString)));
    connect(preferences, SIGNAL(fontSizeChange(int)), timeLine, SIGNAL(fontSizeChange(int)));
    connect(preferences, SIGNAL(frameSizeChange(int)), timeLine, SIGNAL(frameSizeChange(int)));
    connect(preferences, SIGNAL(labelChange(int)), timeLine, SIGNAL(labelChange(int)));
    connect(preferences, SIGNAL(scrubChange(int)), timeLine, SIGNAL(scrubChange(int)));

    connect(preferences, SIGNAL(onionLayer1OpacityChange(int)), this, SLOT(onionLayer1OpacityChangeSlot(int)));
    connect(preferences, SIGNAL(onionLayer2OpacityChange(int)), this, SLOT(onionLayer2OpacityChangeSlot(int)));
    connect(preferences, SIGNAL(onionLayer3OpacityChange(int)), this, SLOT(onionLayer3OpacityChangeSlot(int)));

    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(clipboardChanged()) );

    framelay->addWidget(scribbleArea);
    lay->addWidget(toolSet);
    lay->addLayout(framelay);

    framelay->setMargin(0);
    framelay->setSpacing(0);
    lay->setMargin(0);
    lay->setSpacing(0);

    setLayout(lay);

    qDebug() << QLibraryInfo::location(QLibraryInfo::PluginsPath);
    qDebug() << QLibraryInfo::location(QLibraryInfo::BinariesPath);
    qDebug() << QLibraryInfo::location(QLibraryInfo::LibrariesPath);

    setAcceptDrops(true);
}

Editor::~Editor()
{
    // a lot more probably needs to be cleaned here...
    if (object) delete object;
    clearBackup();
}

void Editor::dragEnterEvent(QDragEnterEvent* event)
{
    //if (event->mimeData()->hasFormat("text/plain"))
    event->acceptProposedAction();
}
/*const QStringList files =
    QFileDialog::getOpenFileNames(filter, startDirectory, parent,
                                  "FileBrowser", title);*/

void Editor::dropEvent(QDropEvent* event)
{

    if( event->mimeData()->hasUrls() )
    {
        for(int i=0; i < event->mimeData()->urls().size(); i++)
        {
            if(i>0) scrubForward();
            QUrl url = event->mimeData()->urls()[i];
            QString filePath = url.toLocalFile();
            if(filePath.endsWith(".png") || filePath.endsWith(".jpg") || filePath.endsWith(".jpeg"))
                importImage( filePath );
            if(filePath.endsWith(".aif") || filePath.endsWith(".mp3") || filePath.endsWith(".wav"))
                importSound( filePath );
        }
    }
}

void Editor::importImageSequence ()
{
    QFileDialog w;
    w.setFileMode(QFileDialog::AnyFile);
    /*	    QListView *l = w.findChild<QListView*>("listView");
    	    if (l) {
    	         l->setSelectionMode(QAbstractItemView::MultiSelection);
    	     }
    	    QTreeView *t = w.findChild<QTreeView*>();
    	     if (t) {
    	       t->setSelectionMode(QAbstractItemView::MultiSelection);
    	    } */

    QSettings settings("Pencil","Pencil");
    QString initialPath = settings.value("lastImportPath", QVariant(QDir::homePath())).toString();
    if(initialPath.isEmpty()) initialPath = QDir::homePath();
    QStringList files = w.getOpenFileNames(
                            this,
                            "Select one or more files to open",
                            initialPath,
                            "Images (*.png *.xpm *.jpg)");
    qDebug() << files;
//#	    w.exec();

    QStringListIterator i(files);
    for (int i = 0; i < files.size(); ++i)
    {
        QString filePath;
        filePath= files.at(i).toLocal8Bit().constData();
        if(i>0) scrubForward();
        {
            QSettings settings("Pencil","Pencil");
            if(filePath.endsWith(".png") || filePath.endsWith(".jpg") || filePath.endsWith(".jpeg"))
                importImage(filePath);
        }

    }


}

bool Editor::importMov()
{
    QSettings settings("Pencil","Pencil");
    QString initialPath = settings.value("lastExportPath", QVariant(QDir::homePath())).toString();
    if(initialPath.isEmpty()) initialPath = QDir::homePath() + "/untitled.avi";
    QString filePath=QFileDialog::getOpenFileName(this, tr("Import movie"),initialPath ,tr("AVI (*.avi);;MPEG(*.mpg);;MOV(*.mov);;MP4(*.mp4);;SWF(*.swf);;FLV(*.flv);;WMV(*.wmv)"));
    if (filePath.isEmpty())
    {
        return false;
    }
    else
    {
        settings.setValue("lastExportPath", QVariant(filePath));
        importMovie(filePath,fps);
        return true;
    }
}



void Editor::showCounter(int n)
{
    toolSet->setCounter(n);
}

void Editor::newDocument()
{
    if (maybeSave())
    {
        //if (!exportFramesDialog) createNewDocumentDialog();
        //newDocumentDialog->exec();
        //if(newDocumentDialog->result() == QDialog::Rejected) return;
        //QSize documentSize = QSize(newDocumentDialog_hBox->value(), newDocumentDialog_vBox->value());
        //newObject(documentSize);
        newObject(); // default size
    }
}

void Editor::openDocument()
{
    if (maybeSave())
    {
        QSettings settings("Pencil","Pencil");
        QString myPath = settings.value("lastFilePath", QVariant(QDir::homePath())).toString();
//		QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), myPath);
        QString fileName = QFileDialog::getOpenFileName(this, tr("Open File..."),myPath ,tr("PCL (*.pcl);;Any files (*)"));
        if (!fileName.isEmpty())
        {
            bool ok = openObject(fileName);
            if(!ok)
            {
                QMessageBox::warning(this, "Warning", "Pencil cannot read this file. If you want to import images, use the command import.");
                newObject();
            }
            else
            {
                updateMaxFrame();
            }
        }
    }
}

void Editor::openRecent()
{
    QSettings settings("Pencil","Pencil");
    QString myPath = settings.value("lastFilePath", QVariant(QDir::homePath())).toString();
    bool ok = openObject(myPath);
    if(!ok)
    {
        QMessageBox::warning(this, "Warning", "Pencil cannot read this file. If you want to import images, use the command import.");
        newObject();
    }
    else
    {
        updateMaxFrame();
    }
}

bool Editor::saveDocument()
{
    //QAction *action = qobject_cast<QAction *>(sender()); // ? old code from Patrick?
    //QByteArray fileFormat = action->data().toByteArray();  // ? old code from Patrick?
    QSettings settings("Pencil","Pencil");
    QString myPath = settings.value("lastFilePath", QVariant(QDir::homePath())).toString();
    if(myPath.isEmpty()) myPath = QDir::homePath() + "/untitled.pcl";

//	QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"), myPath);
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As..."),myPath ,tr("PCL (*.pcl)"));

    if (fileName.isEmpty())
    {
        return false;
    }
    else
    {
        if(! fileName.endsWith(".pcl"))
            fileName =  fileName + ".pcl";
        QSettings settings("Pencil","Pencil");
        settings.setValue("lastFilePath", QVariant(fileName));
        return saveObject(fileName);
    }
}

void Editor::setWidth(qreal width)
{
    scribbleArea->setWidth(width);
    toolSet->setWidth(width);
}

void Editor::applyWidth(qreal width)
{
    setWidth(width);
    Layer* layer = getCurrentLayer();
    if(layer == NULL) return;
    if(layer->type == Layer::VECTOR) ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0)->applyWidthToSelection(width);
}

void Editor::setFeather(qreal feather)
{
    scribbleArea->setFeather(feather);
    toolSet->setFeather(feather);
}

void Editor::applyFeather(qreal feather)
{
    setFeather(feather);
    Layer* layer = getCurrentLayer();
    if(layer == NULL) return;
    if(layer->type == Layer::VECTOR) ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0)->applyFeatherToSelection(feather);
}

void Editor::setOpacity(qreal opacity)
{
    scribbleArea->setOpacity(opacity);
    toolSet->setOpacity(opacity);
}

void Editor::applyOpacity(qreal opacity)
{
    setOpacity(opacity);
    Layer* layer = getCurrentLayer();
    if(layer == NULL) return;
    if(layer->type == Layer::VECTOR) ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0)->applyOpacityToSelection(opacity);
}

void Editor::setInvisibility(int invisibility)
{
    if(invisibility>=0) scribbleArea->setInvisibility(invisibility>0);
    toolSet->setInvisibility(invisibility);
}

void Editor::applyInvisibility(int invisibility)
{
    setInvisibility(invisibility);
    Layer* layer = getCurrentLayer();
    if(layer == NULL) return;
    if(layer->type == Layer::VECTOR) ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0)->applyInvisibilityToSelection(invisibility>0);
}

void Editor::setPreserveAlpha(int preserveAlpha)
{
    if(preserveAlpha>=0) scribbleArea->setPreserveAlpha(preserveAlpha>0);
    toolSet->setPreserveAlpha(preserveAlpha);
}

void Editor::applyPreserveAlpha(int preserveAlpha)
{
    setPreserveAlpha(preserveAlpha);
}

void Editor::setFollowContour(int followContour)
{
    if(followContour>=0) scribbleArea->setFollowContour(followContour>0);
    toolSet->setFollowContour(followContour);
}

void Editor::applyFollowContour(int followContour)
{
    setFollowContour(followContour);
}

void Editor::setPressure(int pressure)
{
    if(pressure>=0) scribbleArea->setPressure(pressure>0);
    toolSet->setPressure(pressure);
}

void Editor::applyPressure(int pressure)
{
    setPressure(pressure);
    Layer* layer = getCurrentLayer();
    if(layer == NULL) return;
    if(layer->type == Layer::VECTOR) ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0)->applyVariableWidthToSelection(pressure>0);
}

void Editor::selectColour(int i)
{
    if(i > -1)
    {
        scribbleArea->setColour(i);
        toolSet->setColour(object->getColour(i).colour);
        palette->selectColour(i);
        palette->setColour(object->getColour(i).colour);
    }
}

void Editor::selectAndApplyColour(int i)
{
    selectColour(i);
    Layer* layer = getCurrentLayer();
    if(layer == NULL) return;
    if(layer->type == Layer::VECTOR) ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0)->applyColourToSelection(i);
}

void Editor::setColour(QColor colour)
{
    scribbleArea->setColour(colour);
    toolSet->setColour(colour);
    palette->setColour(colour);
}

void Editor::changeColour(int i)
{
    //QColor newColour = QColorDialog::getColor(object->getColour(i).colour);
    if(i>-1)
    {
        bool* ok;
        ok = new bool;
        QRgb qrgba = QColorDialog::getRgba( object->getColour(i).colour.rgba(), ok, this );
        if(*ok)
        {
            changeColour(i, QColor::fromRgba(qrgba) );
        }
        delete ok;
    }
}

void Editor::changeColourName(int i)
{
    if (i>-1)
    {
        bool ok;
        QString text = QInputDialog::getText(this, tr("Colour name"),
                                             tr("Colour name:"), QLineEdit::Normal,
                                             object->getColour(i).name, &ok);
        if (ok && !text.isEmpty())
        {
            object->renameColour(i, text);
            palette->updateList();
        }
    }
}

void Editor::changeColour(int i, QColor newColour)
{
    if (newColour.isValid() && i>-1)
    {
        /*object->setColour(i, newColour);
        Layer* layer = object->getLayer(currentLayer);
        if(layer != NULL) {
        	if(layer->type == Layer::VECTOR) scribbleArea->setModified(layer, currentFrame);
        }*/
        updateColour(i, newColour);
        palette->updateList();
        selectColour(i);
    }
}

void Editor::updateColour(int i, QColor newColour)
{
    if( newColour.isValid() && i>-1)
    {
        object->setColour(i, newColour);
        Layer* layer = object->getLayer(currentLayer);
        if(layer != NULL)
        {
            if(layer->type == Layer::VECTOR) scribbleArea->setModified(currentLayer, currentFrame);
        }
        toolSet->setColour(object->getColour(i).colour);
        palette->updateSwatch(object->getColour(i).colour);
        scribbleArea->setColour(i);
    }
}

void Editor::addColour()
{
    QColor initialColour = Qt::white;
    int currentColourIndex = palette->currentColour();
    if( currentColourIndex > -1 )
    {
        initialColour = object->getColour(currentColourIndex).colour;
    }
    bool* ok;
    ok = new bool;
    QRgb qrgba = QColorDialog::getRgba( initialColour.rgba(), ok, this );
    if(*ok)
    {
        object->addColour( QColor::fromRgba(qrgba) );
        palette->updateList();
        selectColour(object->getColourCount()-1);
    }
    delete ok;

    /*QColor newColour = QColorDialog::getColor( initialColour );
    if (newColour.isValid()) {
    	object->addColour(newColour);
    	palette->updateList();
    	//selectColour(object->getColourCount());
    }*/
}


void Editor::removeColour(int i)
{
    if(object->removeColour(i))
    {
        palette->updateList();
    }
    else
    {
        QMessageBox::warning(this, tr("Warning"),
                             tr("You cannot remove this colour because it is used!"),
                             QMessageBox::Ok,
                             QMessageBox::Ok);
    }
}

void Editor::changeAutosave(int x)
{
    QSettings settings("Pencil","Pencil");
    if (x==0) { autosave=false; settings.setValue("autosave","false"); }
    else { autosave=true; settings.setValue("autosave","true"); }
}

void Editor::changeAutosaveNumber(int number)
{
    autosaveNumber = number;
    QSettings settings("Pencil","Pencil");
    settings.setValue("autosaveNumber", number);
}


void Editor::onionLayer1OpacityChangeSlot(int number)
{
    onionLayer1Opacity = number;
    QSettings settings("Pencil","Pencil");
    settings.setValue("onionLayer1Opacity", number);
}


void Editor::onionLayer2OpacityChangeSlot(int number)
{
    onionLayer2Opacity = number;
    QSettings settings("Pencil","Pencil");
    settings.setValue("onionLayer2Opacity", number);
}


void Editor::onionLayer3OpacityChangeSlot(int number)
{
    onionLayer3Opacity = number;
    QSettings settings("Pencil","Pencil");
    settings.setValue("onionLayer3Opacity", number);
}


void Editor::modification()
{
    modification(currentLayer);
}

void Editor::modification(int layerNumber)
{
    modified = true;
    if(object != NULL) object->modification();
    lastModifiedFrame = currentFrame;
    lastModifiedLayer = layerNumber;
    scribbleArea->update();
    timeLine->updateContent();
    numberOfModifications++;
    if(autosave && numberOfModifications > autosaveNumber)
    {
        numberOfModifications = 0;
        //saveForce();
        if (savedName!="") saveObject(savedName);
    }
}

void Editor::backup(QString undoText)
{
    if(lastModifiedLayer>-1 && lastModifiedFrame > 0)
    {
        backup(lastModifiedLayer, lastModifiedFrame, undoText);
    }
    if( lastModifiedLayer != currentLayer || lastModifiedFrame != currentFrame )
    {
        backup(currentLayer, currentFrame, undoText);
    }
}

void Editor::backup(int backupLayer, int backupFrame, QString undoText)
{
    while(backupList.size()-1 > backupIndex && backupList.size() > 0)
    {
        delete backupList.takeLast();
    }
    while(backupList.size() > 19)   // we authorize only 20 levels of cancellation
    {
        delete backupList.takeFirst();
        backupIndex--;
    }
    Layer* layer = object->getLayer(backupLayer);
    if(layer != NULL)
    {
        if(layer->type == Layer::BITMAP)
        {
            BackupBitmapElement* element = new BackupBitmapElement();
            element->layer = backupLayer;
            element->frame = backupFrame;
            element->undoText = undoText;
            element->somethingSelected = this->getScribbleArea()->somethingSelected;
            element->mySelection = this->getScribbleArea()->mySelection;
            element->myTransformedSelection = this->getScribbleArea()->myTransformedSelection;
            element->myTempTransformedSelection = this->getScribbleArea()->myTempTransformedSelection;
            BitmapImage* bitmapImage = ((LayerBitmap*)layer)->getLastBitmapImageAtFrame(backupFrame, 0);
            if(bitmapImage != NULL)
            {
                element->bitmapImage =  bitmapImage->copy();  // copy the image
                backupList.append(element);
                backupIndex++;
            }
        }
        if(layer->type == Layer::VECTOR)
        {
            BackupVectorElement* element = new BackupVectorElement();
            element->layer = backupLayer;
            element->frame = backupFrame;
            element->undoText = undoText;
            element->somethingSelected = this->getScribbleArea()->somethingSelected;
            element->mySelection = this->getScribbleArea()->mySelection;
            element->myTransformedSelection = this->getScribbleArea()->myTransformedSelection;
            element->myTempTransformedSelection = this->getScribbleArea()->myTempTransformedSelection;
            VectorImage* vectorImage = ((LayerVector*)layer)->getLastVectorImageAtFrame(backupFrame, 0);
            if(vectorImage != NULL)
            {
                element->vectorImage = *vectorImage;  // copy the image (that works but I should also provide a copy() method)
                backupList.append(element);
                backupIndex++;
            }
        }
    }
}

void BackupBitmapElement::restore(Editor* editor)
{
    Layer* layer = editor->object->getLayer(this->layer);
    if(layer != NULL)
    {
        if(layer->type == Layer::BITMAP)
        {
            *(   ((LayerBitmap*)layer)->getLastBitmapImageAtFrame(this->frame, 0)    ) = this->bitmapImage;  // restore the image
        }
    }
    editor->getScribbleArea()->somethingSelected = this->somethingSelected;
    editor->getScribbleArea()->mySelection = this->mySelection;
    editor->getScribbleArea()->myTransformedSelection = this->myTransformedSelection;
    editor->getScribbleArea()->myTempTransformedSelection = this->myTempTransformedSelection;

    editor->updateFrame(this->frame);
    editor->scrubTo(this->frame);
}

void BackupVectorElement::restore(Editor* editor)
{
    Layer* layer = editor->object->getLayer(this->layer);
    if(layer != NULL)
    {
        if(layer->type == Layer::VECTOR)
        {
            *(  ((LayerVector*)layer)->getLastVectorImageAtFrame(this->frame, 0)  ) = this->vectorImage;  // restore the image
            //((LayerVector*)layer)->getLastVectorImageAtFrame(this->frame, 0)->setModified(true); // why?
            //editor->scribbleArea->setModified(layer, this->frame);
        }
    }
    editor->getScribbleArea()->somethingSelected = this->somethingSelected;
    editor->getScribbleArea()->mySelection = this->mySelection;
    editor->getScribbleArea()->myTransformedSelection = this->myTransformedSelection;
    editor->getScribbleArea()->myTempTransformedSelection = this->myTempTransformedSelection;

    editor->updateFrameAndVector(this->frame);
    editor->scrubTo(this->frame);
}

void Editor::undo()
{
    if( backupList.size() > 0 && backupIndex > -1)
    {
        if(backupIndex == backupList.size()-1)
        {
            BackupElement* lastBackupElement = backupList[backupIndex];
            if(lastBackupElement->type() == BackupElement::BITMAP_MODIF)
            {
                BackupBitmapElement* lastBackupBitmapElement = (BackupBitmapElement*)lastBackupElement;
                backup( lastBackupBitmapElement->layer, lastBackupBitmapElement->frame ,"NoOp");
                backupIndex--;

            }
            if(lastBackupElement->type() == BackupElement::VECTOR_MODIF)
            {
                BackupVectorElement* lastBackupVectorElement = (BackupVectorElement*)lastBackupElement;
                backup( lastBackupVectorElement->layer, lastBackupVectorElement->frame ,"NoOp");
                backupIndex--;
            }
        }
        //
        backupList[backupIndex]->restore(this);
        backupIndex--;
        scribbleArea->calculateSelectionRect(); // really ugly -- to improve
    }
}

void Editor::redo()
{
    if( backupList.size() > 0 && backupIndex < backupList.size()-2)
    {
        backupIndex++;
        backupList[backupIndex+1]->restore(this);
    }
}

void Editor::clearBackup()
{
    backupIndex = -1;
    while( !backupList.isEmpty() )
    {
        delete backupList.takeLast();
    }
    lastModifiedLayer = -1;
    lastModifiedFrame = -1;
}

void Editor::cut()
{
    scribbleArea->deleteSelection();
    scribbleArea->deselectAll();
}

void Editor::crop()
{
    select_clicked();
    move_clicked();
}

void Editor::croptoselect()
{
    select_clicked();
    copy();
    clear_clicked();
    paste();

}


void Editor::copy()
{
    Layer* layer = object->getLayer(currentLayer);
    if(layer != NULL)
    {
        if(layer->type == Layer::BITMAP)
        {
            if(scribbleArea->somethingSelected)
            {
                clipboardBitmapImage =   ((LayerBitmap*)layer)->getLastBitmapImageAtFrame(currentFrame, 0)->copy( scribbleArea->getSelection().toRect() );  // copy part of the image
                scribbleArea->deselectAll();
            }
            else
            {
                clipboardBitmapImage =   ((LayerBitmap*)layer)->getLastBitmapImageAtFrame(currentFrame, 0)->copy();  // copy the whole image
                scribbleArea->deselectAll();
            }
            clipboardBitmapOk = true;
            if( clipboardBitmapImage.image != NULL ) QApplication::clipboard()->setImage( *(clipboardBitmapImage.image) );
        }
        if(layer->type == Layer::VECTOR)
        {
            clipboardVectorOk = true;
            clipboardVectorImage = *(  ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0)  );  // copy the image (that works but I should also provide a copy() method)
            scribbleArea->deselectAll();
        }
    }
}

void Editor::inbetween()
{
    Layer* layer = object->getLayer(currentLayer);
    if(layer != NULL)
    {
        if(layer->type == Layer::BITMAP)
        {
            if(scribbleArea->somethingSelected)
            {
                clipboardBitmapImage =   ((LayerBitmap*)layer)->getLastBitmapImageAtFrame(currentFrame, 0)->copy( scribbleArea->getSelection().toRect() );  // copy part of the image
                //				scribbleArea->deselectAll();
                addKey();
                //	backup(tr("Inbetween"));
                BitmapImage tobePasted = clipboardBitmapImage.copy();
                if(scribbleArea->somethingSelected)
                {
                    QRectF selection = scribbleArea->getSelection();
                    if( clipboardBitmapImage.width() <= selection.width() && clipboardBitmapImage.height() <= selection.height() )
                    {
                        selection.moveRight(3);
                        tobePasted.transform( selection, true );//TODO move its x factor
                        ((LayerBitmap*)layer)->getLastBitmapImageAtFrame(currentFrame, 0)->paste( &tobePasted );
                        move_clicked();
                        scribbleArea->updateFrame();
                    }



                }
            }

        }
    }
}

void Editor::inbetweenV()
{
    Layer* layer = object->getLayer(currentLayer);
    if(layer != NULL)
    {
        if(layer->type == Layer::VECTOR)
        {
            scribbleArea->moveOn();
            scribbleArea->selectOn();
            scribbleArea->selectAll();
            clipboardVectorOk = true;
            clipboardVectorImage = *(  ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0)  );  // copy the image (that works but I should also provide a copy() method)
            addKey();
            //backup(tr("Inbetween"));

            VectorImage* vectorImage = ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0);
            vectorImage->paste( clipboardVectorImage ); // paste the clipboard
            scribbleArea->selectAll();
            scribbleArea->myTempTransformedSelection.translate(10,0);
            scribbleArea->myTransformedSelection = scribbleArea->myTempTransformedSelection;
            scribbleArea->calculateSelectionTransformation();
            //		scribbleArea->selectAll();
            scribbleArea->update();
            //		scribbleArea->moveOn();
            scribbleArea->setModified(currentLayer, currentFrame);
            scribbleArea->update();
            //#		scribbleArea->moveOn();
            //#		scribbleArea->selectOn();
            /*vectorImage->applySelectionTransformation();
            QRectF selection = vectorImage->getSelectionRect();
            selection.translate(10,0);
            scribbleArea->setSelection(selection, true );
            scribbleArea->paintTransformedSelection();
            //((LayerVector*)layer)->getLastVectorImageAtFrame(backupFrame, 0)->modification(); ????*/
            //	move_clicked();
            //	scribbleArea->updateFrame();
        }

    }
}

void Editor::copyFrames()
{
    bool ok;
    QString name;
    QString text = QInputDialog::getText(NULL, tr("Number of Frames\r\nEnter digits only"),tr("Blank Frames:\r\n(Enter digits only)"), QLineEdit::Normal,name, &ok);
    if (ok && !text.isEmpty())
    {
        name = text;
    }
    int a =text.toInt();
    QListIterator<int> z( frameList );
    for (int i = 0; i < a; ++i)
    {
        addKey();
    }

}

void Editor::pasteFrames()
{
    int a =frameList.count();
    QListIterator<int> z( frameList );
    for (int i = 0; i < a; ++i)
    {
        int b = z.next();
        qDebug()<< i;
        qDebug()<< a;
        scrubTo(i);
        copy();
        //int d = b+a;  Give us the interval between the frames
        scrubTo(b);//TODO scrub to selected frame copy() scrub to starting frame and paste()
        duplicateKey();
    }
}

void Editor::paste()
{
    Layer* layer = object->getLayer(currentLayer);
    if(layer != NULL)
    {
        if(layer->type == Layer::BITMAP && clipboardBitmapImage.image != NULL)   // clipboardBitmapOk
        {
            backup(tr("Paste"));
            BitmapImage tobePasted = clipboardBitmapImage.copy();
            qDebug() << "to be pasted --->" << tobePasted.image->size();
            if(scribbleArea->somethingSelected)
            {
                QRectF selection = scribbleArea->getSelection();
                if( clipboardBitmapImage.width() <= selection.width() && clipboardBitmapImage.height() <= selection.height() )
                {
                    tobePasted.moveTopLeft( selection.topLeft() );
                }
                else
                {
                    tobePasted.transform( selection, true );
                }
            }
            ((LayerBitmap*)layer)->getLastBitmapImageAtFrame(currentFrame, 0)->paste( &tobePasted ); // paste the clipboard
            move_clicked();
        }
        if(layer->type == Layer::VECTOR && clipboardVectorOk)
        {
            backup(tr("Paste"));
            scribbleArea->deselectAll();
            VectorImage* vectorImage = ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0);
            vectorImage->paste( clipboardVectorImage );  // paste the clipboard
            scribbleArea->setSelection( vectorImage->getSelectionRect(), true );
            //((LayerVector*)layer)->getLastVectorImageAtFrame(backupFrame, 0)->modification(); ????
            move_clicked();
        }
    }
    scribbleArea->updateFrame();
}

void Editor::deselectAll()
{
    scribbleArea->deselectAll();
}
void Editor::clipboardChanged()
{
    if(clipboardBitmapOk == false)
    {
        clipboardBitmapImage.image = new QImage( QApplication::clipboard()->image() );
        clipboardBitmapImage.boundaries = QRect( clipboardBitmapImage.topLeft(), clipboardBitmapImage.image->size() );
        qDebug() << "New clipboard image" << clipboardBitmapImage.image->size();
    }
    else
    {
        clipboardBitmapOk = false;
        qDebug() << "The image has been saved in the clipboard";
    }
}

void Editor::newBitmapLayer()
{
    if(object != NULL)
    {
        object->addNewBitmapLayer();
        timeLine->updateLayerNumber( object->getLayerCount() );
        setCurrentLayer( object->getLayerCount()-1 );
    }
}

void Editor::newVectorLayer()
{
    if(object != NULL)
    {
        object->addNewVectorLayer();
        timeLine->updateLayerNumber( object->getLayerCount() );
        setCurrentLayer( object->getLayerCount()-1 );
    }
}

void Editor::newSoundLayer()
{
    if(object != NULL)
    {
        object->addNewSoundLayer();
        timeLine->updateLayerNumber( object->getLayerCount() );
        setCurrentLayer( object->getLayerCount()-1 );
    }
}

void Editor::newCameraLayer()
{
    if(object != NULL)
    {
        object->addNewCameraLayer();
        timeLine->updateLayerNumber( object->getLayerCount() );
        setCurrentLayer( object->getLayerCount()-1 );
    }
}

void Editor::deleteCurrentLayer()
{
    int ret = QMessageBox::warning(this, tr("Warning"),
                                   tr("Are you sure you want to delete layer: "+object->getLayer(currentLayer)->name+" ?"),
                                   QMessageBox::Ok | QMessageBox::Cancel,
                                   QMessageBox::Ok);
    if(ret == QMessageBox::Ok)
    {
        object->deleteLayer(currentLayer);
        if(currentLayer == object->getLayerCount()) setCurrentLayer( currentLayer-1 );
        timeLine->updateLayerNumber( object->getLayerCount() );
        //timeLine->update();
        scribbleArea->updateAllFrames();
    }
}


void Editor::toggleMirror()
{
    object->toggleMirror();
    scribbleArea->toggleMirror();
}
void Editor::toggleMirrorV()
{
    object->toggleMirror();
    scribbleArea->toggleMirrorV();
}
void Editor::toggleShowAllLayers()
{
    scribbleArea->toggleShowAllLayers();
    timeLine->updateContent();
}

void Editor::resetMirror()
{
    object->resetMirror();
    toolSet->resetMirror();
}

void Editor::showPalette()
{
    if(palette->isVisible())
    {
        palette->close();
    }
    else
    {
        palette->show();
    }
}

void Editor::saveLength(QString x)
{
    bool ok;
    int dec = x.toInt(&ok, 10);
    QSettings settings("Pencil","Pencil");
    settings.setValue("length", dec);
}

void Editor::about()
{
    QMessageBox::about(this, tr("Pencil Animation 0.5 beta (Morevna Branch)"),
                       tr("<table style='background-color: #DDDDDD' border='0'><tr><td valign='top'>"
                          "<img src=':icons/logo.png' width='318' height='123' border='0'><br></td></tr><tr><td>"
                          "Developed by: <i>Pascal Naidon</i> &  <i>Patrick Corrieri</i><br>"
                          "Patches by: <i>Mj Mendoza IV and D.F.</i><br>"
                          "Version: <b>0.5</b> (12 sep 2011)<br><br>"
                          "<b>Thanks to:</b><br>"
                          "Trolltech for the Qt libraries<br>"
                          "Roland for the Movie export functions<br>"
                          "Axel for his help with Qt<br>"
                          "Mark for his help with Qt and SVN<br><br>"
                          "<a href='http://www.pencil-animation.org'>http://www.pencil-animation.org</a><br><br>"
                          "Distributed under the <a href='http://www.gnu.org/copyleft/gpl.html'>GPL License</a>."
                          "</td></tr></table>"));
}

void Editor::helpBox()
{
//	QSettings settings("Pencil","Pencil");
//		QString myPath = settings.value("lastFilePath", QVariant(QDir::homePath())).toString();
    QUrl url = QUrl::fromLocalFile(QDir::currentPath() + "/Help/User Manual.pdf" );
    QDesktopServices::openUrl( url );
}


bool Editor::maybeSave()
{
    if (object->modified)
    {
        int ret = QMessageBox::warning(this, tr("Warning"),
                                       tr("This animation has been modified.\n"
                                          "Do you want to save your changes?"),
                                       QMessageBox::Yes | QMessageBox::Default,
                                       QMessageBox::No,
                                       QMessageBox::Cancel | QMessageBox::Escape);
        if (ret == QMessageBox::Yes)
        {
            saveForce();
            return true;
        }
        else if (ret == QMessageBox::Cancel)
        {
            return false;
        }
    }
    return true;
}

bool Editor::saveObject(QString filePath)
{
    QFileInfo fileInfo(filePath);
    if(fileInfo.isDir()) return false;

    QFileInfo dataInfo(filePath+".data");
    if(!dataInfo.exists())
    {
        QDir dir(fileInfo.absolutePath()); // the directory where filePath is or will be saved
        dir.mkpath(filePath+".data"); // creates a directory with the same name +".data"
    }

    savedName=filePath;
    mainWindow->setWindowTitle(savedName);

    QProgressDialog progress("Saving document...", "Abort", 0, 100, mainWindow);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    int progressValue = 0;

    // save data
    int nLayers = object->getLayerCount();
    for(int i=0; i < nLayers; i++)
    {
        Layer* layer = object->getLayer(i);
        qDebug() << "Saving Layer " << i << "(" <<layer->name << ")";
        progressValue = (i*100)/nLayers;
        progress.setValue(progressValue);
        if(layer->type == Layer::BITMAP) ((LayerBitmap*)layer)->saveImages(filePath+".data", i);
        if(layer->type == Layer::VECTOR) ((LayerVector*)layer)->saveImages(filePath+".data", i);
        if(layer->type == Layer::SOUND) ((LayerSound*)layer)->saveImages(filePath+".data", i);
    }

    // save palette
    object->savePalette(filePath+".data");

    // -------- save main XML file -----------
    QFile* file = new QFile(filePath);
    if (!file->open(QFile::WriteOnly | QFile::Text))
    {
        //QMessageBox::warning(this, "Warning", "Cannot write file");
        return false;
    }
    QTextStream out(file);
    QDomDocument doc("PencilDocument");
    QDomElement root = doc.createElement("document");
    doc.appendChild(root);

    // save editor information
    QDomElement editorElement = createDomElement(doc);
    root.appendChild(editorElement);
    // save object
    QDomElement objectElement = object->createDomElement(doc);
    root.appendChild(objectElement);

    int IndentSize = 2;
    doc.save(out, IndentSize);
    // -----------------------------------

    progress.setValue(100);

    object->modified = false;
    timeLine->updateContent();
    return true;
}

void Editor::newObject()
{
    //if(object != NULL) delete object;
    Object* newObject = new Object();
    newObject->defaultInitialisation();

    setObject(newObject);
    updateObject();
    savedName = "";
    maxFrame=0;
    currentFrame=0;
    scrubTo(0);
    mainWindow->setWindowTitle(tr("Pencil Animation v0.5 (Morevna Branch)"));
}

void Editor::setObject(Object* object)
{
    if ((this->object)&&(object!=this->object))
    {
        disconnect( this->object, 0, 0, 0); // disconnect the current object from everything
        delete this->object;
    }
    this->object = object;
    if(object)
    {
        connect( object, SIGNAL(imageAdded(int)), this, SLOT(addFrame(int)) );
        connect( object, SIGNAL(imageAdded(int,int)), this, SLOT(addFrame(int,int)) );
        connect( object, SIGNAL(imageRemoved(int)), this, SLOT(removeFrame(int)) );

        //currentLayer = object->getLayerCount()-1; // the default selected layer is the last one
        currentLayer = 0; // the default selected layer is the first one
        currentFrame = 1;
        frameList.clear();
        frameList << 1;
    }
}

void Editor::updateObject()
{
    scribbleArea->resetColours();
    palette->selectColour(0);

    //scribbleArea->resize(object->size);
    //scribbleArea->updateAllFrames();
    //scribbleArea->resetView();
    timeLine->updateLayerNumber(object->getLayerCount());
    palette->updateList();
    clearBackup();
    scribbleArea->resetColours();
    palette->selectColour(0);
    scribbleArea->updateAllFrames();
    updateMaxFrame();
    //timeLine->forceUpdateLength(QString::number(maxFrame));
}

bool Editor::openObject(QString filePath)
{
    // ---- test before opening ----
    QFileInfo fileInfo(filePath);
    if( fileInfo.isDir() ) return false;
    QFile* file = new QFile(filePath);
    if (!file->open(QFile::ReadOnly)) return false;
    QDomDocument doc;
    if (!doc.setContent(file)) return false; // this is not a XML file
    QDomDocumentType type = doc.doctype();
    if(type.name() != "PencilDocument" && type.name() != "MyObject") return false; // this is not a Pencil document
    // -----------------------------

    QProgressDialog progress("Opening document...", "Abort", 0, 100, mainWindow);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    savedName = filePath;
    QSettings settings("Pencil","Pencil");
    settings.setValue("lastFilePath", QVariant(savedName) );
    mainWindow->setWindowTitle(savedName);

    Object* newObject = new Object();
    if(!newObject->loadPalette(savedName+".data")) newObject->loadDefaultPalette();
    setObject(newObject);

    // ------- reads the XML file -------
    bool ok = true;
    int prog = 0;
    QDomElement docElem = doc.documentElement();
    if(docElem.isNull()) return false;
    if(docElem.tagName() == "document")
    {
        QDomNode tag = docElem.firstChild();
        while(!tag.isNull())
        {
            QDomElement element = tag.toElement(); // try to convert the node to an element.
            if(!element.isNull())
            {
                prog+=MIN(prog+10,100);
                progress.setValue(prog);
                if(element.tagName() == "editor")
                {
                    loadDomElement(element, filePath);
                }
                if(element.tagName() == "object")
                {
                    ok = newObject->loadDomElement(element, filePath);
                    qDebug() << "filePath:" << filePath;
                }
            }
            tag = tag.nextSibling();
        }
    }
    else
    {
        if(docElem.tagName() == "object" || docElem.tagName() == "MyOject")   // old Pencil format (<=0.4.3)
        {
            ok = newObject->loadDomElement(docElem, filePath);
        }
    }
    // ------------------------------
    if(ok) updateObject();

    progress.setValue(100);
    return ok;
}

void Editor::saveForce()
{
    if (savedName!="")
        saveObject(savedName);
    else saveDocument();
}

void Editor::createNewDocumentDialog()
{
    /*newDocumentDialog = new QDialog(this, Qt::Dialog);
    QGridLayout *mainLayout = new QGridLayout;

    QGroupBox *resolutionBox = new QGroupBox(tr("Resolution"));
    newDocumentDialog_hBox = new QSpinBox(this);
    newDocumentDialog_hBox->setMinimum(1);
    newDocumentDialog_hBox->setMaximum(10000);
    int defaultWidth = 720; int defaultHeight = 540;
    //if(object != NULL) {
    //	defaultWidth = object->size.width(); defaultHeight = object->size.height();
    //}
    newDocumentDialog_hBox->setValue(defaultWidth);
    newDocumentDialog_hBox->setFixedWidth(80);
    newDocumentDialog_vBox = new QSpinBox(this);
    newDocumentDialog_vBox->setMinimum(1);
    newDocumentDialog_vBox->setMaximum(10000);
    newDocumentDialog_vBox->setValue(defaultHeight);
    newDocumentDialog_vBox->setFixedWidth(80);
    QGridLayout *resolutionLayout = new QGridLayout;
    resolutionLayout->addWidget(newDocumentDialog_hBox,0,0);
    resolutionLayout->addWidget(newDocumentDialog_vBox,0,1);
    resolutionBox->setLayout(resolutionLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), newDocumentDialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), newDocumentDialog, SLOT(reject()));

    mainLayout->addWidget(resolutionBox, 0, 0);
    mainLayout->addWidget(buttonBox, 1, 0);
    newDocumentDialog->setLayout(mainLayout);
    newDocumentDialog->setWindowTitle(tr("Options"));
    newDocumentDialog->setModal(true);*/
}

void Editor::createExportFramesSizeBox()
{
    int defaultWidth = 720;
    int defaultHeight = 540;
    exportFramesDialog_hBox = new QSpinBox(this);
    exportFramesDialog_hBox->setMinimum(1);
    exportFramesDialog_hBox->setMaximum(10000);
    exportFramesDialog_hBox->setValue(defaultWidth);
    exportFramesDialog_hBox->setFixedWidth(80);
    exportFramesDialog_vBox = new QSpinBox(this);
    exportFramesDialog_vBox->setMinimum(1);
    exportFramesDialog_vBox->setMaximum(10000);
    exportFramesDialog_vBox->setValue(defaultHeight);
    exportFramesDialog_vBox->setFixedWidth(80);
}

void Editor::createExportMovieSizeBox()
{
    int defaultWidth = 720;
    int defaultHeight = 540;
    int defaultFps = 25;
    exportMovieDialog_hBox = new QSpinBox(this);
    exportMovieDialog_hBox->setMinimum(1);
    exportMovieDialog_hBox->setMaximum(10000);
    exportMovieDialog_hBox->setValue(defaultWidth);
    exportMovieDialog_hBox->setFixedWidth(80);
    exportMovieDialog_vBox = new QSpinBox(this);
    exportMovieDialog_vBox->setMinimum(1);
    exportMovieDialog_vBox->setMaximum(10000);
    exportMovieDialog_vBox->setValue(defaultHeight);
    exportMovieDialog_vBox->setFixedWidth(80);

    exportMovieDialog_format = new QComboBox();
    exportMovieDialog_format->addItem("AUTO");
    exportMovieDialog_format->addItem("MOV");
    exportMovieDialog_format->addItem("MPEG2/AVI");
    exportMovieDialog_format->addItem("MPEG4/AVI");
    exportMovieDialog_format->addItem("MPEG4/MP4");
    exportMovieDialog_fpsBox = new QSpinBox(this);
    exportMovieDialog_fpsBox->setMinimum(1);
    exportMovieDialog_fpsBox->setMaximum(60);
    exportMovieDialog_fpsBox->setValue(defaultFps);
    exportMovieDialog_fpsBox->setFixedWidth(40);

}

void Editor::createExportFramesDialog()
{
    exportFramesDialog = new QDialog(this, Qt::Dialog);
    QGridLayout* mainLayout = new QGridLayout;

    QGroupBox* resolutionBox = new QGroupBox(tr("Resolution"));
    if(exportFramesDialog_hBox == NULL || exportFramesDialog_vBox == NULL)
    {
        createExportFramesSizeBox();
    }
    QGridLayout* resolutionLayout = new QGridLayout;
    resolutionLayout->addWidget(exportFramesDialog_hBox,0,0);
    resolutionLayout->addWidget(exportFramesDialog_vBox,0,1);
    resolutionBox->setLayout(resolutionLayout);

    QGroupBox* formatBox = new QGroupBox(tr("Format"));
    exportFramesDialog_format = new QComboBox();
    exportFramesDialog_format->addItem("PNG");
    exportFramesDialog_format->addItem("JPG");
    exportFramesDialog_format->addItem("TIF");
    exportFramesDialog_format->addItem("BMP");
    QGridLayout* formatLayout = new QGridLayout;
    formatLayout->addWidget(exportFramesDialog_format,0,0);
    formatBox->setLayout(formatLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), exportFramesDialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), exportFramesDialog, SLOT(reject()));

    mainLayout->addWidget(resolutionBox, 0, 0);
    mainLayout->addWidget(formatBox, 1, 0);
    mainLayout->addWidget(buttonBox, 2, 0);
    exportFramesDialog->setLayout(mainLayout);
    exportFramesDialog->setWindowTitle(tr("Options"));
    exportFramesDialog->setModal(true);
}

void Editor::createExportMovieDialog()
{
    exportMovieDialog = new QDialog(this, Qt::Dialog);
    QGridLayout* mainLayout = new QGridLayout;

    QGroupBox* resolutionBox = new QGroupBox(tr("Resolution"));
    if(!exportMovieDialog_hBox || !exportMovieDialog_vBox)
    {
        createExportMovieSizeBox();
    }
    QGridLayout* resolutionLayout = new QGridLayout;
    resolutionLayout->addWidget(exportMovieDialog_hBox,0,0);
    resolutionLayout->addWidget(exportMovieDialog_vBox,0,1);
    resolutionBox->setLayout(resolutionLayout);

    QGroupBox* formatBox = new QGroupBox(tr("Format"));
    QGridLayout* formatLayout = new QGridLayout;
    QLabel* label1 = new QLabel("Save as");
    formatLayout->addWidget(label1,0,0);
    formatLayout->addWidget(exportMovieDialog_format,0,1);
    QLabel* label2 = new QLabel("Fps");
    formatLayout->addWidget(label2,0,2);
    formatLayout->addWidget(exportMovieDialog_fpsBox,0,3);
    formatBox->setLayout(formatLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), exportMovieDialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), exportMovieDialog, SLOT(reject()));

    mainLayout->addWidget(resolutionBox, 0, 0);
    mainLayout->addWidget(formatBox, 1, 0);
    mainLayout->addWidget(buttonBox, 2, 0);
    exportMovieDialog->setLayout(mainLayout);
    exportMovieDialog->setWindowTitle(tr("Options"));
    exportMovieDialog->setModal(true);
}



void Editor::createExportFlashDialog()
{
    exportFlashDialog = new QDialog(this, Qt::Dialog);
    QGridLayout* mainLayout = new QGridLayout;

    QSettings settings("Pencil","Pencil");


    exportFlashDialog_compression = new QSlider(Qt::Horizontal);
    exportFlashDialog_compression->setTickPosition(QSlider::TicksBelow);
    exportFlashDialog_compression->setMinimum(0);
    exportFlashDialog_compression->setMaximum(10);
    exportFlashDialog_compression->setValue( 10 - settings.value("flashCompressionLevel").toInt() );
    QLabel* label1 = new QLabel("Large file");
    QLabel* label2 = new QLabel("Small file");

    QGroupBox* compressionBox = new QGroupBox(tr("Compression"));
    QGridLayout* compressionLayout = new QGridLayout;
    compressionLayout->addWidget(label1,0,0);
    compressionLayout->addWidget(exportFlashDialog_compression,0,1);
    compressionLayout->addWidget(label2,0,2);
    compressionBox->setLayout(compressionLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    connect(buttonBox, SIGNAL(accepted()), exportFlashDialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), exportFlashDialog, SLOT(reject()));

    mainLayout->addWidget(compressionBox, 0, 0);
    mainLayout->addWidget(buttonBox, 1, 0);
    exportFlashDialog->setLayout(mainLayout);
    exportFlashDialog->setWindowTitle(tr("Export SWF Options"));
    exportFlashDialog->setModal(true);
}

QMatrix Editor::map(QRectF source, QRectF target)   // this method should be put somewhere else...
{
    qreal x1 = source.left();
    qreal y1 = source.top();
    qreal x2 = source.right();
    qreal y2 = source.bottom();
    qreal x1P = target.left();
    qreal y1P = target.top();
    qreal x2P = target.right();
    qreal y2P = target.bottom();
    QMatrix matrix;
    bool mirror = false;
    if( (x1 != x2) && (y1 != y2) )
    {
        if( !mirror)
        {
            matrix = QMatrix ( (x2P-x1P)/(x2-x1), 0, 0, (y2P-y1P)/(y2-y1), (x1P*x2-x2P*x1)/(x2-x1), (y1P*y2-y2P*y1)/(y2-y1) );
        }
        else
        {
            matrix = QMatrix ( (x2P-x1P)/(x1-x2), 0, 0, (y2P-y1P)/(y2-y1), (x1P*x1-x2P*x2)/(x1-x2), (y1P*y2-y2P*y1)/(y2-y1) );
        }
    }
    else
    {
        matrix.reset();
    }
    return matrix;
}

bool Editor::exportSeqCLI(QString filePath = "", QString format = "PNG")
{
    int width = scribbleArea->getViewRect().toRect().width();
    int height = scribbleArea->getViewRect().toRect().height();

    QSize exportSize = QSize(width, height);
    QByteArray exportFormat(format.toLatin1());

    QMatrix view = map( scribbleArea->getViewRect(), QRectF(QPointF(0,0), exportSize) );
    view = scribbleArea->getView() * view;

    updateMaxFrame();
    object->exportFrames(1, maxFrame, view, getCurrentLayer(), exportSize, filePath, exportFormat, -1, false, true, 2,NULL,0);
    return true;
}

bool Editor::exportSeq()
{
    QSettings settings("Pencil","Pencil");
    QString initialPath = settings.value("lastExportPath", QVariant(QDir::homePath())).toString();
    if(initialPath.isEmpty())
        QString	initialPath = QDir::homePath() + "/untitled";
//	QString filePath = QFileDialog::getSaveFileName(this, tr("Export As",".",tr("Spreadsheet files (*.sp)"));
    QString filePath=QFileDialog::getSaveFileName(this, tr("Save Image Sequence"), initialPath,tr("PNG (*.png);;JPG(*.jpg);;TIFF(*.tiff);;TIF(*.tif);;BMP(*.bmp);;GIF(*.gif)"));
//	QString filePath = QFileDialog::getSaveFileName(this,tr("Save Spreadsheet"), ".",tr("Spreadsheet files (*.sp)"));
    if (filePath.isEmpty())
    {
        return false;
    }
    else
    {
        settings.setValue("lastExportPath", QVariant(filePath));

        if (!exportFramesDialog) createExportFramesDialog();
        exportFramesDialog_hBox->setValue( scribbleArea->getViewRect().toRect().width() );
        exportFramesDialog_vBox->setValue( scribbleArea->getViewRect().toRect().height() );
        exportFramesDialog->exec();
        if(exportFramesDialog->result() == QDialog::Rejected) return false;

        QSize exportSize = QSize(exportFramesDialog_hBox->value(), exportFramesDialog_vBox->value());
        //QMatrix view = map( QRectF(QPointF(0,0), scribbleArea->size() ), QRectF(QPointF(0,0), exportSize) );
        QMatrix view = map( scribbleArea->getViewRect(), QRectF(QPointF(0,0), exportSize) );
        view = scribbleArea->getView() * view;

        QByteArray exportFormat(exportFramesDialog_format->currentText().toLatin1());
        updateMaxFrame();
        object->exportFrames(1, maxFrame, view, getCurrentLayer(), exportSize, filePath, exportFormat, -1, false, true, 2,NULL,0);
        return true;
    }
}

bool Editor::exportX()
{
    QSettings settings("Pencil","Pencil");
    QString initialPath = settings.value("lastExportPath", QVariant(QDir::homePath())).toString();
    if(initialPath.isEmpty()) initialPath = QDir::homePath() + "/untitled";
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save As"),initialPath);
    if (filePath.isEmpty())
    {
        qDebug() << "empty file";
        return false;
    }
    else
    {
        settings.setValue("lastExportPath", QVariant(filePath));

        QSize exportSize = scribbleArea->getViewRect().toRect().size();
        QMatrix view = map( scribbleArea->getViewRect(), QRectF(QPointF(0,0), exportSize) );
        view = scribbleArea->getView() * view;

        updateMaxFrame();
        object->exportX(1, maxFrame, view, exportSize, filePath, true, 2);
        return true;
    }
}

bool Editor::exportImage()
{
    QSettings settings("Pencil","Pencil");
    QString initialPath = settings.value("lastExportPath", QVariant(QDir::homePath())).toString();
    if(initialPath.isEmpty()) initialPath = QDir::homePath() + "/untitled.png";
//	QString filePath = QFileDialog::getSaveFileName(this, tr("Save As"),initialPath);
    QString filePath=QFileDialog::getSaveFileName(this, tr("Save Image"), initialPath,tr("PNG (*.png);;JPG(*.jpg);;TIFF(*.tiff);;TIF(*.tif);;BMP(*.bmp);;GIF(*.gif)"));
    if (filePath.isEmpty())
    {
        qDebug() << "empty file";
        return false;
    }
    else
    {
        settings.setValue("lastExportPath", QVariant(filePath));

        QSize exportSize = scribbleArea->getViewRect().toRect().size();
        QMatrix view = map( scribbleArea->getViewRect(), QRectF(QPointF(0,0), exportSize) );
        view = scribbleArea->getView() * view;

        updateMaxFrame();
        object->exportIm(currentFrame, maxFrame, view, exportSize, filePath, true, 2);
        return true;
    }
}



bool Editor::exportMov()
{
    QSettings settings("Pencil","Pencil");
    QString initialPath = settings.value("lastExportPath", QVariant(QDir::homePath())).toString();
    if(initialPath.isEmpty()) initialPath = QDir::homePath() + "/untitled.avi";
//	QString filePath = QFileDialog::getSaveFileName(this, tr("Export As"),initialPath);
    QString filePath=QFileDialog::getSaveFileName(this, tr("Export Movie As..."),initialPath ,tr("AVI (*.avi);;MOV(*.mov);;WMV(*.wmv)"));
    if (filePath.isEmpty())
    {
        return false;
    }
    else
    {
        settings.setValue("lastExportPath", QVariant(filePath));
        if (!exportMovieDialog) createExportMovieDialog();
        exportMovieDialog_hBox->setValue( scribbleArea->getViewRect().toRect().width() );
        exportMovieDialog_vBox->setValue( scribbleArea->getViewRect().toRect().height() );
        exportMovieDialog->exec();
        if(exportMovieDialog->result() == QDialog::Rejected) return false;

        QSize exportSize = QSize(exportMovieDialog_hBox->value(), exportMovieDialog_vBox->value());
        QMatrix view = map( scribbleArea->getViewRect(), QRectF(QPointF(0,0), exportSize) );
        view = scribbleArea->getView() * view;

        updateMaxFrame();
        object->exportMovie(1, maxFrame, view, getCurrentLayer(), exportSize, filePath, fps, exportMovieDialog_fpsBox->value(),exportMovieDialog_format->currentText());
        return true;
    }
}

bool Editor::exportFlash()
{
    QSettings settings("Pencil","Pencil");
    QString initialPath = settings.value("lastExportPath", QVariant(QDir::homePath())).toString();
    if(initialPath.isEmpty()) initialPath = QDir::homePath() + "/untitled.swf";
//	QString filePath = QFileDialog::getSaveFileName(this, tr("Export SWF As"),initialPath);
    QString filePath=QFileDialog::getSaveFileName(this, tr("Export Movie As..."),initialPath ,tr("SWF (*.swf)"));
    if (filePath.isEmpty())
    {
        return false;
    }
    else
    {
        settings.setValue("lastExportPath", QVariant(filePath));
        if (!exportFlashDialog) createExportFlashDialog();
        exportFlashDialog->exec();
        if(exportFlashDialog->result() == QDialog::Rejected) return false;

        settings.setValue("flashCompressionLevel", 10-exportFlashDialog_compression->value() );

        QSize exportSize = scribbleArea->getViewRect().toRect().size();
        QMatrix view = map( scribbleArea->getViewRect(), QRectF(QPointF(0,0), exportSize) );
        view = scribbleArea->getView() * view;

        updateMaxFrame();
        object->exportFlash(1, maxFrame, view, exportSize, filePath, fps, exportFlashDialog_compression->value());
        return true;
    }
}

void Editor::exportPalette()
{
    QSettings settings("Pencil","Pencil");
    QString initialPath = settings.value("lastPalettePath", QVariant(QDir::homePath())).toString();
    if(initialPath.isEmpty()) initialPath = QDir::homePath() + "/untitled.xml";
    QString filePath = QFileDialog::getSaveFileName(this, tr("Export As"),initialPath);
    if (!filePath.isEmpty())
    {
        object->exportPalette(filePath);
        settings.setValue("lastPalettePath", QVariant(filePath));
    }
}

void Editor::importPalette()
{
    QSettings settings("Pencil","Pencil");
    QString initialPath = settings.value("lastPalettePath", QVariant(QDir::homePath())).toString();
    if(initialPath.isEmpty()) initialPath = QDir::homePath() + "/untitled.xml";
    QString filePath = QFileDialog::getOpenFileName(this, tr("Import"),initialPath);
    if (!filePath.isEmpty())
    {
        object->importPalette(filePath);
        palette->updateList();
        settings.setValue("lastPalettePath", QVariant(filePath));
    }
}

void Editor::importImage()
{
    importImage("fromDialog");
}

void Editor::importImage(QString filePath)
{
    Layer* layer = object->getLayer(currentLayer);
    if(layer != NULL)
    {
        if( layer->type == Layer::BITMAP || layer->type == Layer::VECTOR )
        {
            if(filePath == "fromDialog")
            {
                QSettings settings("Pencil","Pencil");
                QString initialPath = settings.value("lastImportPath", QVariant(QDir::homePath())).toString();
                if(initialPath.isEmpty()) initialPath = QDir::homePath();
                filePath = QFileDialog::getOpenFileName(this, tr("Import image..."),initialPath ,tr("PNG (*.png);;JPG(*.jpg);;TIFF(*.tiff);;TIF(*.tif);;BMP(*.bmp);;GIF(*.gif)"));
                if (!filePath.isEmpty()) settings.setValue("lastImportPath", QVariant(filePath));
            }
            if (!filePath.isEmpty())
            {
                backup(tr("ImportImg"));
                // --- option 1
                //((LayerBitmap*)layer)->loadImageAtFrame(filePath, currentFrame);
                // --- option 2
                // TO BE IMPROVED
                if(layer->type == Layer::BITMAP)
                {
                    BitmapImage* bitmapImage = ((LayerBitmap*)layer)->getBitmapImageAtFrame(currentFrame);
                    if(bitmapImage == NULL) { addKey(); bitmapImage = ((LayerBitmap*)layer)->getBitmapImageAtFrame(currentFrame); }
                    QImage* importedImage = new QImage(filePath);
                    if(!importedImage->isNull())
                    {
                        QRect boundaries = importedImage->rect();
                        //boundaries.moveTopLeft( scribbleArea->getView().inverted().map(QPoint(0,0)) );
                        boundaries.moveTopLeft( scribbleArea->getCentralPoint().toPoint() - QPoint(boundaries.width()/2, boundaries.height()/2) );
                        BitmapImage* importedBitmapImage = new BitmapImage(NULL, boundaries, *importedImage);
                        if(scribbleArea->somethingSelected)
                        {
                            QRectF selection = scribbleArea->getSelection();
                            if( importedImage->width() <= selection.width() && importedImage->height() <= selection.height() )
                            {
                                importedBitmapImage->boundaries.moveTopLeft( selection.topLeft().toPoint() );
                            }
                            else
                            {
                                importedBitmapImage->transform( selection.toRect(), true );
                            }
                        }
                        bitmapImage->paste( importedBitmapImage );
                    }
                    else
                    {
                        QMessageBox::warning(this, tr("Warning"),
                                             tr("Unable to load bitmap image.<br><b>TIP:</b> Use Bitmap layer to import bitmaps."),
                                             QMessageBox::Ok,
                                             QMessageBox::Ok);
                    }
                }
                if(layer->type == Layer::VECTOR)
                {
                    VectorImage* vectorImage = ((LayerVector*)layer)->getVectorImageAtFrame(currentFrame);
                    if(vectorImage == NULL) { addKey(); vectorImage = ((LayerVector*)layer)->getVectorImageAtFrame(currentFrame); }
                    VectorImage* importedVectorImage = new VectorImage(NULL);
                    bool ok = importedVectorImage->read(filePath);
                    if(ok)
                    {
                        importedVectorImage->selectAll();
                        vectorImage->paste( *importedVectorImage );
                    }
                    else
                    {
                        QMessageBox::warning(this, tr("Warning"),
                                             tr("Unable to load vector image.<br><b>TIP:</b> Use Vector layer to import vectors."),
                                             QMessageBox::Ok,
                                             QMessageBox::Ok);
                    }
                }
                scribbleArea->updateFrame();
                timeLine->updateContent();
            }
        }
        else
        {
            // create a new Bitmap layer ?
            QMessageBox::warning(this, tr("Warning"),
                                 tr("Please select a Bitmap or Vector layer to import images."),
                                 QMessageBox::Ok,
                                 QMessageBox::Ok);
        }
    }
}


//void Editor::importSound()
//{
//	importSound("fromDialog");
//}

void Editor::importSound(QString filePath)
{
    Layer* layer = object->getLayer(currentLayer);
    if (layer == NULL)
    {
        QMessageBox msg;
        msg.setText("You must select an empty sound layer as the destination for your sound before importing. Please create a new sound layer.");
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
        return;
    }

    if(layer->type != Layer::SOUND)
    {
        QMessageBox msg;
        msg.setText("No sound layer exists as a destination for your import. Create a new sound layer?");
        QAbstractButton* acceptButton = msg.addButton("Create sound layer", QMessageBox::AcceptRole);
        msg.addButton("Don't create layer", QMessageBox::RejectRole);

        msg.exec();
        if (msg.clickedButton() == acceptButton)
        {
            newSoundLayer();
            layer = object->getLayer(currentLayer);
        }
        else
        {
            return;
        }
    }

    if (!((LayerSound*)layer)->isEmpty())
    {
        QMessageBox msg;
        msg.setText("The sound layer you have selected already contains a sound item. Please select another.");
        msg.exec();
        return;
    }

    if(filePath.isEmpty() || filePath == "fromDialog")
    {
        QSettings settings("Pencil","Pencil");
        QString initialPath = settings.value("lastImportPath", QVariant(QDir::homePath())).toString();
        if(initialPath.isEmpty()) initialPath = QDir::homePath();
        filePath = QFileDialog::getOpenFileName(this, tr("Import sound..."),initialPath,tr("WAV(*.wav);;MP3(*.mp3)"));
        if (!filePath.isEmpty())
        {
            settings.setValue("lastImportPath", QVariant(filePath));
        }
        else
        {
            return;
        }
    }
    ((LayerSound*)layer)->loadSoundAtFrame(filePath, currentFrame);
    timeLine->updateContent();
    modification(currentLayer);
}



void Editor::updateFrame(int frameNumber)
{
    scribbleArea->updateFrame(frameNumber);
}

void Editor::updateFrameAndVector(int frameNumber)
{
    scribbleArea->updateAllVectorLayersAt(frameNumber);
}

void Editor::scrubTo(int frameNumber)
{
    if (scribbleArea->getUpdateAll())
        scribbleArea->updateAllFrames();
    int oldFrame = currentFrame;
    if(frameNumber < 1) frameNumber = 1;
    currentFrame = frameNumber;
    //toolSet->setCounter(frameNumber);
    //timeLine->setCurrentFrame(currentFrame);
    timeLine->updateFrame(oldFrame);
    timeLine->updateFrame(currentFrame);
    timeLine->updateContent();
    scribbleArea->readCanvasFromCache = true;
    scribbleArea->update();
}

void Editor::scrubForward()
{
    scrubTo(currentFrame+1);
}

void Editor::scrubBackward()
{
    if(currentFrame > 1) scrubTo(currentFrame-1);
}

/*void Editor::scrubKF() {
	//timeLine->scrubKF();
}

void Editor::scrubKB() {
	//timeLine->scrubKB();
}*/

void Editor::previousLayer()
{
    currentLayer--;
    if(currentLayer<0) currentLayer = 0;
    timeLine->updateContent();
    scribbleArea->updateAllFrames();
}

void Editor::nextLayer()
{
    currentLayer++;
    if(currentLayer == object->getLayerCount()) currentLayer = object->getLayerCount()-1;
    timeLine->updateContent();
    scribbleArea->updateAllFrames();
}

void Editor::addKey()
{
    addKey(currentLayer, currentFrame);
}
void Editor::duplicateKey()
{
    /*	scribbleArea->selectAll();
    	copy();
    	addKey();
    	paste();*/

    Layer* layer = object->getLayer(currentLayer);
    if(layer != NULL)
    {
        if(layer->type == Layer::VECTOR)
        {
            scribbleArea->selectAll();
            clipboardVectorOk = true;
            clipboardVectorImage = *(  ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0)  );  // copy the image (that works but I should also provide a copy() method)
            addKey();
            VectorImage* vectorImage = ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0);
            vectorImage->paste( clipboardVectorImage ); // paste the clipboard
            scribbleArea->setModified(currentLayer, currentFrame);
            update();
        }
        if(layer->type == Layer::BITMAP)
        {
            scribbleArea->selectAll();
            copy();
            addKey();
            paste();
        }
    }
}

void Editor::addKey(int layerNumber, int& frameNumber)
{
    Layer* layer = object->getLayer(layerNumber);
    if(layer != NULL)
    {
        if(layer->type == Layer::BITMAP || layer->type == Layer::VECTOR || layer->type == Layer::CAMERA)
        {
            bool success = false;
            if(layer->type == Layer::BITMAP) success = ((LayerBitmap*)layer)->addImageAtFrame(frameNumber);
            if(layer->type == Layer::VECTOR) success = ((LayerVector*)layer)->addImageAtFrame(frameNumber);
            if(layer->type == Layer::CAMERA) success = ((LayerCamera*)layer)->addImageAtFrame(frameNumber);
            if(success)
            {
                timeLine->updateContent();
                //scribbleArea->addFrame(frameNumber);
            }
            else
            {
                frameNumber++;
                addKey(); // is this necessary?
                updateMaxFrame();
            }
        }
    }
}

void Editor::removeKey()
{
    Layer* layer = object->getLayer(currentLayer);
    if(layer != NULL)
    {
        if(layer->type == Layer::BITMAP) ((LayerBitmap*)layer)->removeImageAtFrame(currentFrame);
        if(layer->type == Layer::VECTOR) ((LayerVector*)layer)->removeImageAtFrame(currentFrame);
        if(layer->type == Layer::CAMERA) ((LayerCamera*)layer)->removeImageAtFrame(currentFrame);
        //if(layer->type == Layer::SOUND)  ((LayerSound*)layer)->removeImageAtFrame(currentFrame);
        scrubBackward();
        timeLine->updateContent();
        scribbleArea->updateFrame();
    }
}

void Editor::addFrame(int frameNumber)   // adding a frame to the cache
{
    frameList << frameNumber;
    qSort(frameList);
    scribbleArea->updateFrame();
    qDebug()<< frameList;
    qDebug()<< frameNumber;
    timeLine->update();
}

void Editor::addFrame(int frameNumber1, int frameNumber2)   // adding a range of frames to the cache
{
    for(int i=frameNumber1; i<=frameNumber2; i++)
    {
        frameList << i;
    }
    qSort(frameList);

    scribbleArea->updateFrame();
    timeLine->update();
}

void Editor::removeFrame(int frameNumber)
{
    frameList.removeAt( getLastIndexAtFrame(frameNumber) );
    scribbleArea->updateFrame();
    timeLine->update();
}

int Editor::getLastIndexAtFrame(int frameNumber)
{
    int position  = -1;
    int index = -1;
    for(int i=0; i < frameList.size(); i++)
    {
        if(frameList.at(i) > position && frameList.at(i) <= frameNumber)
        {
            position = frameList.at(i);
            index = i;
        }
    }
    return index;
}

int Editor::getLastFrameAtFrame(int frameNumber)
{
    return frameList.at( getLastIndexAtFrame(frameNumber) );
}



void Editor::showPreferences()
{
    preferences->show();
}

void Editor::play()
{
    updateMaxFrame();
    if(currentFrame > maxFrame)
    {
        scrubTo(maxFrame);
    }
    else
    {
        if(currentFrame == maxFrame)
        {
            if( !playing )
            {
                scrubTo(0);
            }
            else
            {
                if( looping ) { scrubTo(0); }
                else { startOrStop(); }
            }
        }
        else
        {
            startOrStop();
        }
    }
}

void Editor::startOrStop()
{
    if(!playing)
    {
        playing = true;
        timer->start();
    }
    else
    {
        playing = false;
        timer->stop();
        object->stopSoundIfAny();
    }
}

void Editor::playNextFrame()
{
    if(currentFrame < maxFrame)
    {
        if(sound) object->playSoundIfAny(currentFrame,fps);
        scrubForward();
    }
    else
    {
        if( !playing )
        {
            scrubTo(maxFrame);
        }
        else
        {
            if( looping ) { scrubTo(0); }
            else { startOrStop(); }
        }
    }
}

void Editor::playPrevFrame()
{
    if(currentFrame > 0)
    {
        if(sound) object->playSoundIfAny(currentFrame,fps);
        scrubBackward();
    }
}

void Editor::changeFps(int x)
{
    fps=x;
//	QSettings settings("Pencil","Pencil");
//	settings.setValue("fps", x);
    timer->setInterval(1000/fps);
    timeLine->updateContent();
}

int Editor::getFps()
{
    return fps;
}

void Editor::setLoop(bool checked)
{
    looping = checked;
}

void Editor::setSound()
{
    if(sound) sound = false;
    else sound = true;
}

void Editor::setCurrentLayer(int layerNumber)
{
    currentLayer = layerNumber;
    timeLine->updateContent();
    scribbleArea->updateAllFrames();
}

void Editor::switchVisibilityOfLayer(int layerNumber)
{
    Layer* layer = object->getLayer(layerNumber);
    if(layer != NULL) layer->switchVisibility();
    scribbleArea->updateAllFrames();
    timeLine->updateContent();
}

void Editor::moveLayer(int i, int j)
{
    object->moveLayer(i, j);
    if(j<i) { currentLayer = j; }
    else { currentLayer = j-1; }
    timeLine->updateContent();
    scribbleArea->updateAllFrames();
}

void Editor::updateMaxFrame()
{
    maxFrame = -1;
    for(int i=0; i < object->getLayerCount(); i++)
    {
        int frameNumber = object->getLayer(i)->getMaxFrame();
        if( frameNumber > maxFrame) maxFrame = frameNumber;
    }
    timeLine->forceUpdateLength(QString::number(maxFrame));
}

void Editor::dockAllPalettes()
{
    getToolSet()->drawPalette->setFloating(false);
    getToolSet()->optionPalette->setFloating(false);
    getToolSet()->displayPalette->setFloating(false);
    getToolSet()->onionPalette->setFloating(false);
    getTimeLine()->setFloating(false);
    getPalette()->setFloating(false);
}/*

void Editor::detachAllPalettes() {
	getToolSet()->drawPalette->setFloating(true);
	getToolSet()->optionPalette->setFloating(true);
	getToolSet()->displayPalette->setFloating(true);
	getToolSet()->onionPalette->setFloating(true);
	getTimeLine()->setFloating(true);
	getPalette()->setFloating(true);
	restorePalettesSettings(false, true, true);
}*/

void Editor::restorePalettesSettings(bool restoreFloating, bool restorePosition, bool restoreSize)
{
    QSettings settings("Pencil", "Pencil");

    Palette* colourPalette = getPalette();
    if(colourPalette != NULL)
    {
        QPoint pos = settings.value("colourPalettePosition", QPoint(100, 100)).toPoint();
        QSize size = settings.value("colourPaletteSize", QSize(400, 300)).toSize();
        bool floating = settings.value("colourPaletteFloating", false).toBool();
        if(restoreFloating) colourPalette->setFloating(floating);
        if(restorePosition) colourPalette->move(pos);
        if(restoreSize) colourPalette->resize(size);
        colourPalette->show();
    }

    TimeLine* timelinePalette = getTimeLine();
    if(timelinePalette != NULL)
    {
        QPoint pos = settings.value("timelinePalettePosition", QPoint(100, 100)).toPoint();
        QSize size = settings.value("timelinePaletteSize", QSize(400, 300)).toSize();
        bool floating = settings.value("timelinePaletteFloating", false).toBool();
        if(restoreFloating) timelinePalette->setFloating(floating);
        if(restorePosition) timelinePalette->move(pos);
        if(restoreSize) timelinePalette->resize(size);
        timelinePalette->show();
    }

    QDockWidget* drawPalette = getToolSet()->drawPalette;
    if(drawPalette != NULL)
    {
        QPoint pos = settings.value("drawPalettePosition", QPoint(100, 100)).toPoint();
        QSize size = settings.value("drawPaletteSize", QSize(400, 300)).toSize();
        bool floating = settings.value("drawPaletteFloating", false).toBool();
        if(restoreFloating) drawPalette->setFloating(floating);
        if(restorePosition) drawPalette->move(pos);
        if(restoreSize) drawPalette->resize(size);
        drawPalette->show();
    }

    QDockWidget* optionPalette = getToolSet()->optionPalette;
    if(optionPalette != NULL)
    {
        QPoint pos = settings.value("optionPalettePosition", QPoint(100, 100)).toPoint();
        QSize size = settings.value("optionPaletteSize", QSize(400, 300)).toSize();
        bool floating = settings.value("optionPaletteFloating", false).toBool();
        if(restoreFloating) optionPalette->setFloating(floating);
        if(restorePosition) optionPalette->move(pos);
        if(restoreSize) optionPalette->resize(size);
        optionPalette->show();
    }

    QDockWidget* displayPalette = getToolSet()->displayPalette;
    if(displayPalette != NULL)
    {
        QPoint pos = settings.value("displayPalettePosition", QPoint(100, 100)).toPoint();
        QSize size = settings.value("displayPaletteSize", QSize(400, 300)).toSize();
        bool floating = settings.value("displayPaletteFloating", false).toBool();
        if(restoreFloating) displayPalette->setFloating(floating);
        if(restorePosition) displayPalette->move(pos);
        if(restoreSize) displayPalette->resize(size);
        displayPalette->show();
    }
}

QDomElement Editor::createDomElement(QDomDocument& doc)
{
    QDomElement tag = doc.createElement("editor");

    QDomElement tag1 = doc.createElement("currentLayer");
    tag1.setAttribute("value", currentLayer);
    tag.appendChild(tag1);
    QDomElement tag2 = doc.createElement("currentFrame");
    tag2.setAttribute("value", currentFrame);
    tag.appendChild(tag2);
    QDomElement tag2a = doc.createElement("currentFps");
    tag2a.setAttribute("value", fps);
    tag.appendChild(tag2a);
    QDomElement tag3 = doc.createElement("currentView");
    QMatrix myView = scribbleArea->getMyView();
    tag3.setAttribute("m11", myView.m11());
    tag3.setAttribute("m12", myView.m12());
    tag3.setAttribute("m21", myView.m21());
    tag3.setAttribute("m22", myView.m22());
    tag3.setAttribute("dx", myView.dx());
    tag3.setAttribute("dy", myView.dy());
    tag.appendChild(tag3);

    return tag;
}

bool Editor::loadDomElement(QDomElement docElem, QString filePath)
{
    if(docElem.isNull()) return false;
    QDomNode tag = docElem.firstChild();
    while(!tag.isNull())
    {
        QDomElement element = tag.toElement(); // try to convert the node to an element.
        if(!element.isNull())
        {
            if(element.tagName() == "currentLayer")
            {
                currentLayer = element.attribute("value").toInt();
            }
            if(element.tagName() == "currentFrame")
            {
                currentFrame = element.attribute("value").toInt();
            }
            if(element.tagName() == "currentFps")
            {
                fps = element.attribute("value").toInt();
                //timer->setInterval(1000/fps);
                timeLine->setFps(fps);
            }
            if(element.tagName() == "currentView")
            {
                qreal m11 = element.attribute("m11").toDouble();
                qreal m12 = element.attribute("m12").toDouble();
                qreal m21 = element.attribute("m21").toDouble();
                qreal m22 = element.attribute("m22").toDouble();
                qreal dx = element.attribute("dx").toDouble();
                qreal dy = element.attribute("dy").toDouble();
                scribbleArea->setMyView( QMatrix(m11,m12,m21,m22,dx,dy) );
            }
        }
        tag = tag.nextSibling();
    }
    return true;
}

void Editor::move_clicked()
{
    scribbleArea->moveOn();
    toolSet->changeMoveButton();
}
void Editor::clear_clicked()
{
    scribbleArea->clearImage();
    //toolSet->clearClick();
}
void Editor::pencil_clicked()
{
    scribbleArea->pencilOn();
    toolSet->changePencilButton();
}
void Editor::eraser_clicked()
{
    scribbleArea->eraserOn();
    toolSet->changeEraserButton();
}
void Editor::select_clicked()
{
    scribbleArea->selectOn();
    toolSet->changeSelectButton();
}
void Editor::hand_clicked()
{
    scribbleArea->handOn();
    toolSet->changeHandButton();
}
void Editor::pen_clicked()
{
    scribbleArea->penOn();
    toolSet->changePenButton();
}
void Editor::polyline_clicked()
{
    scribbleArea->polylineOn();
    toolSet->changePolylineButton();
}
void Editor::bucket_clicked()
{
    scribbleArea->bucketOn();
    toolSet->changeBucketButton();
}
void Editor::eyedropper_clicked()
{
    scribbleArea->eyedropperOn();
    toolSet->changeEyedropperButton();
}
void Editor::color_clicked()
{
    scribbleArea->colouringOn();
    toolSet->changeColouringButton();
}
void Editor::smudge_clicked()
{
    scribbleArea->smudgeOn();
    toolSet->changeSmudgeButton();
}

void Editor::setzoom()
{
    scribbleArea->zoom();
}

void Editor::setzoom1()
{
    scribbleArea->zoom1();
}
void Editor::rotatecw()
{
    scribbleArea->rotatecw();
}

void Editor::rotateacw()
{
    scribbleArea->rotateacw();
}

void Editor::gridview()
{
    hand_clicked();
//	scribbleArea->setBackgroundBrush(QPixmap(":/images/cheese.jpg"));
    //scene->addPixmap(QPixmap(":/images/grid.jpg"));TODO GraphicsScene changes the background
    /*	QGraphicsScene* scene = new QGraphicsScene();
    	    scene->setSceneRect(QRectF(0, 0, 500, 500));
    	    scene->addPixmap(QPixmap(":/images/grid.jpg"));*/

    /*	QPainter painter(&this);
    	painter.setWorldMatrixEnabled(true);
    		painter.setWorldMatrix(  centralView.inverted() * transMatrix * centralView  );
    		painter.drawPixmap( QPoint(0,0), canvas );
    		painter.drawImage(QPoint(100,100),QImage(":background/grid"));//TODO The grid is being drawn but the white background over rides it!*/

    scribbleArea->grid();
    QMessageBox msgBox;
    msgBox.setText("Would you like to add a camera layer?");
    msgBox.exec();



}

void Editor::print()
{
    QPrinter printer(QPrinter::HighResolution);
    //printer.setOrientation(QPrinter::Landscape);
    //printer.setFullPage(false);
    //printer->setPaperSize(QPrinter::A4);

    QPrintPreviewDialog printPreviewDialog(&printer,this);
    connect(&printPreviewDialog, SIGNAL(paintRequested(QPrinter*)), this, SLOT(printAndPreview(QPrinter*)));
    if (printPreviewDialog.exec() == QDialog::Accepted)
    {
        if (!printer.isValid())
        {
            QMessageBox msg;
            msg.setText("An invalid printer was selected. The print job will now abort.");
            msg.setIcon(QMessageBox::Warning);
            msg.exec();
            return;
        }

        printAndPreview(&printer);
    }
}

void Editor::printAndPreview(QPrinter* printer)
{
    QRect exportRect = scribbleArea->rect();
    QSize exportSize = exportRect.size();
    if (printer->outputFileName() != "")
    {
        QPrinter pdfPrinter(QPrinter::ScreenResolution);
        pdfPrinter.setOutputFileName(printer->outputFileName());
        pdfPrinter.setOutputFormat(QPrinter::PdfFormat);
        pdfPrinter.setOrientation(printer->orientation());
        QPainter painter(&pdfPrinter);
        painter.setRenderHint(QPainter::HighQualityAntialiasing);
        QRect pageRect = pdfPrinter.pageRect();
        pageRect.moveTo(0, 0);
        qDebug() << "page:" << pageRect.width() << "x" << pageRect.height();
        qDebug() << "image:" << exportRect.width() << "x" << exportRect.height();
        if (exportSize.width() >= exportSize.height())
        {
            // landscape
        }
        else
        {
            // portrait
        }
        //exportSize.scale(pageRect.size(), Qt::KeepAspectRatio);
        //exportRect.setSize(exportSize);
        painter.setViewport(pageRect);
        painter.setWindow(exportRect);
        scribbleArea->render(&painter);
        painter.end();
    }
    else
    {
        QRect pageRect = printer->pageRect();
        pageRect.moveTo(0, 0);
        exportSize.scale(pageRect.size(), Qt::KeepAspectRatio);
        exportRect.setSize(exportSize);
        QPainter painter(printer);
        painter.setViewport(pageRect);
        painter.setWindow(exportRect);
        scribbleArea->render( &painter );
        painter.end();
    }
}

void Editor::getCameraLayer()
{

    for(int i=0; i < object->getLayerCount(); i++)
    {
        Layer* layer = object->getLayer(i);
        // paints the bitmap images
        if(layer->type == Layer::BITMAP)
        {
            nextLayer();
        }
        if(layer->type == Layer::VECTOR)
        {
            nextLayer();
        }
        if(layer->type == Layer::CAMERA)
        {

        }
        /*if(i=object->getLayerCount()) {
        QMessageBox msgBox;
         msgBox.setText("Would you like to add a camera layer?");
         msgBox.exec();
        }*/
    }
}
/*
	Layer* layer = getCurrentLayer(); if(layer == NULL) return;
		if(layer->type == Layer::CAMERA) {
			QMessageBox msgBox;
					 msgBox.setText("This is the Preview Layer");
					 msgBox.exec();
					 for(int i=0; i < object->getLayerCount(); i++);
					 			qDebug()<< object->getLayerCount();
		} else{
//			for (int i = 0; i < a; ++i){
			for(int a=0;a< object->getLayerCount(); a++);{
				object->getLayer(i);

	//		viewRect = QRect( QPoint(-320,-240), QSize(640,480) );
			nextLayer();
			nextLayer();
			if(layer->type == Layer::CAMERA) {
						QMessageBox msgBox;
								 msgBox.setText("This is the Preview Layer");
								 msgBox.exec();
				//				 a=object->getLayerCount();
			}
			}
		}

	}*/

/*void Editor::onionNext(bool state){
        scribbleArea->onionNextSlot(state);
//	toolSet->nextClick(); TODO right variable to click button.  it's only a signal

}

void Editor::onionPrev(bool state){
        scribbleArea->onionPrevSlot(state);
//	toolSet->prevClick();

}*/

void Editor::endPlay()
{
    int a =frameList.count();
    QListIterator<int> z( frameList );
    for (int i = 0; i < a; ++i)
    {
        int b = z.next();
        scrubTo(b);
    }


}


void Editor::startPlay()
{
//scrubTo(1);

    QListIterator<int> z( frameList );
    for (int i = 0; i < 1; ++i)
    {
        int b = z.next();
        scrubTo(b);
    }
}

void Editor::saveSvg()
{
    QString newPath = QFileDialog::getSaveFileName(this, tr("Save SVG"),
                      path, tr("SVG files (*.svg)"));

    if (newPath.isEmpty())
        return;

    path = newPath;

    QSvgGenerator generator;
    generator.setFileName(path);
    generator.setSize(QSize(700, 500));
    generator.setViewBox(QRect(0, 0, 700, 500));
    generator.setTitle(tr("SVG Generator Example Drawing"));
    generator.setDescription(tr("An SVG drawing created by the SVG Generator "
                                "Example provided with Qt."));

    QPainter painter;
    painter.begin(&generator);
    Layer* layer = object->getLayer(currentLayer);
    if(layer != NULL)
    {
        if(layer->type == Layer::VECTOR)
        {
            scribbleArea->selectAll();
            //VectorImage* vectorImage = ((LayerVector*)layer)->getLastVectorImageAtFrame(currentFrame, 0);
            scribbleArea->render(&painter);
        }
    }
    painter.end();
}

void Editor::addcolorbutton()
{

    QColor initialColour = Qt::white;
    int currentColourIndex = palette->currentColour();
    /*		  if( currentColourIndex > -1 ) {
    				initialColour = object->getColour(currentColourIndex).colour;
    			}
    			bool *ok;
    			ok = new bool;
    			QRgb qrgba = QColorDialog::getRgba( initialColour.rgba(), ok, this );



    		QColor colour;*/
    initialColour = object->getColour(currentColourIndex).colour;
    QRgb qrgba = initialColour.rgba();
    object->addColour(qrgba );
    palette->updateList();
    selectColour(object->getColourCount()-1);
}
