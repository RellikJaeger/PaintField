#pragma once

#include "paintfield/core/tool.h"
#include "paintfield/core/rectlayer.h"

namespace PaintField {

class RectTool : public Tool
{
	Q_OBJECT
	
public:
	
	enum AddingType
	{
		NoAdding,
		AddRect,
		AddEllipse,
		AddText
	};
	
	explicit RectTool(AddingType type, Canvas *canvas);
	~RectTool();
	
	void drawLayer(Malachite::SurfacePainter *painter, const Layer *layer) override;
	
protected:
	
	void tabletMoveEvent(CanvasTabletEvent *event) override;
	void tabletPressEvent(CanvasTabletEvent *event) override;
	void tabletReleaseEvent(CanvasTabletEvent *event) override;
	
private slots:
	
	void onCurrentChanged(const LayerRef &layer);
	void onLayerPropertyChanged(const LayerRef &layer);
	void moveHandles();
	
private:
	
	enum HandleType
	{
		Left = 1,
		Right = 1 << 1,
		Top = 1 << 2,
		Bottom = 1 << 3
	};
	
	enum Mode
	{
		NoOperation,
		Dragging,
		Inserting
	};
	
	void onHandleMoved(const QPointF &pos, int handleTypes);
	void onHandleMoveFinished();
	
	void addHandle(int handleTypes, qreal zValue);
	
	friend class RectInserter;
	friend class RectHandleItem;
	
	struct Data;
	Data *d;
};

} // namespace PaintField
