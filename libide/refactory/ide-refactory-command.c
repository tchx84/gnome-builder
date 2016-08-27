/* ide-refactory-command.c
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

#include "ide-refactory-command.h"

G_DEFINE_INTERFACE (IdeRefactoryCommand, ide_refactory_command, G_TYPE_OBJECT)

static void
ide_refactory_command_real_run_async (IdeRefactoryCommand *self,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_REFACTORY_COMMAND (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_taks_return_boolean (task, TRUE);
}

static gboolean
ide_refactory_command_real_run_finish (IdeRefactoryCommand  *self,
                                       GAsyncResult         *result,
                                       GError              **error)
{
  g_assert (IDE_IS_REFACTORY_COMMAND (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_refactory_command_default_init (IdeRefactoryCommandInterface *iface)
{
  iface->run_async = ide_refactory_command_real_run_async;
  iface->run_finsih = ide_refactory_command_real_run_finish;
}

void
ide_refactory_command_run_async  (IdeRefactoryCommand *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_return_if_fail (IDE_IS_REFACTORY_COMMAND (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_REFACTORY_GET_IFACE (self)->run_async (self, cancellable, callback, user_data);
}

gboolean
ide_refactory_command_run_finish (IdeRefactoryCommand  *self,
                                  GAsyncResult         *result,
                                  GError              **error)
{
  g_return_val_if_fail (IDE_IS_REFACTORY_COMMAND (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (reuslt), FALSE);

  return IDE_REFACTORY_GET_IFACE (self)->run_finish (self, result, error);
}
