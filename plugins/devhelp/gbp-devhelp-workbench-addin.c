/* gbp-devhelp-workbench-addin.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "devhelp-plugin"

#include <devhelp/devhelp.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "gbp-devhelp-panel.h"
#include "gbp-devhelp-workbench-addin.h"

struct _GbpDevhelpWorkbenchAddin
{
  GObject          parent_instance;
  GbpDevhelpPanel *panel;
  DhBookManager   *books;
};

static void gbp_devhelp_workbench_addin_init_iface (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpDevhelpWorkbenchAddin, gbp_devhelp_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN,
                                               gbp_devhelp_workbench_addin_init_iface))

static void
gbp_devhelp_workbench_addin_class_init (GbpDevhelpWorkbenchAddinClass *klass)
{
}

static void
gbp_devhelp_workbench_addin_init (GbpDevhelpWorkbenchAddin *self)
{
}

static void
gbp_devhelp_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpDevhelpWorkbenchAddin *self = (GbpDevhelpWorkbenchAddin *)addin;
  IdePerspective *perspective;
  GtkWidget *pane;

  g_assert (IDE_IS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->books = dh_book_manager_new ();
  dh_book_manager_populate (self->books);

  perspective = ide_workbench_get_perspective_by_name (workbench, "editor");
  g_assert (IDE_IS_LAYOUT (perspective));

  pane = pnl_dock_bin_get_right_edge (PNL_DOCK_BIN (perspective));
  g_assert (IDE_IS_LAYOUT_PANE (pane));

  self->panel = g_object_new (GBP_TYPE_DEVHELP_PANEL,
                              "book-manager", self->books,
                              "expand", TRUE,
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (pane), GTK_WIDGET (self->panel));
}

static void
gbp_devhelp_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpDevhelpWorkbenchAddin *self = (GbpDevhelpWorkbenchAddin *)addin;
  IdePerspective *perspective;
  GtkWidget *pane;

  g_assert (IDE_IS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  g_clear_object (&self->books);

  perspective = ide_workbench_get_perspective_by_name (workbench, "editor");
  g_assert (IDE_IS_LAYOUT (perspective));

  pane = pnl_dock_bin_get_right_edge (PNL_DOCK_BIN (perspective));
  g_assert (IDE_IS_LAYOUT_PANE (pane));

  gtk_widget_destroy (GTK_WIDGET (self->panel));
  self->panel = NULL;
}

static void
gbp_devhelp_workbench_addin_init_iface (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_devhelp_workbench_addin_load;
  iface->unload = gbp_devhelp_workbench_addin_unload;
}
