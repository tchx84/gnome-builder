/* ide-refactory-command.h
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

#ifndef IDE_REFACTORY_COMMAND_H
#define IDE_REFACTORY_COMMAND_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_REFACTORY_COMMAND (ide_refactory_command_get_type())

G_DECLARE_INTERFACE (IdeRefactoryCommand, ide_refactory_command, IDE, REFACTORY_COMMAND, IdeObject)

struct _IdeRefactoryCommandInterface
{
  GTypeInterface parent;

  void     (*run_async)  (IdeRefactoryCommand   *self,
                          GCancellable          *cancellable,
                          GAsyncReadyCallback    callback,
                          gpointer               user_data);
  gboolean (*run_finish) (IdeRefactoryCommand   *self,
                          GAsyncResult          *result,
                          GError               **error);
};

void     ide_refactory_command_run_async  (IdeRefactoryCommand  *self,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data);
gboolean ide_refactory_command_run_finish (IdeRefactoryCommand  *self,
                                           GAsyncResult         *result,
                                           GError              **error);

G_END_DECLS

#endif /* IDE_REFACTORY_COMMAND_H */
