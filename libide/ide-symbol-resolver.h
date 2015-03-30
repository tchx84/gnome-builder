/* ide-symbol-resolver.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_SYMBOL_RESOLVER_H
#define IDE_SYMBOL_RESOLVER_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SYMBOL_RESOLVER (ide_symbol_resolver_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeSymbolResolver, ide_symbol_resolver, IDE, SYMBOL_RESOLVER, IdeObject)

struct _IdeSymbolResolverClass
{
  IdeObjectClass parent;

  void       (*lookup_symbol_async)  (IdeSymbolResolver    *self,
                                      IdeSourceLocation    *location,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data);
  IdeSymbol *(*lookup_symbol_finish) (IdeSymbolResolver    *self,
                                      GAsyncResult         *result,
                                      GError              **error);
  void       (*get_symbols_async)    (IdeSymbolResolver    *self,
                                      IdeFile              *file,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data);
  GPtrArray *(*get_symbols_finish)   (IdeSymbolResolver    *self,
                                      GAsyncResult         *result,
                                      GError              **error);
};

void       ide_symbol_resolver_lookup_symbol_async  (IdeSymbolResolver    *self,
                                                     IdeSourceLocation    *location,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IdeSymbol *ide_symbol_resolver_lookup_symbol_finish (IdeSymbolResolver    *self,
                                                     GAsyncResult         *result,
                                                     GError              **error);
void       ide_symbol_resolver_get_symbols_async    (IdeSymbolResolver    *self,
                                                     IdeFile              *file,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
GPtrArray *ide_symbol_resolver_get_symbols_finish   (IdeSymbolResolver    *self,
                                                     GAsyncResult         *result,
                                                     GError              **error);

G_END_DECLS

#endif /* IDE_SYMBOL_RESOLVER_H */
