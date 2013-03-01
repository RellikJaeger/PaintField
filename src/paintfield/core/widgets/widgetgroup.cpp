#include <QWidget>

#include "widgetgroup.h"

namespace PaintField
{

struct WidgetGroup::Data
{
	QWidgetList widgets;
	bool visible = false, enabled = true;
};

WidgetGroup::WidgetGroup(QObject *parent) : 
	QObject(parent),
	d(new Data)
{}

WidgetGroup::~WidgetGroup()
{
	delete d;
}

void WidgetGroup::addWidget(QWidget *widget)
{
	widget->setVisible(d->visible);
	widget->setEnabled(d->enabled);
	d->widgets << widget;
}

bool WidgetGroup::isVisible() const
{
	return d->visible;
}

bool WidgetGroup::isEnabled() const
{
	return d->enabled;
}

void WidgetGroup::setVisible(bool visible)
{
	d->visible = visible;
	for (QWidget *widget : d->widgets)
		widget->setVisible(visible);
}

void WidgetGroup::setEnabled(bool enabled)
{
	d->enabled = enabled;
	for (QWidget *widget : d->widgets)
		widget->setEnabled(enabled);
}

}

