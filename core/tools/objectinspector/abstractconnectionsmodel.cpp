/*
  abstractconnectionsmodel.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "abstractconnectionsmodel.h"

#include <core/util.h>

#include <QMetaMethod>

#include <private/qobject_p.h>
#include <private/qmetaobject_p.h>

using namespace GammaRay;

AbstractConnectionsModel::AbstractConnectionsModel(QObject* parent): QAbstractTableModel(parent)
{
}

AbstractConnectionsModel::~AbstractConnectionsModel()
{
}

int AbstractConnectionsModel::columnCount(const QModelIndex& parent) const
{
  Q_UNUSED(parent);
  return 4;
}

int AbstractConnectionsModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid())
    return 0;
  return m_connections.size();
}

QVariant AbstractConnectionsModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
    return QVariant();

  if (role == Qt::DisplayRole && index.column() == 3) {
    const Connection &conn = m_connections.at(index.row());
    switch (conn.type) { // see qobject_p.h
      case 0: return tr("Auto");
      case 1: return tr("Direct");
      case 2: return tr("Queued");
      case 3: return tr("Blocking");
    }
  }

  return QVariant();
}

QVariant AbstractConnectionsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (section == 3 && orientation == Qt::Horizontal && role == Qt::DisplayRole)
    return tr("Type");
  return QAbstractItemModel::headerData(section, orientation, role);
}

QString AbstractConnectionsModel::displayString(QObject *object, int methodIndex)
{
  if (!object)
    return QObject::tr("<destroyed>");
  if (methodIndex < 0)
    return QObject::tr("<unknown>");

  const QMetaMethod method = object->metaObject()->method(methodIndex);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
  return method.methodSignature();
#else
  return method.signature();
#endif
}

QString AbstractConnectionsModel::displayString(QObject* object)
{
  if (!object)
    return QObject::tr("<destroyed>");
  return Util::displayString(object);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
// from Qt4's qobject.cpp
static void computeOffsets(const QMetaObject *mo, int *signalOffset, int *methodOffset)
{
  *signalOffset = *methodOffset = 0;
  const QMetaObject *m = mo->superClass();
  while (m) {
    const QMetaObjectPrivate *d = QMetaObjectPrivate::get(m);
    *methodOffset += d->methodCount;
    *signalOffset += d->signalCount;
    m = m->superClass();
  }
}
#endif

int AbstractConnectionsModel::signalIndexToMethodIndex(QObject* object, int signalIndex)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
  return QMetaObjectPrivate::signal(object->metaObject(), signalIndex).methodIndex();
#else

  if (signalIndex < 0)
    return signalIndex;
  Q_ASSERT(object);

  const QMetaObject *mo = object->metaObject();
  int signalOffset, methodOffset;
  computeOffsets(mo, &signalOffset, &methodOffset);
  while (signalOffset > signalIndex) {
    mo = mo->superClass();
    computeOffsets(mo, &signalOffset, &methodOffset);
  }
  const int offset = methodOffset - signalOffset;
  return object->metaObject()->method(signalIndex + offset).methodIndex();
#endif
}