#ifndef FSTOOL_H
#define FSTOOL_H

#include <QObject>
#include <QString>
#include <QIcon>
#include <Malachite/Painter>
#include "document.h"
#include "tabletevent.h"
#include "canvasview.h"
#include "canvas.h"
#include "layerrenderer.h"

class QGraphicsItem;

namespace PaintField
{

class ToolManager;

/**
 * The Tool object delegates editing of a layer in each CanvasView.
 * It is owned by a CanvasView and recreated whenever the user change the current tool or a new canvas is created.
 */
class Tool : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QCursor cursor READ cursor)
	
public:
	
	explicit Tool(Canvas *parent = 0);
	~Tool();
	
	/**
	 * @return The document's current layer index
	 */
	QModelIndex currentLayerIndex() { return selectionModel()->currentIndex(); }
	
	/**
	 * @return The document's current layer
	 */
	const Layer *currentLayer() { return layerModel()->layerForIndex(currentLayerIndex()); }
	
	/**
	 * @return A graphics item which is displayed on top of the canvas
	 */
	virtual QGraphicsItem *graphicsItem() { return 0; }
	
	/**
	 * Draws a layer onto a painter.
	 * You do not need to apply opacity, blend mode or filters in this function when you override it.
	 * @param painter
	 * @param layer
	 */
	virtual void drawLayer(Malachite::SurfacePainter *painter, const Layer *layer) { Q_UNUSED(painter) Q_UNUSED(layer) }
	
	/**
	 * Adds a new layer insertion which is performed on rendering.
	 * The Tool takes ownership of "layer".
	 * @param insertAt The layer is inserted above this layer
	 * @param layer The inserted layer
	 */
	void addLayerInsertion(const Layer *insertAt, const Layer *layer);
	
	void clearLayerInsertions();
	
	QHash<const Layer *, const Layer *> layerInsertions() const;
	
	/**
	 * Adds a layer delegation which is performed on rendering.
	 * Tool::drawLayer is called instead of the default layer drawing function of canvas, when the layer is going to be rendered.
	 * @param layer
	 */
	void addLayerDelegation(const Layer *layer);
	
	void clearLayerDelegation();
	
	LayerConstList layerDelegations() const;
	
	QCursor cursor() const;
	
	virtual void mouseMoveEvent(CanvasMouseEvent *event) { event->ignore(); return; }
	virtual void mousePressEvent(CanvasMouseEvent *event) { event->ignore(); return; }
	virtual void mouseReleaseEvent(CanvasMouseEvent *event) { event->ignore(); return; }
	virtual void mouseDoubleClickEvent(CanvasMouseEvent *event) { event->ignore(); return; }
	
	virtual void tabletMoveEvent(CanvasTabletEvent *event) { event->ignore(); return; }
	virtual void tabletPressEvent(CanvasTabletEvent *event) { event->ignore(); return; }
	virtual void tabletReleaseEvent(CanvasTabletEvent *event) { event->ignore(); return; }
	
	virtual void keyPressEvent(QKeyEvent *event) { event->ignore(); return; }
	virtual void keyReleaseEvent(QKeyEvent *event) { event->ignore(); return; }
	
	virtual void toolEvent(QEvent *event);
	
public slots:
	
signals:
	
	void requestUpdate(const QPointSet &tiles);
	void requestUpdate(const QHash<QPoint, QRect> &rects);
	
protected:
	
	void setCursor(const QCursor &cursor);
	
	Canvas *canvas() { return static_cast<Canvas *>(parent()); }
	Document *document() { return canvas()->document(); }
	LayerModel *layerModel() { return document()->layerModel(); }
	QItemSelectionModel *selectionModel() { return canvas()->selectionModel(); }
	
private:
	
	struct Data;
	Data *d;
};

}

#endif // FSTOOL_H
