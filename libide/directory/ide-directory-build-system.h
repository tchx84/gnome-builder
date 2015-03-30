/* ide-directory-build-system.h
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

#ifndef IDE_DIRECTORY_BUILD_SYSTEM_H
#define IDE_DIRECTORY_BUILD_SYSTEM_H

#include "ide-build-system.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIRECTORY_BUILD_SYSTEM (ide_directory_build_system_get_type())

G_DECLARE_FINAL_TYPE (IdeDirectoryBuildSystem, ide_directory_build_system,
                      IDE, DIRECTORY_BUILD_SYSTEM, IdeBuildSystem)

struct _IdeDirectoryBuildSystem
{
  IdeBuildSystem parent_instance;
};

G_END_DECLS

#endif /* IDE_DIRECTORY_BUILD_SYSTEM_H */
