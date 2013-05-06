#ifndef FSLAYER_H
#define FSLAYER_H

#include <QList>
#include <QPixmap>
#include <QVariant>
#include <tuple>
#include <memory>

#include <Malachite/Surface>
#include <Malachite/Misc>
#include <Malachite/BlendMode>
#include <Malachite/Container>

#include "util.h"
#include "global.h"

namespace Malachite
{
class Painter;
}

namespace PaintField
{

class Layer;
typedef std::shared_ptr<Layer> LayerPtr;
typedef std::shared_ptr<const Layer> LayerConstPtr;

class Layer : public std::enable_shared_from_this<Layer>
{
public:
	
	Layer(const QString &name = QString());
	virtual ~Layer();
	
	QList<LayerPtr> children() { return _children; }
	QList<LayerConstPtr> children() const { return Malachite::blindCast<QList<LayerConstPtr> >(_children); }
	
	LayerConstPtr constChild(int index) const;
	LayerConstPtr child(int index) const { return constChild(index); }
	LayerPtr child(int index) { return std::const_pointer_cast<Layer>(constChild(index)); }
	
	LayerConstPtr sibling(int index) const { return parent()->child(index); }
	LayerPtr sibling(int index) { return parent()->child(index); }
	
	LayerConstPtr parent() const { return _parent.lock(); }
	LayerPtr parent() { return _parent.lock(); }
	
	/**
	 * @return The root layer of this layer
	 */
	LayerConstPtr constRoot() const;
	
	LayerConstPtr root() const { return constRoot(); }
	
	/**
	 * @return The root layer of this layer
	 */
	LayerPtr root() { return std::const_pointer_cast<Layer>(constRoot()); }
	
	/**
	 * This function is faster than isAncestorOfSafe().
	 * @param layer
	 * @return Whether this layer is an ancestor of the "layer"
	 */
	bool isAncestorOf(const LayerConstPtr &layer) const;
	
	/**
	 * This function is slower but safer than isAncestorOf() because it even works if "layer" is already deleted.
	 * @param layer
	 * @return Whether this layer is an ancestor of "layer"
	 */
	bool isAncestorOfSafe(const LayerConstPtr &layer) const;
	
	/**
	 * @param row
	 * @return Whether this layer contains a child layer with row index = "row"
	 */
	bool contains(int index) const { return (0 <= index && index < _children.size()) ? true : false; }
	
	/**
	 * @param layer
	 * @return Wheter this layer contains "layer"
	 */
	bool contains(const LayerPtr &layer) const { return _children.contains(layer); }
	
	/**
	 * @param row
	 * @return Whether this contains a child layer with row index = "row".
	 */
	bool insertable(int index) const { return (0 <= index && index <= _children.size()) ? true : false; }
	
	/**
	 * @return How many child layers this layer has
	 */
	int count() const { return _children.size(); }
	
	/**
	 * @return How many child layers "this" parent has
	 */
	int siblingCount() const { return parent()->count(); }
	
	/**
	 * @param child
	 * @return The index of a child layer
	 */
	int indexOf(const LayerConstPtr &child) const { return _children.indexOf(std::const_pointer_cast<Layer>(child)); }
	
	/**
	 * @return The index of this layer in its parent.
	 */
	int index() const
	{
		auto p = parent();
		return p ? p->indexOf(shared_from_this()) : 0;
	}
	
	/**
	 * Inserts a child layer.
	 * @param row
	 * @param child
	 * @return Whether succeeded
	 */
	bool insert(int index, const LayerPtr &child);
	
	/**
	 * Inserts child layers.
	 * @param row
	 * @param children
	 * @return Whether succeeded
	 */
	bool insert(int index, const QList<LayerPtr> &children);
	
	/**
	 * Prepends a child layer.
	 * @param child
	 */
	void prepend(const LayerPtr &child) { insert(0, child); }
	
	void prepend(const QList<LayerPtr> &layers) { insert(0, layers); }
	
	/**
	 * Appends a child layer.
	 * @param child
	 */
	void append(const LayerPtr &child) { insert(count(), child); }
	
	void append(const QList<LayerPtr> &layers) { insert(count(), layers); }
	
	/**
	 * Take a child layer.
	 * Its parent will be 0.
	 * @param row
	 * @return The taken layer
	 */
	LayerPtr take(int index);
	
	/**
	 * Take all child layers.
	 * @return The taken layers
	 */
	QList<LayerPtr> takeAll();
	
	/**
	 * Clones this layer.
	 * Calls createAnother to duplicate the layer and use encode / decode to copy data.
	 * @return 
	 */
	LayerPtr clone() const;
	
	/**
	 * Clones this layer and its descendants.
	 * @return The cloned layer
	 */
	LayerPtr cloneRecursive() const;
	
	/**
	 * Creates an unduplicated child name (eg "Layer 1").
	 * @param name A base name (eg "Layer")
	 * @return The unduplicated child name
	 */
	QString unduplicatedChildName(const QString &name) const { return Util::unduplicatedName(childNames(), name); }
	
	QStringList childNames() const;
	
	/**
	 * Sets a property.
	 * @param data
	 * @param role
	 * @return Whether succeeded
	 */
	virtual bool setProperty(const QVariant &data, int role);
	
	/**
	 * @param role
	 * @return The property
	 */
	virtual QVariant property(int role) const;
	
	void setName(const QString &name) { _name = name; }
	QString name() const { return _name; }
	
	void setVisible(bool visible) { _isVisible = visible; }
	bool isVisible() const { return _isVisible; }
	
	void setLocked(bool locked) { _isLocked = locked; }
	bool isLocked() const { return property(PaintField::RoleLocked).toBool(); }
	
	void setThumbnail(const QPixmap &thumbnail) { _thumbnail = thumbnail; }
	QPixmap thumbnail() const { return _thumbnail; }
	
	void setOpacity(double opacity) { _opacity = opacity; }
	double opacity() const { return _opacity; }
	
	void setBlendMode(Malachite::BlendMode mode) { _blendMode = mode; }
	Malachite::BlendMode blendMode() const { return _blendMode; }
	
	void setThumbnailDirty(bool x) { _isThumbnailDirty = x; }
	bool isThumbnailDirty() const { return _isThumbnailDirty; }
	
	/**
	 * Updates only this layer's thumbnail
	 * @param documentSize
	 */
	virtual void updateThumbnail(const QSize &documentSize) { Q_UNUSED(documentSize) }
	
	/**
	 * Updates the thumbnail of each of ths and this descendant layers.
	 * @param documentSize
	 */
	void updateThumbnailRecursive(const QSize &documentSize);
	
	/**
	 * Updates the thumbnail of each of ths and this descendant layers, if its isThumbnailDirty() is true.
	 * @param size
	 */
	void updateDirtyThumbnailRecursive(const QSize &size);
	
	LayerConstPtr descendantAt(const QPoint &pos, int margin) const;
	
	virtual QPointSet tileKeys() const { return QPointSet(); }
	QPointSet tileKeysRecursive() const;
	
	void encodeRecursive(QDataStream &stream) const;
	static LayerPtr decodeRecursive(QDataStream &stream);
	
	/**
	 * Creates another instance of the layer.
	 * The properties does not have to be copied.
	 * @return 
	 */
	virtual LayerPtr createAnother() const = 0;
	
	virtual bool canHaveChildren() const { return false; }
	
	template <class T> bool isType() const { return dynamic_cast<const T *>(this); }
	
	virtual void encode(QDataStream &stream) const;
	virtual void decode(QDataStream &stream);
	
	virtual QVariantMap saveProperies() const;
	virtual void loadProperties(const QVariantMap &map);
	
	virtual bool hasDataToSave() const { return false; }
	virtual void saveDataFile(QDataStream &stream) const { Q_UNUSED(stream) }
	virtual void loadDataFile(QDataStream &stream) { Q_UNUSED(stream) }
	virtual QString dataSuffix() const { return "data"; }
	
	virtual void render(Malachite::Painter *painter) const { Q_UNUSED(painter) }
	
	/**
	 * Returns whether the layer is not transparent
	 * in the rectangle ( pos.x() - margin, pos.y() - margin, margin * 2, margin * 2 ).
	 * @param pos
	 * @param margin
	 * @return 
	 */
	virtual bool includes(const QPoint &pos, int margin) const { Q_UNUSED(pos) Q_UNUSED(margin) return false; }
	
	/**
	 * Return whether the layer is "selectable" graphically.
	 * This function returns false if and only if includes() returns false for any position.
	 * @return 
	 */
	virtual bool isGraphicallySelectable() const { return false; }
	
protected:
	
private:
	
	friend class LayerRef;
	
	std::weak_ptr<Layer> _parent;
	QList<LayerPtr> _children;
	
	QString _name;
	bool _isLocked = false, _isVisible = true;
	double _opacity = 1.0;
	Malachite::BlendMode _blendMode;
	QPixmap _thumbnail;
	
	bool _isThumbnailDirty = false;
};

class LayerFactory
{
public:
	LayerFactory() {}
	virtual ~LayerFactory() {}
	
	virtual QString name() const = 0;
	virtual LayerPtr create() const = 0;
	virtual const ::std::type_info &typeInfo() const = 0;
};

}

QDebug operator<<(QDebug debug, const PaintField::LayerConstPtr &layer);

#endif // FSLAYER_H
