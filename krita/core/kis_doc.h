/*
 *  Copyright (c) 1999-2000 Matthias Elter  <me@kde.org>
 *  Copyright (c) 2001 Toshitaka Fujioka  <fujioka@kde.org>
 *  Copyright (c) 2002 Patrick Julien <freak@codepimps.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#if !defined KIS_DOC_H_
#define KIS_DOC_H_

#include <koDocument.h>
#include "kis_global.h"
#include "kis_types.h"
#include "kis_image.h"

class QImage;
class QString;
class DCOPObject;
class KCommand;
class KCommandHistory;
class KMacroCommand;
class KisView;
class KisNameServer;

class KisDoc : public KoDocument {
	typedef KoDocument super;
	Q_OBJECT

public:
	KisDoc(QWidget *parentWidget = 0, const char *widgetName = 0, QObject* parent = 0, const char* name = 0, bool singleViewMode = false);
	virtual ~KisDoc();

public:
	// Overide KoDocument
	virtual bool completeLoading(KoStore *store);
	virtual bool completeSaving(KoStore*);
	virtual DCOPObject* dcopObject();
	virtual bool initDoc();
	virtual bool isEmpty() const;
	virtual bool loadXML(QIODevice *, const QDomDocument& doc);
	virtual QCString mimeType() const;
	virtual void paintContent(QPainter& painter, const QRect& rect, bool transparent = false, double zoomX = 1.0, double zoomY = 1.0);
	virtual QDomDocument saveXML();

public:
	KisLayerSP layerAdd(KisImageSP img, Q_INT32 width, Q_INT32 height, const QString& name, QUANTUM devOpacity);
	KisLayerSP layerAdd(KisImageSP img, const QString& name, KisSelectionSP selection);
	KisLayerSP layerAdd(KisImageSP img, KisLayerSP layer, Q_INT32 position);
	void layerRemove(KisImageSP img, KisLayerSP layer);
	void layerRaise(KisImageSP img, KisLayerSP layer);
	void layerLower(KisImageSP img, KisLayerSP layer);
	void layerNext(KisImageSP img, KisLayerSP layer);
	void layerPrev(KisImageSP img, KisLayerSP layer);
	void layerProperties(KisImageSP img, KisLayerSP layer, QUANTUM opacity, const QString& name);

	void beginMacro(const QString& macroName);
	void endMacro();
	bool inMacro() const;
	void addCommand(KCommand *cmd);
	Q_INT32 undoLimit() const;
	void setUndoLimit(Q_INT32 limit);
	Q_INT32 redoLimit() const;
	void setRedoLimit(Q_INT32 limit);
	void setUndo(bool undo);
	bool undo() const;

	KisImageSP newImage(const QString& name, Q_INT32 width, Q_INT32 height, enumImgType type);
	void addImage(KisImageSP img);
	void removeImage(KisImageSP img);
	void removeImage(const QString& name);

	KisImageSP imageNum(Q_UINT32 num) const;
	Q_INT32 nimages() const;
	Q_INT32 imageIndex(KisImageSP img) const;
	KisImageSP findImage(const QString& name) const;
	bool contains(KisImageSP img) const;

	bool empty() const;
	QStringList images();
	void renameImage(const QString& oldName, const QString& newName);
	bool namePresent(const QString& name) const;

	QString nextImageName() const;
	void setProjection(KisImageSP img);

	void setClipboardSelection(KisSelectionSP selection);
	KisSelectionSP clipboardSelection();

	bool importImage(const QString& filename);

public slots:
	void slotImageUpdated();
	void slotImageUpdated(const QRect& rect);
	bool slotNewImage();
	void slotDocumentRestored();
	void slotCommandExecuted();

signals:
	void docUpdated();
	void docUpdated(const QRect& rect);
	void imageListUpdated();
	void layersUpdated(KisImageSP img);
	void projectionUpdated(KisImageSP img);
	void ioProgress(Q_INT8 percentage);
	void ioSteps(Q_INT32 steps);
	void ioCompletedStep();
	void ioDone();

protected:
	// Overide KoDocument
	virtual KoView* createViewInstance(QWidget *parent, const char *name);

private slots:
	void slotUpdate(KisImageSP img, Q_UINT32 x, Q_UINT32 y, Q_UINT32 w, Q_UINT32 h);
	void clipboardDataChanged();
	void slotIOProgress(Q_INT8 percentage);

private:
	QDomElement saveImage(QDomDocument& doc, KisImageSP img);
	KisImageSP loadImage(const QDomElement& elem);
	QDomElement saveLayer(QDomDocument& doc, KisLayerSP layer);
	KisLayerSP loadLayer(const QDomElement& elem, KisImageSP img);
	QDomElement saveChannel(QDomDocument& doc, KisChannelSP channel);
	KisChannelSP loadChannel(const QDomElement& elem, KisImageSP img);
	bool init();
	void setupColorspaces();

private:
	bool m_undo;
	KCommandHistory *m_cmdHistory;
	vKisImageSP m_images;
	KisImageSP m_projection;
	KisSelectionSP m_clipboard;
	bool m_pushedClipboard;
	DCOPObject *m_dcop;
	KisNameServer *m_nserver;
	KMacroCommand *m_currentMacro;
	Q_INT32 m_conversionDepth;
	KisStrategyColorSpaceMap m_colorspaces;
};

#endif // KIS_DOC_H_

