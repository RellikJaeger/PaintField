#include <QAction>
#include <QFileDialog>
#include <QDir>
#include <Malachite/ImageIO>
#include <QItemSelectionModel>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
#include <tuple>
#include <boost/range/algorithm.hpp>
#include <boost/range/adaptors.hpp>

#include "paintfield/core/cpplinq.hpp"

#include "paintfield/core/layeritemmodel.h"
#include "paintfield/core/layerscene.h"
#include "paintfield/core/rasterlayer.h"
#include "paintfield/core/grouplayer.h"
#include "paintfield/core/util.h"
#include "paintfield/core/appcontroller.h"
#include "paintfield/core/settingsmanager.h"

#include "layeruicontroller.h"

namespace PaintField
{

struct LayerUIController::Data
{
	QHash<ActionType, QAction *> actions;
	Document *document = 0;
	QList<QAction *> actionsForLayers;
};

LayerUIController::LayerUIController(Document *document, QObject *parent) :
    QObject(parent),
	d(new Data)
{
	d->document = document;
	
	auto settingsManager = appController()->settingsManager();
	
	{
		auto a = Util::createAction("paintfield.layer.import", this, SLOT(importLayer()));
		a->setText(tr("Import..."));
		d->actions[ActionImport] = a;
	}
	
	{
		auto a = Util::createAction("paintfield.layer.newRaster", this, SLOT(newRasterLayer()));
		a->setText(tr("New Layer"));
		a->setShortcut(settingsManager->value({".key-bindings", "paintfield.layer.newRaster"}).toString());
		d->actions[ActionNewRaster] = a;
	}
	
	{
		auto a = Util::createAction("paintfield.layer.newGroup", this, SLOT(newGroupLayer()));
		a->setText(tr("New Group"));
		a->setShortcut(settingsManager->value({".key-bindings", "paintfield.layer.newGroup"}).toString());
		d->actions[ActionNewGroup] = a;
	}
	
	{
		auto a = Util::createAction("paintfield.layer.remove", this, SLOT(removeLayers()));
		a->setText(tr("Delete"));
		a->setShortcut(settingsManager->value({".key-bindings", "paintfield.edit.delete"}).toString());
		d->actions[ActionRemove] = a;
		d->actionsForLayers << a;
	}
	
	{
		auto a = Util::createAction("paintfield.layer.merge", this, SLOT(mergeLayers()));
		a->setText(tr("Merge"));
		a->setShortcut(settingsManager->value({".key-bindings", "paintfield.layer.merge"}).toString());
		d->actions[ActionMerge] = a;
	}
	
	{
		auto a = Util::createAction("paintfield.layer.copy", this, SLOT(copyLayers()));
		a->setText(tr("Copy"));
		a->setShortcut(settingsManager->value({".key-bindings", "paintfield.edit.copy"}).toString());
		d->actions[ActionCopy] = a;
		d->actionsForLayers << a;
	}
	
	{
		auto a = Util::createAction("paintfield.layer.cut", this, SLOT(cutLayers()));
		a->setText(tr("Cut"));
		a->setShortcut(settingsManager->value({".key-bindings", "paintfield.edit.cut"}).toString());
		d->actions[ActionCut] = a;
		d->actionsForLayers << a;
	}
	
	{
		auto a = Util::createAction("paintfield.layer.paste", this, SLOT(pasteLayers()));
		a->setText(tr("Paste"));
		a->setShortcut(settingsManager->value({".key-bindings", "paintfield.edit.paste"}).toString());
		d->actions[ActionPaste] = a;
	}
	
	connect(document->layerScene(), SIGNAL(selectionChanged(QList<LayerConstPtr>,QList<LayerConstPtr>)), this, SLOT(onSelectionChanged()));
	onSelectionChanged();
}

LayerUIController::~LayerUIController()
{
	delete d;
}

QAction *LayerUIController::action(ActionType type)
{
	return d->actions.value(type, 0);
}

QList<QAction *> LayerUIController::actions()
{
	return d->actions.values();
}

Document *LayerUIController::document()
{
	return d->document;
}

void LayerUIController::importLayer()
{
	QString filePath = QFileDialog::getOpenFileName(0,
													QObject::tr("Add Layer From Image File"),
													QDir::homePath(),
													QObject::tr("Image Files (*.bmp *.png *.jpg *.jpeg)"));
	if (filePath.isEmpty())
		return;
	
	auto layer = RasterLayer::createFromImageFile(filePath);
	
	if (layer)
		addLayers( { layer} , tr("Add From Image File") );
}

void LayerUIController::newRasterLayer()
{
	addLayers( { std::make_shared<RasterLayer>(tr("New Layer") ) }, tr("Add Layer") );
}

void LayerUIController::newGroupLayer()
{
	addLayers( { std::make_shared<GroupLayer>(tr("New Group") ) }, tr("Add Group") );
}

void LayerUIController::removeLayers()
{
	d->document->layerScene()->removeLayers(d->document->layerScene()->selection());
}

static std::tuple<LayerConstPtr, int , int> layerRangeFromLayers(const QList<LayerConstPtr> &layers)
{
	// this value is returned if failed
	auto defaultValue = std::make_tuple(nullptr, 0, 0);
	
	if (layers.size() == 0)
		return defaultValue;
	
	LayerConstPtr parent = layers.at(0)->parent();
	QList<int> indexes;
	
	for (auto &layer : layers)
	{
		// have different parents
		if (layer->parent() != parent)
			return defaultValue;
		
		indexes << layer->index();
	}
	
	int min = cpplinq::from(indexes) >> cpplinq::min();
	int max = cpplinq::from(indexes) >> cpplinq::max();
	int count = max - min + 1;
	
	// success if indexes are continuous
	if (cpplinq::from(indexes) >> cpplinq::distinct() >> cpplinq::count() == count)
		return std::make_tuple(parent, min, count);
	else
		return defaultValue;
}

void LayerUIController::mergeLayers()
{
	LayerConstPtr parent;
	int start, count;
	std::tie(parent, start, count) = layerRangeFromLayers(d->document->layerScene()->selection());
	
	if (count >= 2)
	{
		d->document->layerScene()->mergeLayers(parent, start, count);
		d->document->layerScene()->setCurrent(parent->child(start));
	}
}

static const QString layersMimeType = "application/x-paintfield-layers";

void LayerUIController::copyLayers()
{
	copyOrCutLayers(false);
}

void LayerUIController::cutLayers()
{
	copyOrCutLayers(true);
}

void LayerUIController::copyOrCutLayers(bool cut)
{
	auto layers = d->document->layerScene()->selection();
	if (layers.size() == 0)
		return;
	
	auto mime = new QMimeData();
	
	{
		QByteArray data;
		QDataStream stream(&data, QIODevice::WriteOnly);
		
		stream << layers.size();
		
		for (auto layer : layers)
			layer->encodeRecursive(stream);
		
		mime->setData(layersMimeType, data);
	}
	
	qApp->clipboard()->setMimeData(mime);
	
	if (cut)
		d->document->layerScene()->removeLayers(layers, tr("Cut Layers"));
}

void LayerUIController::pasteLayers()
{
	auto mime = qApp->clipboard()->mimeData();
	if (mime->hasFormat(layersMimeType))
	{
		QByteArray data = mime->data(layersMimeType);
		QDataStream stream(&data, QIODevice::ReadOnly);
		
		int count;
		stream >> count;
		
		if (count == 0)
			return;
		
		QList<LayerPtr> layers;
		layers.reserve(count);
		
		for (int i = 0; i < count; ++i)
		{
			auto layer = Layer::decodeRecursive(stream);
			if (!layer)
				return;
			layers << layer;
		}
		
		addLayers(layers, tr("Paste Layers"));
	}
}

void LayerUIController::addLayers(const QList<LayerPtr> &layers, const QString &description)
{
	auto scene = d->document->layerScene();
	
	auto current = scene->current();
	int row = current ? current->index() : scene->rootLayer()->count();
	auto parent = current ? current->parent() : scene->rootLayer();
	scene->addLayers(layers, parent, row, description);
	scene->setCurrent(parent->child(row));
}

void LayerUIController::onSelectionChanged()
{
	auto selection = d->document->layerScene()->selection();
	
	{
		LayerConstPtr parent;
		int start, count;
		std::tie(parent, start, count) = layerRangeFromLayers(selection);
		d->actions[ActionMerge]->setEnabled(count >= 2);
	}
	
	for (auto action : d->actionsForLayers)
		action->setEnabled(selection.size());
}

}
