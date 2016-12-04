/* ide-shortcut-controller.c
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

#define G_LOG_DOMAIN "ide-shortcut-controller"

#include "ide-debug.h"

#include "shortcuts/ide-shortcut-context.h"
#include "shortcuts/ide-shortcut-controller.h"
#include "shortcuts/ide-shortcut-manager.h"

typedef struct
{
  /*
   * This is the widget for which we are the shortcut controller. There are
   * zero or one shortcut controller for a given widget. These are persistent
   * and dispatch events to the current IdeShortcutContext (which can be
   * changed upon theme changes or shortcuts emitting the ::set-context signal.
   */
  GtkWidget *widget;

  /*
   * This is the current context for the controller. These are collections of
   * shortcuts to signals, actions, etc. The context can be changed in reaction
   * to different events.
   */
  IdeShortcutContext *context;

  /*
   * This is the IdeShortcutContext used for commands attached to the controller.
   * Commands are operations which the user can override and will be activated
   * after the current @context.
   */
  IdeShortcutContext *command_context;

  /*
   * This is an array of Command elements which are used to build the shortcuts
   * window and list of commands that the user can override.
   */
  GArray *commands;

  /*
   * This is a pointer to the root controller for the window. We register with
   * the root controller so that keybindings can be activated even when the
   * focus widget is somewhere else.
   */
  IdeShortcutController *root;

  /*
   * The root controller keeps track of the children controllers in the window.
   * Instead of allocating GList entries, we use an inline GList for the Queue
   * link nodes.
   */
  GQueue descendants;
  GList  descendants_link;

  /*
   * Signal handlers to react to various changes in the system.
   */
  gulong hierarchy_changed_handler;
  gulong widget_destroy_handler;
} IdeShortcutControllerPrivate;

typedef struct
{
  const gchar *id;
  const gchar *group;
  const gchar *title;
  const gchar *subtitle;
} Command;

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_WIDGET,
  N_PROPS
};

enum {
  RESET,
  SET_CONTEXT_NAMED,
  N_SIGNALS
};

struct _IdeShortcutController { GObject object; };
G_DEFINE_TYPE_WITH_PRIVATE (IdeShortcutController, ide_shortcut_controller, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static guint       signals [N_SIGNALS];
static GQuark      root_quark;
static GQuark      controller_quark;

static void ide_shortcut_controller_connect    (IdeShortcutController *self);
static void ide_shortcut_controller_disconnect (IdeShortcutController *self);

static gboolean
ide_shortcut_controller_is_mapped (IdeShortcutController *self)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);
  return priv->widget != NULL && gtk_widget_get_mapped (priv->widget);
}

static void
ide_shortcut_controller_add (IdeShortcutController *self,
                             IdeShortcutController *descendant)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);
  IdeShortcutControllerPrivate *dpriv = ide_shortcut_controller_get_instance_private (descendant);

  g_assert (IDE_IS_SHORTCUT_CONTROLLER (self));
  g_assert (IDE_IS_SHORTCUT_CONTROLLER (descendant));

  g_object_ref (descendant);

  if (ide_shortcut_controller_is_mapped (descendant))
    g_queue_push_head_link (&priv->descendants, &dpriv->descendants_link);
  else
    g_queue_push_tail_link (&priv->descendants, &dpriv->descendants_link);
}

static void
ide_shortcut_controller_remove (IdeShortcutController *self,
                                IdeShortcutController *descendant)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);
  IdeShortcutControllerPrivate *dpriv = ide_shortcut_controller_get_instance_private (descendant);

  g_assert (IDE_IS_SHORTCUT_CONTROLLER (self));
  g_assert (IDE_IS_SHORTCUT_CONTROLLER (descendant));

  g_queue_unlink (&priv->descendants, &dpriv->descendants_link);
}

static void
ide_shortcut_controller_widget_destroy (IdeShortcutController *self,
                                        GtkWidget             *widget)
{
  g_assert (IDE_IS_SHORTCUT_CONTROLLER (self));
  g_assert (GTK_IS_WIDGET (widget));

  ide_shortcut_controller_disconnect (self);
}

static void
ide_shortcut_controller_widget_hierarchy_changed (IdeShortcutController *self,
                                                  GtkWidget             *previous_toplevel,
                                                  GtkWidget             *widget)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);
  GtkWidget *toplevel;

  g_assert (IDE_IS_SHORTCUT_CONTROLLER (self));
  g_assert (!previous_toplevel || GTK_IS_WIDGET (previous_toplevel));
  g_assert (GTK_IS_WIDGET (widget));

  g_object_ref (self);

  /*
   * Here we register our controller with the toplevel controller. If that
   * widget doesn't yet have a placeholder toplevel controller, then we
   * create that and attach to it.
   *
   * The toplevel controller is used to dispatch events from the window
   * to any controller that could be activating for the window.
   */

  if (priv->root != NULL)
    {
      ide_shortcut_controller_remove (priv->root, self);
      g_clear_object (&priv->root);
    }

  toplevel = gtk_widget_get_toplevel (widget);

  if (toplevel != widget)
    {
      priv->root = g_object_get_qdata (G_OBJECT (toplevel), root_quark);
      if (priv->root == NULL)
        priv->root = ide_shortcut_controller_new (toplevel);
      ide_shortcut_controller_add (priv->root, self);
    }

  g_object_unref (self);
}

static void
ide_shortcut_controller_disconnect (IdeShortcutController *self)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);

  g_assert (IDE_IS_SHORTCUT_CONTROLLER (self));
  g_assert (GTK_IS_WIDGET (priv->widget));

  g_signal_handler_disconnect (priv->widget, priv->widget_destroy_handler);
  priv->widget_destroy_handler = 0;

  g_signal_handler_disconnect (priv->widget, priv->hierarchy_changed_handler);
  priv->hierarchy_changed_handler = 0;
}

static void
ide_shortcut_controller_connect (IdeShortcutController *self)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);

  g_assert (IDE_IS_SHORTCUT_CONTROLLER (self));
  g_assert (GTK_IS_WIDGET (priv->widget));

  priv->widget_destroy_handler =
    g_signal_connect_swapped (priv->widget,
                              "destroy",
                              G_CALLBACK (ide_shortcut_controller_widget_destroy),
                              self);

  priv->hierarchy_changed_handler =
    g_signal_connect_swapped (priv->widget,
                              "hierarchy-changed",
                              G_CALLBACK (ide_shortcut_controller_widget_hierarchy_changed),
                              self);

  ide_shortcut_controller_widget_hierarchy_changed (self, NULL, priv->widget);
}

static void
ide_shortcut_controller_set_widget (IdeShortcutController *self,
                                    GtkWidget             *widget)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);

  g_assert (IDE_IS_SHORTCUT_CONTROLLER (self));
  g_assert (GTK_IS_WIDGET (widget));

  if (widget != priv->widget)
    {
      if (priv->widget != NULL)
        {
          ide_shortcut_controller_disconnect (self);
          g_object_remove_weak_pointer (G_OBJECT (priv->widget), (gpointer *)&priv->widget);
          priv->widget = NULL;
        }

      if (widget != NULL && widget != priv->widget)
        {
          priv->widget = widget;
          g_object_add_weak_pointer (G_OBJECT (priv->widget), (gpointer *)&priv->widget);
          ide_shortcut_controller_connect (self);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WIDGET]);
    }
}

static void
ide_shortcut_controller_emit_reset (IdeShortcutController *self)
{
  g_assert (IDE_IS_SHORTCUT_CONTROLLER (self));

  g_signal_emit (self, signals[RESET], 0);
}

void
ide_shortcut_controller_set_context (IdeShortcutController *self,
                                     IdeShortcutContext    *context)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);

  g_return_if_fail (IDE_IS_SHORTCUT_CONTROLLER (self));
  g_return_if_fail (!context || IDE_IS_SHORTCUT_CONTEXT (context));

  if (g_set_object (&priv->context, context))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTEXT]);
      ide_shortcut_controller_emit_reset (self);
    }
}

static void
ide_shortcut_controller_real_set_context_named (IdeShortcutController *self,
                                                const gchar           *name)
{
  g_autoptr(IdeShortcutContext) context = NULL;
  IdeShortcutManager *manager;
  IdeShortcutTheme *theme;

  g_return_if_fail (IDE_IS_SHORTCUT_CONTROLLER (self));
  g_return_if_fail (name != NULL);

  manager = ide_shortcut_manager_get_default ();
  theme = ide_shortcut_manager_get_theme (manager);
  context = ide_shortcut_theme_find_context_by_name (theme, name);

  ide_shortcut_controller_set_context (self, context);
}

static void
ide_shortcut_controller_finalize (GObject *object)
{
  IdeShortcutController *self = (IdeShortcutController *)object;
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);

  if (priv->widget != NULL)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->widget), (gpointer *)&priv->widget);
      priv->widget = NULL;
    }

  g_clear_pointer (&priv->commands, g_array_unref);

  g_clear_object (&priv->command_context);
  g_clear_object (&priv->context);
  g_clear_object (&priv->root);

  while (priv->descendants.length > 0)
    g_queue_unlink (&priv->descendants, priv->descendants.head);

  G_OBJECT_CLASS (ide_shortcut_controller_parent_class)->finalize (object);
}

static void
ide_shortcut_controller_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeShortcutController *self = (IdeShortcutController *)object;
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, priv->context);
      break;

    case PROP_WIDGET:
      g_value_set_object (value, priv->widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_shortcut_controller_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeShortcutController *self = (IdeShortcutController *)object;

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_shortcut_controller_set_context (self, g_value_get_object (value));
      break;

    case PROP_WIDGET:
      ide_shortcut_controller_set_widget (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_shortcut_controller_class_init (IdeShortcutControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_shortcut_controller_finalize;
  object_class->get_property = ide_shortcut_controller_get_property;
  object_class->set_property = ide_shortcut_controller_set_property;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The current context of the controller",
                         IDE_TYPE_SHORTCUT_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_WIDGET] =
    g_param_spec_object ("widget",
                         "Widget",
                         "The widget for which the controller attached",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeShortcutController::reset:
   *
   * This signal is emitted when the shortcut controller is requesting
   * the widget to reset any state it may have regarding the shortcut
   * controller. Such an example might be a modal system that lives
   * outside the controller whose state should be cleared in response
   * to the controller changing modes.
   */
  signals [RESET] =
    g_signal_new_class_handler ("reset",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                NULL, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * IdeShortcutController::set-context-named:
   * @self: An #IdeShortcutController
   * @name: The name of the context
   *
   * This changes the current context on the #IdeShortcutController to be the
   * context matching @name. This is found by looking up the context by name
   * in the active #IdeShortcutTheme.
   */
  signals [SET_CONTEXT_NAMED] =
    g_signal_new_class_handler ("set-context",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_shortcut_controller_real_set_context_named),
                                NULL, NULL, NULL,
                                G_TYPE_NONE, 1, G_TYPE_STRING);

  controller_quark = g_quark_from_static_string ("IDE_SHORTCUT_CONTROLLER");
  root_quark = g_quark_from_static_string ("IDE_SHORTCUT_CONTROLLER_ROOT");
}

static void
ide_shortcut_controller_init (IdeShortcutController *self)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);

  g_queue_init (&priv->descendants);

  priv->descendants_link.data = self;
}

IdeShortcutController *
ide_shortcut_controller_new (GtkWidget *widget)
{
  IdeShortcutController *ret;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (NULL != (ret = g_object_get_qdata (G_OBJECT (widget), controller_quark)))
    return g_object_ref (ret);

  ret = g_object_new (IDE_TYPE_SHORTCUT_CONTROLLER,
                      "widget", widget,
                      NULL);

  g_object_set_qdata_full (G_OBJECT (widget),
                           controller_quark,
                           g_object_ref (ret),
                           g_object_unref);

  return ret;
}

/**
 * ide_shortcut_controller_find:
 *
 * Finds the registered #IdeShortcutController for a widget.
 *
 * Returns: (not nullable) (transfer none): An #IdeShortcutController or %NULL.
 */
IdeShortcutController *
ide_shortcut_controller_find (GtkWidget *widget)
{
  IdeShortcutController *controller;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  controller = g_object_get_qdata (G_OBJECT (widget), controller_quark);

  if (controller == NULL)
    {
      /* We want to pass a borrowed reference */
      g_object_unref (ide_shortcut_controller_new (widget));
      controller = g_object_get_qdata (G_OBJECT (widget), controller_quark);
    }

  return controller;
}

/**
 * ide_shortcut_controller_get_context:
 * @self: An #IdeShortcutController
 *
 * This function gets the #IdeShortcutController:context property, which
 * is the current context to dispatch events to. An #IdeShortcutContext
 * is a group of keybindings that may be activated in response to a
 * single or series of #GdkEventKey.
 *
 * Returns: (transfer none) (nullable): An #IdeShortcutContext or %NULL.
 */
IdeShortcutContext *
ide_shortcut_controller_get_context (IdeShortcutController *self)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SHORTCUT_CONTROLLER (self), NULL);

  if (priv->widget == NULL)
    return NULL;

  if (priv->context == NULL)
    {
      IdeShortcutManager *manager;
      IdeShortcutTheme *theme;

      manager = ide_shortcut_manager_get_default ();
      theme = ide_shortcut_manager_get_theme (manager);

      priv->context = ide_shortcut_theme_find_default_context (theme, priv->widget);
    }

  return priv->context;
}

/**
 * ide_shortcut_controller_handle_event:
 * @self: An #IdeShortcutController
 * @event: A #GdkEventKey
 *
 * This function uses @event to determine if the current context has a shortcut
 * registered matching the event. If so, the shortcut will be dispatched and
 * %TRUE is returned.
 *
 * Otherwise, %FALSE is returned.
 *
 * Returns: %TRUE if @event has been handled, otherwise %FALSE.
 */
gboolean
ide_shortcut_controller_handle_event (IdeShortcutController *self,
                                      const GdkEventKey     *event)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);
  IdeShortcutContext *context;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SHORTCUT_CONTROLLER (self), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (priv->widget == NULL ||
      !gtk_widget_get_visible (priv->widget) ||
      !gtk_widget_is_sensitive (priv->widget))
    IDE_RETURN (GDK_EVENT_STOP);

  context = ide_shortcut_controller_get_context (self);

  if (context != NULL)
    {
      if (ide_shortcut_context_activate (context, priv->widget, event))
        IDE_RETURN (GDK_EVENT_STOP);
    }

  if (priv->command_context != NULL)
    {
      if (ide_shortcut_context_activate (priv->command_context, priv->widget, event))
        IDE_RETURN (GDK_EVENT_STOP);
    }

  for (GList *iter = priv->descendants.head; iter != NULL; iter = iter->next)
    {
      IdeShortcutController *descendant = iter->data;

      if (ide_shortcut_controller_handle_event (descendant, event))
        IDE_RETURN (GDK_EVENT_STOP);
    }

  IDE_RETURN (GDK_EVENT_PROPAGATE);
}

static IdeShortcutContext *
ide_shortcut_controller_get_command_context (IdeShortcutController *self)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);

  g_assert (IDE_IS_SHORTCUT_CONTROLLER (self));

  if (priv->command_context == NULL)
    priv->command_context = g_object_new (IDE_TYPE_SHORTCUT_CONTEXT,
                                          "use-binding-set", FALSE,
                                          NULL);

  return priv->command_context;
}

/**
 * ide_shortcut_controller_add_command_signal: (skip)
 * @self: An #IdeShortcutController
 * @command_id: the command-id such as "org.gnome.builder.plugins.foo.bar"
 * @default_accel: the accelerator for the default key theme
 * @group: the group to place the shortcut within the shortcuts overview
 * @title: the title for the shortcut
 * @subtitle: (nullable): an optional subtitle for the command
 * @signal_name: the name of the signal to activate on the controllers widget
 * @n_args: the number of argument pairs
 *
 * This adds a command to the controller which will activate the signal @signal_name
 * on the attached #GtkWidget. Use @n_args followed by pairs of (#GType, value) to
 * specify the arguments for the signal. This is similar to
 * gtk_binding_entry_add_signal().
 *
 * By registering a command on a controller directly, the shortcuts overview can
 * display the shortcut in the shortcuts window as well as allow the user to
 * override the accelerator. Where as the user cannot override operations found
 * directly in #IdeShortcutContext's as provided by themes.
 */
void
ide_shortcut_controller_add_command_signal (IdeShortcutController *self,
                                            const gchar           *command_id,
                                            const gchar           *default_accel,
                                            const gchar           *group,
                                            const gchar           *title,
                                            const gchar           *subtitle,
                                            const gchar           *signal_name,
                                            guint                  n_args,
                                            ...)
{
  IdeShortcutControllerPrivate *priv = ide_shortcut_controller_get_instance_private (self);
  IdeShortcutContext *command_context;
  Command command = { 0 };
  va_list args;

  g_return_if_fail (IDE_IS_SHORTCUT_CONTROLLER (self));
  g_return_if_fail (command_id != NULL);
  g_return_if_fail (group != NULL);
  g_return_if_fail (title != NULL);
  g_return_if_fail (signal_name != NULL);

  command.id = g_intern_string (command_id);
  command.group = g_intern_string (group);
  command.title = g_intern_string (title);
  command.subtitle = g_intern_string (subtitle);

  if (priv->commands == NULL)
    priv->commands = g_array_new (FALSE, FALSE, sizeof (Command));

  g_array_append_val (priv->commands, command);

  command_context = ide_shortcut_controller_get_command_context (self);

  va_start (args, n_args);
  ide_shortcut_context_add_signal_va_list (command_context,
                                           default_accel,
                                           signal_name,
                                           n_args,
                                           args);
  va_end (args);
}
