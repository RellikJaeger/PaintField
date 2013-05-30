#pragma once

#include <QRect>
#include <Malachite/Surface>
#include <QImage>
#include <QPainter>
#include "global.h"

namespace PaintField
{

struct CanvasViewportTileTraits
{
	static constexpr int tileWidth() { return Malachite::Surface::tileWidth(); }
	static Malachite::ImageU8::PixelType defaultPixel() { return Malachite::ImageU8::PixelType(128, 128, 128, 255); }
};

typedef Malachite::GenericSurface<Malachite::ImageU8, CanvasViewportTileTraits> CanvasViewportSurface;

struct CanvasViewportState
{
	QSize documentSize;
	
	CanvasViewportSurface surface;
	
	QTransform transformToScene, transformToView;
	
	bool translationOnly = false;
	QPoint translationToScene;
	
	bool retinaMode = false;
	
	bool cacheAvailable = false;
	QRect cacheRect;
	Malachite::ImageU8 cacheImage;
};

template <typename TFunction>
void drawDivided(const QRect &viewRect, const TFunction &drawFunc)
{
	constexpr auto unit = 128;
	
	if (viewRect.width() * viewRect.height() <= unit * unit)
	{
		drawFunc(viewRect);
	}
	else
	{
		int xCount = viewRect.width() / unit;
		if (viewRect.width() % unit)
			xCount++;
		
		int yCount = viewRect.height() / unit;
		if (viewRect.height() % unit)
			yCount++;
		
		for (int x = 0; x < xCount; ++x)
		{
			for (int y = 0; y < yCount; ++y)
			{
				auto viewRectDivided = viewRect & QRect(viewRect.topLeft() + QPoint(x, y) * unit, QSize(unit, unit));
				drawFunc(viewRectDivided);
			}
		}
	}
}

template <typename TDrawImageFunction, typename TDrawBackgroundFunction>
void drawViewport(const QRect &windowRepaintRect, CanvasViewportState *state, const TDrawImageFunction &drawImage, const TDrawBackgroundFunction &drawBackground)
{
	bool retinaMode = state->retinaMode;
	
	auto fromWindowRect = [retinaMode](const QRect &rect)->QRect
	{
		if (retinaMode)
			return QRect(rect.left() * 2, rect.top() * 2, rect.width() * 2, rect.height() * 2);
		else
			return rect;
	};
	
	auto toWindowRect = [retinaMode](const QRect &rect)->QRect
	{
		if (retinaMode)
			return QRect(rect.left() / 2, rect.top() / 2, rect.width() / 2, rect.height() / 2);
		else
			return rect;
	};
	
	auto repaintRect = fromWindowRect(windowRepaintRect);
	
	auto cropSurface = [&](const QRect &rect)
	{
		if (state->cacheAvailable && state->cacheRect == rect)
			return state->cacheImage;
		else
			return state->surface.crop(rect);
	};
	
	if (state->translationOnly) // easy, view is only translated
	{
		auto drawInViewRect = [&](const QRect &viewRect)
		{
			auto sceneRect = viewRect.translated(state->translationToScene);
			
			if ((sceneRect & QRect(QPoint(), state->documentSize)).isEmpty())
				drawBackground(toWindowRect(viewRect));
			else
				drawImage(toWindowRect(viewRect), cropSurface(sceneRect).wrapInQImage());
		};
		
		drawDivided(repaintRect, drawInViewRect);
	}
	else
	{
		auto drawInViewRect = [&](const QRect &viewRect)
		{
			auto sceneRect = state->transformToScene.mapRect(QRectF(viewRect)).toAlignedRect();
			auto croppedImage = cropSurface(sceneRect);
			
			QImage image(viewRect.size(), QImage::Format_ARGB32_Premultiplied);
			{
				QPainter imagePainter(&image);
				imagePainter.setCompositionMode(QPainter::CompositionMode_Source);
				imagePainter.setRenderHint(QPainter::SmoothPixmapTransform);
				imagePainter.setTransform( state->transformToView * QTransform::fromTranslate(-viewRect.left(), -viewRect.top()) );
				imagePainter.drawImage(sceneRect.topLeft(), croppedImage.wrapInQImage());
			}
			
			if ((sceneRect & QRect(QPoint(), state->documentSize)).isEmpty())
				drawBackground(toWindowRect(viewRect));
			else
				drawImage(toWindowRect(viewRect), image);
		};
		
		drawDivided(repaintRect, drawInViewRect);
	}
}

}
