/* ide-shortcut-manager.c
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

#define G_LOG_DOMAIN "ide-shortcut-manager.h"

#include "ide-debug.h"

#include "shortcuts/ide-shortcut-controller.h"
#include "shortcuts/ide-shortcut-manager.h"

struct _IdeShortcutManager
{
  GObject object;
};

typedef struct
{
  IdeShortcutTheme *theme;
  GPtrArray        *themes;
} IdeShortcutManagerPrivate;

enum {
  PROP_0,
  PROP_THEME,
  PROP_THEME_NAME,
  N_PROPS
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeShortcutManager, ide_shortcut_manager, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (IdeShortcutManager)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
ide_shortcut_manager_finalize (GObject *object)
{
  IdeShortcutManager *self = (IdeShortcutManager *)object;
  IdeShortcutManagerPrivate *priv = ide_shortcut_manager_get_instance_private (self);

  g_clear_pointer (&priv->themes, g_ptr_array_unref);
  g_clear_object (&priv->theme);

  G_OBJECT_CLASS (ide_shortcut_manager_parent_class)->finalize (object);
}

static void
ide_shortcut_manager_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeShortcutManager *self = (IdeShortcutManager *)object;

  switch (prop_id)
    {
    case PROP_THEME:
      g_value_set_object (value, ide_shortcut_manager_get_theme (self));
      break;

    case PROP_THEME_NAME:
      g_value_set_string (value, ide_shortcut_manager_get_theme_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_shortcut_manager_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeShortcutManager *self = (IdeShortcutManager *)object;

  switch (prop_id)
    {
    case PROP_THEME:
      ide_shortcut_manager_set_theme (self, g_value_get_object (value));
      break;

    case PROP_THEME_NAME:
      ide_shortcut_manager_set_theme_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_shortcut_manager_class_init (IdeShortcutManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_shortcut_manager_finalize;
  object_class->get_property = ide_shortcut_manager_get_property;
  object_class->set_property = ide_shortcut_manager_set_property;

  properties [PROP_THEME] =
    g_param_spec_object ("theme",
                         "Theme",
                         "The current key theme.",
                         IDE_TYPE_SHORTCUT_THEME,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_THEME_NAME] =
    g_param_spec_string ("theme-name",
                         "Theme Name",
                         "The name of the current theme",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_shortcut_manager_init (IdeShortcutManager *self)
{
  IdeShortcutManagerPrivate *priv = ide_shortcut_manager_get_instance_private (self);

  priv->themes = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * ide_shortcut_manager_get_default:
 *
 * Gets the singleton #IdeShortcutManager for the process.
 *
 * Returns: (transfer none) (not nullable): An #IdeShortcutManager.
 */
IdeShortcutManager *
ide_shortcut_manager_get_default (void)
{
  static IdeShortcutManager *instance;

  if (instance == NULL)
    {
      instance = g_object_new (IDE_TYPE_SHORTCUT_MANAGER, NULL);
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
    }

  return instance;
}

/**
 * ide_shortcut_manager_get_theme:
 *
 * Gets the "theme" property.
 *
 * Returns: (transfer none) (not nullable): An #IdeShortcutTheme.
 */
IdeShortcutTheme *
ide_shortcut_manager_get_theme (IdeShortcutManager *self)
{
  IdeShortcutManagerPrivate *priv = ide_shortcut_manager_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SHORTCUT_MANAGER (self), NULL);

  if (priv->theme == NULL)
    priv->theme = g_object_new (IDE_TYPE_SHORTCUT_THEME,
                                "name", "default",
                                NULL);

  return priv->theme;
}

/**
 * ide_shortcut_manager_set_theme:
 * @self: An #IdeShortcutManager
 * @theme: (not nullable): An #IdeShortcutTheme
 *
 * Sets the theme for the shortcut manager.
 */
void
ide_shortcut_manager_set_theme (IdeShortcutManager *self,
                                IdeShortcutTheme   *theme)
{
  IdeShortcutManagerPrivate *priv = ide_shortcut_manager_get_instance_private (self);

  g_return_if_fail (IDE_IS_SHORTCUT_MANAGER (self));
  g_return_if_fail (IDE_IS_SHORTCUT_THEME (theme));

  /*
   * It is important that IdeShortcutController instances watch for
   * notify::theme so that they can reset their state. Otherwise, we
   * could be transitioning between incorrect contexts.
   */

  if (g_set_object (&priv->theme, theme))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_THEME]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_THEME_NAME]);
    }
}

/**
 * ide_shortcut_manager_handle_event:
 * @self: (nullable): An #IdeShortcutManager
 * @toplevel: A #GtkWidget or %NULL.
 * @event: A #GdkEventKey event to handle.
 *
 * This function will try to dispatch @event to the proper widget and
 * #IdeShortcutContext. If the event is handled, then %TRUE is returned.
 *
 * You should call this from #GtkWidget::key-press-event handler in your
 * #GtkWindow toplevel.
 *
 * Returns: %TRUE if the event was handled.
 */
gboolean
ide_shortcut_manager_handle_event (IdeShortcutManager *self,
                                   const GdkEventKey  *event,
                                   GtkWidget          *toplevel)
{
  GtkWidget *widget;
  GtkWidget *focus;
  GdkModifierType modifier;

  IDE_ENTRY;

  if (self == NULL)
    self = ide_shortcut_manager_get_default ();

  g_return_val_if_fail (IDE_IS_SHORTCUT_MANAGER (self), FALSE);
  g_return_val_if_fail (GTK_IS_WINDOW (toplevel), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type != GDK_KEY_PRESS)
    IDE_RETURN (GDK_EVENT_PROPAGATE);

  modifier = event->state & gtk_accelerator_get_default_mod_mask ();
  widget = focus = gtk_window_get_focus (GTK_WINDOW (toplevel));

  while (widget != NULL)
    {
      g_autoptr(GtkWidget) widget_hold = g_object_ref (widget);
      IdeShortcutController *controller;
      gboolean use_binding_sets = TRUE;

      if (NULL != (controller = ide_shortcut_controller_find (widget)))
        {
          IdeShortcutContext *context = ide_shortcut_controller_get_context (controller);

          /*
           * Fetch this property first as the controller context could change
           * during activation of the handle_event().
           */
          if (context != NULL)
            g_object_get (context,
                          "use-binding-sets", &use_binding_sets,
                          NULL);

          /*
           * Now try to activate the event using the controller.
           */
          if (ide_shortcut_controller_handle_event (controller, event))
            IDE_RETURN (GDK_EVENT_STOP);
        }

      /*
       * If the current context at activation indicates that we can
       * dispatch using the default binding sets for the widget, go
       * ahead and try to do that.
       */
      if (use_binding_sets)
        {
          GtkStyleContext *style_context;
          g_autoptr(GPtrArray) sets = NULL;

          style_context = gtk_widget_get_style_context (widget);
          gtk_style_context_get (style_context,
                                 gtk_style_context_get_state (style_context),
                                 "-gtk-key-bindings", &sets,
                                 NULL);

          if (sets != NULL)
            {
              for (guint i = 0; i < sets->len; i++)
                {
                  GtkBindingSet *set = g_ptr_array_index (sets, i);

                  if (gtk_binding_set_activate (set, event->keyval, modifier, G_OBJECT (widget)))
                    IDE_RETURN (GDK_EVENT_STOP);
                }
            }

          /*
           * Only if this widget is also our focus, try to activate the default
           * keybindings for the widget.
           */
          if (widget == focus)
            {
              GtkBindingSet *set = gtk_binding_set_by_class (G_OBJECT_GET_CLASS (widget));

              if (gtk_binding_set_activate (set, event->keyval, modifier, G_OBJECT (widget)))
                IDE_RETURN (GDK_EVENT_STOP);
            }
        }

      widget = gtk_widget_get_parent (widget);
    }

  IDE_RETURN (GDK_EVENT_PROPAGATE);
}

const gchar *
ide_shortcut_manager_get_theme_name (IdeShortcutManager *self)
{
  IdeShortcutManagerPrivate *priv = ide_shortcut_manager_get_instance_private (self);
  const gchar *ret = NULL;

  g_return_val_if_fail (IDE_IS_SHORTCUT_MANAGER (self), NULL);

  if (priv->theme != NULL)
    ret = ide_shortcut_theme_get_name (priv->theme);

  return ret;
}

void
ide_shortcut_manager_set_theme_name (IdeShortcutManager *self,
                                     const gchar        *name)
{
  IdeShortcutManagerPrivate *priv = ide_shortcut_manager_get_instance_private (self);

  g_return_if_fail (IDE_IS_SHORTCUT_MANAGER (self));

  if (name == NULL)
    name = "default";

  for (guint i = 0; i < priv->themes->len; i++)
    {
      IdeShortcutTheme *theme = g_ptr_array_index (priv->themes, i);
      const gchar *theme_name = ide_shortcut_theme_get_name (theme);

      if (g_strcmp0 (name, theme_name) == 0)
        {
          ide_shortcut_manager_set_theme (self, theme);
          return;
        }
    }

  g_warning ("No such shortcut theme “%s”", name);
}

static guint
ide_shortcut_manager_get_n_items (GListModel *model)
{
  IdeShortcutManager *self = (IdeShortcutManager *)model;
  IdeShortcutManagerPrivate *priv = ide_shortcut_manager_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SHORTCUT_MANAGER (self), 0);

  return priv->themes->len;
}

static GType
ide_shortcut_manager_get_item_type (GListModel *model)
{
  return IDE_TYPE_SHORTCUT_THEME;
}

static gpointer
ide_shortcut_manager_get_item (GListModel *model,
                               guint       position)
{
  IdeShortcutManager *self = (IdeShortcutManager *)model;
  IdeShortcutManagerPrivate *priv = ide_shortcut_manager_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SHORTCUT_MANAGER (self), NULL);
  g_return_val_if_fail (position < priv->themes->len, NULL);

  return g_object_ref (g_ptr_array_index (priv->themes, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = ide_shortcut_manager_get_n_items;
  iface->get_item_type = ide_shortcut_manager_get_item_type;
  iface->get_item = ide_shortcut_manager_get_item;
}

void
ide_shortcut_manager_add_theme (IdeShortcutManager *self,
                                IdeShortcutTheme   *theme)
{
  IdeShortcutManagerPrivate *priv = ide_shortcut_manager_get_instance_private (self);
  guint position;

  g_return_if_fail (IDE_IS_SHORTCUT_MANAGER (self));
  g_return_if_fail (IDE_IS_SHORTCUT_THEME (theme));

  for (guint i = 0; i < priv->themes->len; i++)
    {
      if (g_ptr_array_index (priv->themes, i) == theme)
        {
          g_warning ("%s named %s has already been added",
                     G_OBJECT_TYPE_NAME (theme),
                     ide_shortcut_theme_get_name (theme));
          return;
        }
    }

  position = priv->themes->len;

  g_ptr_array_add (priv->themes, g_object_ref (theme));

  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

void
ide_shortcut_manager_remove_theme (IdeShortcutManager *self,
                                   IdeShortcutTheme   *theme)
{
  IdeShortcutManagerPrivate *priv = ide_shortcut_manager_get_instance_private (self);

  g_return_if_fail (IDE_IS_SHORTCUT_MANAGER (self));
  g_return_if_fail (IDE_IS_SHORTCUT_THEME (theme));

  for (guint i = 0; i < priv->themes->len; i++)
    {
      if (g_ptr_array_index (priv->themes, i) == theme)
        {
          g_ptr_array_remove_index (priv->themes, i);
          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
          break;
        }
    }
}
