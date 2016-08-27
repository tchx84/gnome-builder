/* ide-refactory.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-refactory"

#include "ide-context.h"
#include "ide-debug.h"

#include "refactory/ide-refactory.h"

struct _IdeRefactory
{
  IdeObject parent_instance;
};

G_DEFINE_TYPE (IdeRefactory, ide_refactory, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_refactory_finalize (GObject *object)
{
  IdeRefactory *self = (IdeRefactory *)object;

  G_OBJECT_CLASS (ide_refactory_parent_class)->finalize (object);
}

static void
ide_refactory_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeRefactory *self = IDE_REFACTORY (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_refactory_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeRefactory *self = IDE_REFACTORY (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_refactory_class_init (IdeRefactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_refactory_finalize;
  object_class->get_property = ide_refactory_get_property;
  object_class->set_property = ide_refactory_set_property;
}

static void
ide_refactory_init (IdeRefactory *self)
{
}
