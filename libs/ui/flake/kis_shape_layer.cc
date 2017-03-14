/*
 *  Copyright (c) 2006-2008 Boudewijn Rempt <boud@valdyas.org>
 *  Copyright (c) 2007 Thomas Zander <zander@kde.org>
 *  Copyright (c) 2009 Cyrille Berger <cberger@cberger.net>
 *  Copyright (c) 2011 Jan Hambrecht <jaham@gmx.net>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kis_shape_layer.h"

#include <QPainter>
#include <QPainterPath>
#include <QRect>
#include <QDomElement>
#include <QDomDocument>
#include <QString>
#include <QList>
#include <QMap>
#include <kis_debug.h>
#include <kundo2command.h>
#include <commands_new/kis_node_move_command2.h>
#include <QMimeData>

#include <QTemporaryFile>
#include <kis_debug.h>

#include <kis_icon.h>
#include <KoColorSpace.h>
#include <KoCompositeOp.h>
#include <KisDocument.h>
#include <KoUnit.h>
#include <KoOdf.h>
#include <KoOdfReadStore.h>
#include <KoOdfStylesReader.h>
#include <KoOdfLoadingContext.h>
#include <KoPageLayout.h>
#include <KoShapeContainer.h>
#include <KoShapeLayer.h>
#include <KoShapeGroup.h>
#include <KoShapeLoadingContext.h>
#include <KoShapeManager.h>
#include <KoSelectedShapesProxy.h>
#include <KoShapeRegistry.h>
#include <KoShapeSavingContext.h>
#include <KoStore.h>
#include <KoShapeBasedDocumentBase.h>
#include <KoStoreDevice.h>
#include <KoViewConverter.h>
#include <KoXmlNS.h>
#include <KoXmlReader.h>
#include <KoXmlWriter.h>
#include <KoSelection.h>
#include <KoShapeMoveCommand.h>
#include <KoShapeTransformCommand.h>
#include <KoShapeShadow.h>
#include <KoShapeShadowCommand.h>

#include <kis_types.h>
#include <kis_image.h>
#include "kis_default_bounds.h"
#include <kis_paint_device.h>
#include "kis_shape_layer_canvas.h"
#include "kis_image_view_converter.h"
#include <kis_painter.h>
#include "kis_node_visitor.h"
#include "kis_processing_visitor.h"
#include "kis_effect_mask.h"

#include <SimpleShapeContainerModel.h>
class ShapeLayerContainerModel : public SimpleShapeContainerModel
{
public:
    ShapeLayerContainerModel(KisShapeLayer *parent)
        : q(parent)
{}

    void add(KoShape *child) override {
        SimpleShapeContainerModel::add(child);
    }

    void remove(KoShape *child) override {
        SimpleShapeContainerModel::remove(child);
    }

    void shapeHasBeenAddedToHierarchy(KoShape *shape, KoShapeContainer *addedToSubtree) override {
        q->shapeManager()->addShape(shape);
        SimpleShapeContainerModel::shapeHasBeenAddedToHierarchy(shape, addedToSubtree);
    }

    void shapeToBeRemovedFromHierarchy(KoShape *shape, KoShapeContainer *removedFromSubtree) override {
        q->shapeManager()->remove(shape);
        SimpleShapeContainerModel::shapeToBeRemovedFromHierarchy(shape, removedFromSubtree);
    }

private:
    KisShapeLayer *q;
};


struct KisShapeLayer::Private
{
public:
    Private()
        : canvas(0)
        , controller(0)
        , x(0)
        , y(0)
         {}

    KisPaintDeviceSP paintDevice;
    KisShapeLayerCanvas * canvas;
    KoShapeBasedDocumentBase* controller;
    int x;
    int y;
};


KisShapeLayer::KisShapeLayer(KoShapeBasedDocumentBase* controller,
                             KisImageWSP image,
                             const QString &name,
                             quint8 opacity)
    : KisExternalLayer(image, name, opacity),
      KoShapeLayer(new ShapeLayerContainerModel(this)),
      m_d(new Private())
{
    initShapeLayer(controller);
}

KisShapeLayer::KisShapeLayer(const KisShapeLayer& rhs)
    : KisShapeLayer(rhs, rhs.m_d->controller)
{
}

KisShapeLayer::KisShapeLayer(const KisShapeLayer& _rhs, KoShapeBasedDocumentBase* controller)
        : KisExternalLayer(_rhs)
        , KoShapeLayer(new ShapeLayerContainerModel(this)) //no _rhs here otherwise both layer have the same KoShapeContainerModel
        , m_d(new Private())
{
    // Make sure our new layer is visible otherwise the shapes cannot be painted.
    setVisible(true);

    initShapeLayer(controller);

    Q_FOREACH (KoShape *shape, _rhs.shapes()) {
        addShape(shape->cloneShape());
    }
}

KisShapeLayer::KisShapeLayer(const KisShapeLayer& _rhs, const KisShapeLayer &_addShapes)
        : KisExternalLayer(_rhs)
        , KoShapeLayer(new ShapeLayerContainerModel(this)) //no _merge here otherwise both layer have the same KoShapeContainerModel
        , m_d(new Private())
{
    // Make sure our new layer is visible otherwise the shapes cannot be painted.
    setVisible(true);

    initShapeLayer(_rhs.m_d->controller);

    // copy in _rhs's shapes
    Q_FOREACH (KoShape *shape, _rhs.shapes()) {
        addShape(shape->cloneShape());
    }

    // copy in _addShapes's shapes
    Q_FOREACH (KoShape *shape, _addShapes.shapes()) {
        addShape(shape->cloneShape());
    }
}

KisShapeLayer::~KisShapeLayer()
{
    /**
     * Small hack alert: we should avoid updates on shape deletion
     */
    m_d->canvas->prepareForDestroying();

    Q_FOREACH (KoShape *shape, shapes()) {
        shape->setParent(0);
        delete shape;
    }

    delete m_d->canvas;
    delete m_d;
}

void KisShapeLayer::initShapeLayer(KoShapeBasedDocumentBase* controller)
{
    setSupportsLodMoves(false);
    setShapeId(KIS_SHAPE_LAYER_ID);

    KIS_ASSERT_RECOVER_NOOP(this->image());
    m_d->paintDevice = new KisPaintDevice(image()->colorSpace());
    m_d->paintDevice->setDefaultBounds(new KisDefaultBounds(this->image()));
    m_d->paintDevice->setParentNode(this);

    m_d->canvas = new KisShapeLayerCanvas(this, image());
    m_d->canvas->setProjection(m_d->paintDevice);
    m_d->canvas->moveToThread(this->thread());
    m_d->controller = controller;

    m_d->canvas->shapeManager()->selection()->disconnect(this);

    connect(m_d->canvas->selectedShapesProxy(), SIGNAL(selectionChanged()), this, SIGNAL(selectionChanged()));
    connect(m_d->canvas->selectedShapesProxy(), SIGNAL(currentLayerChanged(const KoShapeLayer*)),
            this, SIGNAL(currentLayerChanged(const KoShapeLayer*)));

    connect(this, SIGNAL(sigMoveShapes(const QPointF&)), SLOT(slotMoveShapes(const QPointF&)));
}

bool KisShapeLayer::allowAsChild(KisNodeSP node) const
{
    return node->inherits("KisMask");
}

void KisShapeLayer::setImage(KisImageWSP _image)
{
    KisLayer::setImage(_image);
    m_d->canvas->setImage(_image);
    m_d->paintDevice->convertTo(_image->colorSpace());
    m_d->paintDevice->setDefaultBounds(new KisDefaultBounds(_image));
}

KisLayerSP KisShapeLayer::createMergedLayerTemplate(KisLayerSP prevLayer)
{
    KisShapeLayer *prevShape = dynamic_cast<KisShapeLayer*>(prevLayer.data());

    if (prevShape)
        return new KisShapeLayer(*prevShape, *this);
    else
        return KisExternalLayer::createMergedLayerTemplate(prevLayer);
}

void KisShapeLayer::fillMergedLayerTemplate(KisLayerSP dstLayer, KisLayerSP prevLayer)
{
    if (!dynamic_cast<KisShapeLayer*>(dstLayer.data())) {
        KisLayer::fillMergedLayerTemplate(dstLayer, prevLayer);
    }
}

void KisShapeLayer::setParent(KoShapeContainer *parent)
{
    Q_UNUSED(parent)
    KIS_ASSERT_RECOVER_RETURN(0)
}

QIcon KisShapeLayer::icon() const
{
    return KisIconUtils::loadIcon("vectorLayer");
}

KisPaintDeviceSP KisShapeLayer::original() const
{
    return m_d->paintDevice;
}

KisPaintDeviceSP KisShapeLayer::paintDevice() const
{
    return 0;
}

qint32 KisShapeLayer::x() const
{
    return m_d->x;
}

qint32 KisShapeLayer::y() const
{
    return m_d->y;
}

void KisShapeLayer::setX(qint32 x)
{
    qint32 delta = x - this->x();
    QPointF diff = QPointF(m_d->canvas->viewConverter()->viewToDocumentX(delta), 0);
    emit sigMoveShapes(diff);

    // Save new value to satisfy LSP
    m_d->x = x;
}

void KisShapeLayer::setY(qint32 y)
{
    qint32 delta = y - this->y();
    QPointF diff = QPointF(0, m_d->canvas->viewConverter()->viewToDocumentY(delta));
    emit sigMoveShapes(diff);

    // Save new value to satisfy LSP
    m_d->y = y;
}

void KisShapeLayer::slotMoveShapes(const QPointF &diff)
{
    QList<QPointF> prevPos;
    QList<QPointF> newPos;

    QList<KoShape*> shapes;
    Q_FOREACH (KoShape* shape, shapeManager()->shapes()) {
        if (!dynamic_cast<KoShapeGroup*>(shape)) {
            shapes.append(shape);
        }
    }
    Q_FOREACH (KoShape* shape, shapes) {
        QPointF pos = shape->position();
        prevPos << pos;
        newPos << pos + diff;
    }

    KoShapeMoveCommand cmd(shapes, prevPos, newPos);
    cmd.redo();
}

bool KisShapeLayer::accept(KisNodeVisitor& visitor)
{
    return visitor.visit(this);
}

void KisShapeLayer::accept(KisProcessingVisitor &visitor, KisUndoAdapter *undoAdapter)
{
    return visitor.visit(this, undoAdapter);
}

KoShapeManager* KisShapeLayer::shapeManager() const
{
    return m_d->canvas->shapeManager();
}

KoViewConverter* KisShapeLayer::converter() const
{
    return m_d->canvas->viewConverter();
}

bool KisShapeLayer::visible(bool recursive) const
{
    return KisExternalLayer::visible(recursive);
}

void KisShapeLayer::setVisible(bool visible, bool isLoading)
{
    KisExternalLayer::setVisible(visible, isLoading);
}

#include "SvgWriter.h"
#include "SvgParser.h"
#include <QXmlStreamReader>

bool KisShapeLayer::saveShapesToStore(KoStore *store, QList<KoShape *> shapes, const QSizeF &sizeInPt)
{
    if (!store->open("content.svg")) {
        return false;
    }

    KoStoreDevice storeDev(store);
    storeDev.open(QIODevice::WriteOnly);

    qSort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);

    SvgWriter writer(shapes, sizeInPt);
    writer.save(storeDev);

    if (!store->close()) {
        return false;
    }

    return true;
}

QList<KoShape *> KisShapeLayer::createShapesFromSvg(QIODevice *device, const QString &baseXmlDir, const QRectF &rectInPixels, qreal resolutionPPI, KoDocumentResourceManager *resourceManager, QSizeF *fragmentSize)
{
    QXmlStreamReader reader(device);
    reader.setNamespaceProcessing(false);

    QString errorMsg;
    int errorLine = 0;
    int errorColumn;

    KoXmlDocument doc;
    bool ok = doc.setContent(&reader, &errorMsg, &errorLine, &errorColumn);
    if (!ok) {
        errKrita << "Parsing error in " << "contents.svg" << "! Aborting!" << endl
        << " In line: " << errorLine << ", column: " << errorColumn << endl
        << " Error message: " << errorMsg << endl;
        errKrita << i18n("Parsing error in the main document at line %1, column %2\nError message: %3"
                         , errorLine , errorColumn , errorMsg);
    }

    SvgParser parser(resourceManager);
    parser.setXmlBaseDir(baseXmlDir);
    parser.setResolution(rectInPixels /* px */, resolutionPPI /* ppi */);
    return parser.parseSvg(doc.documentElement(), fragmentSize);
}


bool KisShapeLayer::saveLayer(KoStore * store) const
{
    // FIXME: we handle xRes() only!

    const QSizeF sizeInPx = image()->bounds().size();
    const QSizeF sizeInPt(sizeInPx.width() / image()->xRes(), sizeInPx.height() / image()->yRes());

    return saveShapesToStore(store, this->shapes(), sizeInPt);
}

bool KisShapeLayer::loadSvg(QIODevice *device, const QString &baseXmlDir)
{
    QSizeF fragmentSize; // unused!
    KisImageSP image = this->image();

    // FIXME: we handle xRes() only!
    KIS_SAFE_ASSERT_RECOVER_NOOP(qFuzzyCompare(image->xRes(), image->yRes()));
    const qreal resolutionPPI = 72.0 * image->xRes();

    QList<KoShape*> shapes =
        createShapesFromSvg(device, baseXmlDir,
                            image->bounds(), resolutionPPI,
                            m_d->controller->resourceManager(),
                            &fragmentSize);

    Q_FOREACH (KoShape *shape, shapes) {
        addShape(shape);
    }

    return true;
}

bool KisShapeLayer::loadLayer(KoStore* store)
{
    if (!store) {
        warnKrita << i18n("No store backend");
        return false;
    }

    if (store->open("content.svg")) {
        KoStoreDevice storeDev(store);
        storeDev.open(QIODevice::ReadOnly);

        loadSvg(&storeDev, "");

        store->close();

        return true;
    }

    KoOdfReadStore odfStore(store);
    QString errorMessage;

    odfStore.loadAndParse(errorMessage);

    if (!errorMessage.isEmpty()) {
        warnKrita << errorMessage;
        return false;
    }

    KoXmlElement contents = odfStore.contentDoc().documentElement();

    //    dbgKrita <<"Start loading OASIS document..." << contents.text();
    //    dbgKrita <<"Start loading OASIS contents..." << contents.lastChild().localName();
    //    dbgKrita <<"Start loading OASIS contents..." << contents.lastChild().namespaceURI();
    //    dbgKrita <<"Start loading OASIS contents..." << contents.lastChild().isElement();

    KoXmlElement body(KoXml::namedItemNS(contents, KoXmlNS::office, "body"));

    if (body.isNull()) {
        //setErrorMessage( i18n( "Invalid OASIS document. No office:body tag found." ) );
        return false;
    }

    body = KoXml::namedItemNS(body, KoXmlNS::office, "drawing");
    if (body.isNull()) {
        //setErrorMessage( i18n( "Invalid OASIS document. No office:drawing tag found." ) );
        return false;
    }

    KoXmlElement page(KoXml::namedItemNS(body, KoXmlNS::draw, "page"));
    if (page.isNull()) {
        //setErrorMessage( i18n( "Invalid OASIS document. No draw:page tag found." ) );
        return false;
    }

    KoXmlElement * master = 0;
    if (odfStore.styles().masterPages().contains("Standard"))
        master = odfStore.styles().masterPages().value("Standard");
    else if (odfStore.styles().masterPages().contains("Default"))
        master = odfStore.styles().masterPages().value("Default");
    else if (! odfStore.styles().masterPages().empty())
        master = odfStore.styles().masterPages().begin().value();

    if (master) {
        const KoXmlElement *style = odfStore.styles().findStyle(
                                        master->attributeNS(KoXmlNS::style, "page-layout-name", QString()));
        KoPageLayout pageLayout;
        pageLayout.loadOdf(*style);
        setSize(QSizeF(pageLayout.width, pageLayout.height));
    }
    // We work fine without a master page

    KoOdfLoadingContext context(odfStore.styles(), odfStore.store());
    context.setManifestFile(QString("tar:/") + odfStore.store()->currentPath() + "META-INF/manifest.xml");
    KoShapeLoadingContext shapeContext(context, m_d->controller->resourceManager());


    KoXmlElement layerElement;
    forEachElement(layerElement, context.stylesReader().layerSet()) {
        // FIXME: investigate what is this
        //        KoShapeLayer * l = new KoShapeLayer();
        if (!loadOdf(layerElement, shapeContext)) {
            dbgKrita << "Could not load vector layer!";
            return false;
        }
    }

    KoXmlElement child;
    forEachElement(child, page) {
        KoShape * shape = KoShapeRegistry::instance()->createShapeFromOdf(child, shapeContext);
        if (shape) {
            addShape(shape);
        }
    }

    return true;

}

void KisShapeLayer::resetCache()
{
    m_d->paintDevice->clear();

    QList<KoShape*> shapes = m_d->canvas->shapeManager()->shapes();
    Q_FOREACH (const KoShape* shape, shapes) {
        shape->update();
    }
}

KUndo2Command* KisShapeLayer::crop(const QRect & rect) {
    QPoint oldPos(x(), y());
    QPoint newPos = oldPos - rect.topLeft();

    return new KisNodeMoveCommand2(this, oldPos, newPos);
}

KUndo2Command* KisShapeLayer::transform(const QTransform &transform) {
    QList<KoShape*> shapes = m_d->canvas->shapeManager()->shapes();
    if(shapes.isEmpty()) return 0;

    KisImageViewConverter *converter = dynamic_cast<KisImageViewConverter*>(this->converter());
    QTransform realTransform = converter->documentToView() *
        transform * converter->viewToDocument();

    QList<QTransform> oldTransformations;
    QList<QTransform> newTransformations;

    QList<KoShapeShadow*> newShadows;
    const qreal transformBaseScale = KoUnit::approxTransformScale(transform);


    // this code won't work if there are shapes, that inherit the transformation from the parent container.
    // the chart and tree shapes are examples for that, but they aren't used in krita and there are no other shapes like that.
    Q_FOREACH (const KoShape* shape, shapes) {
        QTransform oldTransform = shape->transformation();
        oldTransformations.append(oldTransform);
        if (dynamic_cast<const KoShapeGroup*>(shape)) {
            newTransformations.append(oldTransform);
        } else {
            QTransform globalTransform = shape->absoluteTransformation(0);
            QTransform localTransform = globalTransform * realTransform * globalTransform.inverted();
            newTransformations.append(localTransform*oldTransform);
        }

        KoShapeShadow *shadow = 0;

        if (shape->shadow()) {
            shadow = new KoShapeShadow(*shape->shadow());
            shadow->setOffset(transformBaseScale * shadow->offset());
            shadow->setBlur(transformBaseScale * shadow->blur());
        }

        newShadows.append(shadow);

    }

    KUndo2Command *parentCommand = new KUndo2Command();
    new KoShapeTransformCommand(shapes,
                                oldTransformations,
                                newTransformations,
                                parentCommand);

    new KoShapeShadowCommand(shapes,
                             newShadows,
                             parentCommand);

    return parentCommand;
}

