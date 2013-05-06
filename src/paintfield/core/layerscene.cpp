#include <functional>
#include <tuple>
#include <QUndoCommand>
#include <QTimer>
#include <QItemSelectionModel>
#include <boost/range/adaptors.hpp>

#include "document.h"
#include "rasterlayer.h"
#include "grouplayer.h"
#include "layeredit.h"
#include "layerrenderer.h"
#include "layeritemmodel.h"

#include "layerscene.h"

using namespace std;
using namespace Malachite;

namespace PaintField {

class LayerSceneCommand : public QUndoCommand
{
public:
	
	typedef QList<int> Path;
	
	LayerSceneCommand(LayerScene *scene, QUndoCommand *parent) :
		QUndoCommand(parent),
		_scene(scene)
	{}
	
	void insertLayer(const LayerPtr &parent, int index, const LayerPtr &layer)
	{
		PAINTFIELD_DEBUG << parent << index << layer;
		
		emit _scene->layerAboutToBeInserted(parent, index);
		parent->insert(index, layer);
		emit _scene->layerInserted(parent, index);
		
		enqueueTileUpdate(layer->tileKeysRecursive());
	}
	
	LayerPtr takeLayer(const LayerPtr &parent, int index)
	{
		emit _scene->layerAboutToBeRemoved(parent, index);
		auto layer = parent->take(index);
		emit _scene->layerRemoved(parent, index);
		
		enqueueTileUpdate(layer->tileKeysRecursive());
		
		return layer;
	}
	
	void emitLayerPropertyChanged(const LayerConstPtr &layer)
	{
		emit _scene->layerPropertyChanged(layer);
	}
	
	LayerScene *scene() { return _scene; }
	
	void enqueueTileUpdate(const QPointSet &keys)
	{
		_scene->enqueueTileUpdate(keys);
	}
	
	LayerPtr layerForPath(const Path &path)
	{
		return std::const_pointer_cast<Layer>(_scene->layerForPath(path));
	}
	
	static Path pathForLayer(const LayerConstPtr &layer)
	{
		return LayerScene::pathForLayer(layer);
	}
	
private:
	
	LayerScene *_scene = 0;
	LayerPtr _insertParent, _removeParent;
};

class LayerSceneEditCommand : public LayerSceneCommand
{
public:
	
	LayerSceneEditCommand(const LayerConstPtr &layer, LayerEdit *edit, LayerScene *scene, QUndoCommand *parent = 0) :
		LayerSceneCommand(scene, parent),
		_path(pathForLayer(layer)),
		_edit(edit)
	{}
	
	void redo()
	{
		redoUndo(true);
	}
	
	void undo()
	{
		redoUndo(false);
	}
	
private:
	
	void redoUndo(bool redo)
	{
		auto layer = layerForPath(_path);
		
		if (redo)
			_edit->redo(layer);
		else
			_edit->undo(layer);
		
		layer->setThumbnailDirty(true);
		enqueueTileUpdate(_edit->modifiedKeys());
	}
	
	Path _path;
	QScopedPointer<LayerEdit> _edit;
};

class LayerScenePropertyChangeCommand : public LayerSceneCommand
{
public:
	
	LayerScenePropertyChangeCommand(const LayerConstPtr &layer, const QVariant &data, int role, LayerScene *scene, QUndoCommand *parent = 0) :
		LayerSceneCommand(scene, parent),
		_path(pathForLayer(layer)),
		_data(data),
		_role(role)
	{}
	
	void redo()
	{
		change();
	}
	
	void undo()
	{
		change();
	}
	
private:
	
	void change()
	{
		auto layer = layerForPath(_path);
		
		enqueueLayerTileUpdate(layer);
		
		auto old = layer->property(_role);
		layer->setProperty(_data, _role);
		_data = old;
		
		enqueueLayerTileUpdate(layer);
		
		emitLayerPropertyChanged(layer);
	}
	
	void enqueueLayerTileUpdate(const LayerConstPtr &layer)
	{
		switch (_role)
		{
			case RoleName:
			case RoleLocked:
				break;
			default:
				enqueueTileUpdate(layer->tileKeysRecursive());
				break;
		}
	}
	
	Path _path;
	QVariant _data;
	int _role;
};

class LayerSceneAddCommand : public LayerSceneCommand
{
public:
	
	LayerSceneAddCommand(const LayerPtr &layer, const LayerConstPtr &parentRef, int index, LayerScene *scene, QUndoCommand *parent) :
		LayerSceneCommand(scene, parent),
		_layer(layer),
		_parentPath(pathForLayer(parentRef)),
		_index(index)
	{}
	
	void redo()
	{
		auto parent = layerForPath(_parentPath);
		insertLayer(parent, _index, _layer);
	}
	
	void undo()
	{
		auto parent = layerForPath(_parentPath);
		_layer = takeLayer(parent, _index);
	}
	
private:
	
	LayerPtr _layer;
	Path _parentPath;
	int _index;
};

class LayerSceneRemoveCommand : public LayerSceneCommand
{
public:
	
	LayerSceneRemoveCommand(const LayerConstPtr &layer, LayerScene *scene, QUndoCommand *parent) :
		LayerSceneCommand(scene, parent),
		_ref(layer)
	{
	}
	
	void redo()
	{
		if (!_pathsSet)
		{
			auto path = pathForLayer(_ref);
			_parentPath = path;
			_parentPath.removeLast();
			_index = path.last();
		}
		
		_layer = takeLayer(layerForPath(_parentPath), _index);
	}
	
	void undo()
	{
		insertLayer(layerForPath(_parentPath), _index, _layer);
	}
	
private:
	
	LayerConstPtr _ref;
	
	bool _pathsSet = false;
	Path _parentPath;
	LayerPtr _layer;
	int _index;
};

enum InsertionType
{
	InsertionTypeBefore,
	InsertionTypeAppendAsChild
};

class LayerSceneCopyCommand : public LayerSceneCommand
{
public:
	
	LayerSceneCopyCommand(const LayerConstPtr &layer, const LayerConstPtr &parentLayer, int index, const QString &newName, LayerScene *scene, QUndoCommand *parent) :
		LayerSceneCommand(scene, parent),
		_layer(layer),
		_parent(parentLayer),
		_index(index),
		_newName(newName)
	{
	}
	
	void redo()
	{
		if (!_pathsSet)
		{
			_layerPath = pathForLayer(_layer);
			_parentPath = pathForLayer(_parent);
			_pathsSet = true;
		}
		
		auto parent = layerForPath(_parentPath);
		auto layer = layerForPath(_layerPath);
		
		auto clone = layer->cloneRecursive();
		clone->setName(_newName);
		
		insertLayer(parent, _index, clone);
	}
	
	void undo()
	{
		auto parent = layerForPath(_parentPath);
		takeLayer(parent, _index);
	}
	
private:
	
	LayerConstPtr _layer, _parent;
	
	bool _pathsSet = false;
	Path _layerPath, _parentPath;
	int _index;
	QString _newName;
};

class LayerSceneMoveCommand : public LayerSceneCommand
{
public:
	
	LayerSceneMoveCommand(const LayerConstPtr &layer, const LayerConstPtr &parentLayer, int index, const QString &newName, LayerScene *scene, QUndoCommand *parent) :
		LayerSceneCommand(scene, parent),
		_layer(layer),
		_parent(parentLayer),
		_index(index),
		_newName(newName)
	{
	}

	void redo()
	{
		if (!_pathsSet)
		{
			_layerPath = pathForLayer(_layer);
			_parentPath = pathForLayer(_parent);
			_pathsSet = true;
		}
		
		move();
	}
	
	void undo()
	{
		move();
	}
	
private:
	
	void move()
	{
		PAINTFIELD_DEBUG << _layerPath << _parentPath << _index;
		
		auto layer = layerForPath(_layerPath);
		int oldIndex = layer->index();
		auto oldParent = layer->parent();
		
		auto parent = layerForPath(_parentPath);
		int index = _index;
		
		takeLayer(oldParent, oldIndex);
		
		if (parent == oldParent)
		{
			if (index > oldIndex)
				--index;
			else if (oldIndex > index)
				++oldIndex;
		}
		
		insertLayer(parent, index, layer);
		
		auto oldName = layer->name();
		layer->setName(_newName);
		
		_index = oldIndex;
		_parentPath = pathForLayer(oldParent);
		_layerPath = pathForLayer(layer);
		_newName = oldName;
		
		PAINTFIELD_DEBUG << _layerPath << _parentPath << _index;
	}
	
	LayerConstPtr _layer, _parent;
	
	bool _pathsSet = false;
	Path _layerPath, _parentPath;
	int _index;
	QString _newName;
};

class LayerSceneMergeCommand : public LayerSceneCommand
{
public:
	
	LayerSceneMergeCommand(const LayerConstPtr &parentRef, int index, int count, const QString &newName, LayerScene *scene, QUndoCommand *parent) :
		LayerSceneCommand(scene, parent),
		_index(index),
		_count(count),
		_newName(newName),
		_group(std::make_shared<GroupLayer>())
	{
		_parentPath = pathForLayer(parentRef);
	}
	
	void redo()
	{
		auto parent = layerForPath(_parentPath);
		
		for (int i = 0; i < _count; ++i)
			_group->append(takeLayer(parent, _index));
		
		LayerRenderer renderer;
		
		auto newLayer = std::make_shared<RasterLayer>(_newName);
		newLayer->setSurface(renderer.renderToSurface(_group));
		newLayer->updateThumbnail(scene()->document()->size());
		
		insertLayer(parent, _index, newLayer);
	}
	
	void undo()
	{
		auto parent = layerForPath(_parentPath);
		
		takeLayer(parent, _index);
		
		for (int i = 0; i < _count; ++i)
			insertLayer(parent, _index + i, _group->take(0));
	}
	
private:
	
	Path _parentPath;
	int _index, _count;
	QString _newName;
	
	std::shared_ptr<GroupLayer> _group;
};

struct LayerScene::Data
{
	std::shared_ptr<GroupLayer> rootLayer;
	Document *document = 0;
	QPointSet updatedKeys;
	
	QTimer *thumbnailUpdateTimer = 0;
	
	LayerItemModel *itemModel = 0;
	QItemSelectionModel *selectionModel = 0;
	
	LayerConstPtr current;
	
	bool checkLayer(const LayerConstPtr &layer)
	{
		return layer && layer->root() == rootLayer;
	}
};

LayerScene::LayerScene(const QList<LayerPtr> &layers, Document *document) :
	QObject(document),
	d(new Data)
{
	connect(this, SIGNAL(layerPropertyChanged(LayerConstPtr)), this, SLOT(onLayerPropertyChanged(LayerConstPtr)));
	
	d->document = document;
	connect(d->document, SIGNAL(modified()), this, SLOT(update()));
	
	{
		auto root = std::make_shared<GroupLayer>();
		root->insert(0, layers);
		root->updateThumbnailRecursive(document->size());
		d->rootLayer = root;
	}
	
	{
		auto t = new QTimer(this);
		t->setInterval(500);
		t->setSingleShot(true);
		connect(t, SIGNAL(timeout()), this, SLOT(updateDirtyThumbnails()));
		d->thumbnailUpdateTimer = t;
	}
	
	{
		auto im = new LayerItemModel( this, this );
		auto sm = new QItemSelectionModel(im, this );
		
		connect( sm, SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(onCurrentIndexChanged(QModelIndex,QModelIndex)) );
		connect( sm, SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(onItemSelectionChanged(QItemSelection,QItemSelection)) );
		
		sm->setCurrentIndex( im->index( 0, 0, QModelIndex() ), QItemSelectionModel::Current );
		
		d->itemModel = im;
		d->selectionModel = sm;
	}
}

LayerScene::~LayerScene()
{
	delete d;
}

class DuplicatedNameResolver
{
public:
	
	enum Type
	{
		TypeAdd,
		TypeMove
	};
	
	DuplicatedNameResolver(const LayerConstPtr &parent, Type type) :
		_type(type),
		_parent(parent),
		_names(parent->childNames())
	{
	}
	
	QString resolve(const LayerConstPtr &layer)
	{
		QString original = layer->name();
		
		if (_type == TypeMove && layer->parent() == _parent)
			return original;
		
		QString name = Util::unduplicatedName(_names, original);
		_names << name;
		return name;
	}
	
private:
	
	Type _type;
	LayerConstPtr _parent;
	QStringList _names;
};

void LayerScene::addLayers(const QList<LayerPtr> &layers, const LayerConstPtr &parent, int index, const QString &description)
{
	if (!d->checkLayer(parent))
	{
		PAINTFIELD_WARNING << "invalid parent";
		return;
	}
	
	DuplicatedNameResolver resolver(parent, DuplicatedNameResolver::TypeAdd);
	auto command = new QUndoCommand(description);
	
	for (const auto &layer : layers)
	{
		layer->updateThumbnailRecursive(d->document->size());
		layer->setName(resolver.resolve(layer));
		new LayerSceneAddCommand(layer, parent, index++, this, command);
	}
	
	pushCommand(command);
}

void LayerScene::removeLayers(const QList<LayerConstPtr> &layers, const QString &description)
{
	auto command = new QUndoCommand(description.isEmpty() ? tr("Remove Layers") : description);
	for (const auto &layer : layers)
		new LayerSceneRemoveCommand(layer, this, command);
	pushCommand(command);
}

void LayerScene::moveLayers(const QList<LayerConstPtr> &layers, const LayerConstPtr &parent, int index)
{
	for (const auto &layer : layers)
		if (!d->checkLayer(layer))
		{
			PAINTFIELD_WARNING << "invalid layers";
			return;
		}
	
	if (!d->checkLayer(parent))
	{
		PAINTFIELD_WARNING << "invalid parent";
		return;
	}
	
	int newIndex = index;
	
	DuplicatedNameResolver resolver(parent, DuplicatedNameResolver::TypeMove);
	
	auto command = new QUndoCommand(tr("Move Layers"));
	
	for (const auto &layer : layers)
	{
		new LayerSceneMoveCommand(layer, parent, newIndex, resolver.resolve(layer), this, command);
		
		if (layer->parent() == parent && layer->index() < index)
			--newIndex;
		++newIndex;
	}
	
	pushCommand(command);
}

void LayerScene::copyLayers(const QList<LayerConstPtr> &layers, const LayerConstPtr &parent, int index)
{
	for (const auto &layer : layers)
		if (!d->checkLayer(layer))
		{
			PAINTFIELD_WARNING << "invalid layers";
			return;
		}
	
	if (!d->checkLayer(parent))
	{
		PAINTFIELD_WARNING << "invalid parent";
		return;
	}
	
	int newIndex = index;
	
	DuplicatedNameResolver resolver(parent, DuplicatedNameResolver::TypeAdd);
	
	auto command = new QUndoCommand(tr("Move Layers"));
	
	for (const auto &layer : layers)
	{
		new LayerSceneCopyCommand(layer, parent, newIndex, resolver.resolve(layer), this, command);
		++newIndex;
	}
	
	pushCommand(command);
}

void LayerScene::mergeLayers(const LayerConstPtr &parent, int index, int count)
{
	if (!d->checkLayer(parent))
	{
		PAINTFIELD_WARNING << "invalid parent";
		return;
	}
	
	QString mergedName;
	
	for (int i = index; i < index + count; ++i)
	{
		mergedName += parent->child(i)->name();
		if (i != index + count - 1)
			mergedName += " + ";
	}
	
	auto command = new LayerSceneMergeCommand(parent, index, count, mergedName, this, 0);
	command->setText(tr("Merge Layers"));
	pushCommand(command);
}

void LayerScene::editLayer(const LayerConstPtr &layer, LayerEdit *edit, const QString &description)
{
	PAINTFIELD_DEBUG << d->rootLayer->children();
	PAINTFIELD_DEBUG << layer;
	PAINTFIELD_DEBUG << layer->parent();
	
	if (!d->checkLayer(layer))
	{
		PAINTFIELD_WARNING << "invalid layer";
		return;
	}
	
	if (layer->isLocked())
		return;
	
	auto command = new LayerSceneEditCommand(layer, edit, this, 0);
	command->setText(description);
	pushCommand(command);
}

void LayerScene::setLayerProperty(const LayerConstPtr &layer, const QVariant &data, int role, const QString &description)
{
	if (!d->checkLayer(layer))
	{
		PAINTFIELD_WARNING << "invalid layer";
		return;
	}
	if (layer->isLocked() && role != RoleLocked)
	{
		PAINTFIELD_WARNING << "layer locked";
		return;
	}
	if (layer->property(role) == data)
		return;
	
	QString text = description;
	
	if (text.isEmpty())
	{
		// set description text if possible
		switch (role)
		{
			case PaintField::RoleName:
				text = tr("Rename Layer");
				break;
			case PaintField::RoleVisible:
				text = tr("Change visibility");
				break;
			case PaintField::RoleBlendMode:
				text = tr("Change Blend Mode");
				break;
			case PaintField::RoleOpacity:
				text = tr("Change Opacity");
				break;
			default:
				break;
		}
	}
	
	auto command = new LayerScenePropertyChangeCommand(layer, data, role, this, 0);
	command->setText(text);
	pushCommand(command);
}

LayerConstPtr LayerScene::rootLayer() const
{
	return d->rootLayer;
}

Document *LayerScene::document()
{
	return d->document;
}

LayerItemModel *LayerScene::itemModel()
{
	return d->itemModel;
}

QItemSelectionModel *LayerScene::itemSelectionModel()
{
	return d->selectionModel;
}

LayerConstPtr LayerScene::current() const
{
	return d->current;
}

QList<LayerConstPtr> LayerScene::selection() const
{
	return d->itemModel->layersForIndexes(d->selectionModel->selection().indexes());
}

LayerConstPtr LayerScene::layerForPath(const QList<int> &path)
{
	auto layer = rootLayer();
	
	for (int index : path)
		layer = layer->child(index);
	
	return layer;
}

QList<int> LayerScene::pathForLayer(const LayerConstPtr &layer)
{
	QList<int> path;
	auto l = layer;
	while (l->parent())
	{
		path.prepend(l->index());
		l = l->parent();
	}
	return path;
}

void LayerScene::abortThumbnailUpdate()
{
	d->thumbnailUpdateTimer->stop();
}

void LayerScene::update()
{
	emit tilesUpdated(d->updatedKeys);
	d->thumbnailUpdateTimer->start();
	d->updatedKeys.clear();
}

void LayerScene::setCurrent(const LayerConstPtr &layer)
{
	d->selectionModel->setCurrentIndex(d->itemModel->indexForLayer(layer), QItemSelectionModel::Current);
}

void LayerScene::setSelection(const QList<LayerConstPtr> &layers)
{
	d->selectionModel->clearSelection();
	
	for (auto layer : layers)
		d->selectionModel->select(d->itemModel->indexForLayer(layer), QItemSelectionModel::Select);
}

void LayerScene::enqueueTileUpdate(const QPointSet &keys)
{
	d->updatedKeys |= keys;
}

void LayerScene::updateDirtyThumbnails()
{
	d->rootLayer->updateDirtyThumbnailRecursive(d->document->size());
	emit thumbnailsUpdated();
}

LayerPtr LayerScene::mutableRootLayer()
{
	return d->rootLayer;
}

void LayerScene::pushCommand(QUndoCommand *command)
{
	PAINTFIELD_DEBUG << "pushing command" << command->text();
	d->document->undoStack()->push(command);
}

void LayerScene::onCurrentIndexChanged(const QModelIndex &now, const QModelIndex &old)
{
	d->current = d->itemModel->layerExceptRootForIndex(now);
	emit currentChanged(d->current, d->itemModel->layerExceptRootForIndex(old));
}

void LayerScene::onItemSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
	emit selectionChanged(d->itemModel->layersForIndexes(selected.indexes()), d->itemModel->layersForIndexes(deselected.indexes()));
}

void LayerScene::onLayerPropertyChanged(const LayerConstPtr &layer)
{
	if (layer == d->current)
		emit currentLayerPropertyChanged();
}

} // namespace PaintField
